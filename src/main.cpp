/**
    @file
    @brief The main IceTea logic

    This file contains and holds upon all the functions that translate targets,
    actions, externals and rules into some logic. This is done by processing
    the input using a single thread, and executing the tasks over a multiple of
    threads (depending on the given hardware).

    Only minor additions are made to the script environment, that contain simple
    wrapper functions.
*/

#include <iostream>
#include <fstream>
#include <list>
#include <map>
#include <vector>
#include <stdlib.h>

// Threading
#include "tinythread.h"
#include "fast_mutex.h"
#include "threading.h" /* Threadpool */

// Scripting
#include "objectscript.h"
#include "os-value.h"
#include "os-icetea.h"
#include "os-detector.h"
#include "os-exec.h"
#include "os-pfs.h"
#include "os-std.h"
#include "os-stacker.h"

// FS access
#include "file_system.hpp"
#include "wildcard.hpp"

// Hashing
#include "picosha2.h"

// Console
#include "lineprinter.h"

// Misc
#include "predef.h"
#include "cli.h"
#include "util.h"
#include "stlplus_version.hpp"
#include "filecache.hpp"
#include "getMemorySize.h"

// Because I have to
#ifdef __APPLE__
#include <execinfo.h>
#include <signal.h>
#include "stacktrace.h"
void handler(int sig) {
  // print out all the frames to stderr
  cerr << Backtrace() << endl;
}
#endif

// Namespaces!
using namespace std;
using namespace tthread;
using namespace ObjectScript;
using namespace stlplus;
using namespace picosha2;

/// @section IceTea globals

/// An ObjectScript object which holds all registered targets and externals.
Value::Object* targets;

/// An ObjectScript object which holds all registered rules.
Value::Object* rules;

/// An ObjectScript object which holds all registered actions.
Value::Object* actions;

/// What type of execution scheme is to be used within a task
enum TaskType {
    SCRIPT, ///< The task is to be run as a script.
    COMMAND ///< The task is to be run using the given commands.
};

typedef tthread::lock_guard<tthread::mutex> Locker; ///< Typedef for our locker class.

/**
    @brief An actual task that is executed within @ref Run

    This structure is a (mostly) C++ structure, containing information about
    a target and how it has to be built - i.e., commands or script statements.
*/
class Task {
    ///@{
private:
    bool _isMaster; ///< If true, then the output is to be displayed in the progress.
    bool _isBuilt; ///< If the target was already built or not.
    bool _shouldUpdate; ///< Should be set on targets whose build must be forced.
    bool _isProcessing; ///< A "target lock"
    tthread::mutex m; ///< Class-local mutex to control access to the task.
    tthread::mutex tm; ///< Task mutex
    tthread::thread::id myID; ///< thread id

public:
    string ruleName; ///< The rule's name.
    string targetName; ///< The target's name.
    string ruleDisplay; ///< How the rule should be displayed.
    list<string> input; ///< A vector containing the input files for the task.
    list<string> dependencies; ///< List of files that this target depends on.
    string output; ///< The output file.

    TaskType type; ///< The type in which this task is ran.
    // [ cmd1, cmd2, ... ]
    vector<string> commands; ///< If `type==COMMAND`, then these are ran.
    // Rule.build: function(input, output, target)
    // The function is either called with only one input - or an input array. Thus access to the
    // target is given for settings - such as include dirs and alike.

    Value::Object* target; ///< The ObjectScript representation of the target.

    // Iteration?
    Task* parent; ///< Parent task
    Task* child; ///< Child task


    /**
        @brief Brief initialization
    */
    Task() {
        _isBuilt = false;
        _isMaster = false;
        _isProcessing = false;
    }

    /**
        @brief Destructs and cleans the task.
    */
    ~Task() {
        delete target;
    }

    /// Getter and setters
    bool isBuilt() {
        Locker g(m);
        return _isBuilt;
    }
    bool isBuilt(bool b) {
        Locker g(m);
        _isBuilt = b;
        return b;
    }
    bool shouldUpdate() {
        Locker g(m);
        return _shouldUpdate;
    }
    bool shouldUpdate(bool b) {
        Locker g(m);
        _shouldUpdate = b;
        return b;
    }
    bool isMaster() {
        Locker g(m);
        return _isMaster;
    }
    bool isMaster(bool b) {
        Locker g(m);
        _isMaster = b;
        return b;
    }
    bool isProcessing() {
        Locker g(m);
        return _isProcessing;
    }
    bool isProcessing(bool b) {
        Locker g(m);
        _isProcessing = b;
        return b;
    }
    void lock() { tm.lock(); }
    void unlock() { tm.unlock(); }
    bool try_lock() { return tm.try_lock(); }
    ///@}
};
typedef WorkQueue<Task*, OS*> TaskQueue; ///< Typedef to instantiate the threadpool.
typedef list<string> StrList;

OS*        os; ///< Global ObjectScript instance.
CLI*       cli; ///< Global Command Line Interface instance.
Filecache* fc; ///< Global Filecache instance.
TaskQueue* tasks; ///< Threadpool containing task runners and queue.
string     runScheme = "build"; ///< The build scheme to go for. Build or Clean.
string     outputDir; ///< The folder where output should be stored at.
bool       runTasks = true; ///< A boolean that defines if tasks are being ran or not.
// In order to call OS savely, we use this.
static tthread::mutex OSMutex; ///< A mutex to lock and unlock to gain access to ObjectScript.
static tthread::mutex ConfigMutex; ///< A mutex used to halt execution of @ref Run
// Printing
stringstream tout; ///< Output for the @ref LinePrinter
LinePrinter print(&tout); ///< LinePrinter instance

// Tool functions
int itCleanup();

/**
    @brief Checks if a rule exists.

    @param ruleName The name of the rule to look for.
    @returns A bool wether the rule was found or not.
*/
bool ruleExists(Value::String* ruleName) {
    os->pushValueById(ruleName->valueID);
    os->pushValueById(rules->valueID);
    if(os->in(/* string in object */)) {
        os->pop(2);
        return true;
    } else {
        os->pop(2);
        return false;
    }
}

/**
    @overload bool ruleExists(Value::String* ruleName)
*/
bool ruleExists(const string ruleName) {
    os->pushString(ruleName.c_str());
    os->pushValueById(rules->valueID);
    if(os->in(/* string in object */)) {
        os->pop(2);
        return true;
    } else {
        os->pop(2);
        return false;
    }
    return false;
}

/**
    @brief Checks if an action exists.

    This is used to see if a specific action exists. A script may utilize
    this to figure out if it has to create a certain action or not.

    @param name The name of the action to look for.
    @returns A bool wether the action exists or not.
*/
bool actionExists(const string name) {
    os->pushString(name.c_str());
    os->pushValueById(actions->valueID);
    if(os->in(/* string in object */)) {
        os->pop(2);
        return true;
    } else {
        os->pop(2);
        return false;
    }
}

/**
    @brief Look for the existence of a target

    This function is especially used during the @ref Transformer step. It looks
    up the target and resolves dependencies. This function helps to check for
    the availability of said target.

    @param name The target to look for.
    @returns A bool wether it exits or not.
*/
bool targetExists(const string name) {
    os->pushString(name.c_str());
    os->pushValueById(targets->valueID);
    if(os->in(/* string in object */)) {
        os->pop(2);
        return true;
    } else {
        os->pop(2);
        return false;
    }
}

/**
    @brief Estimate the output tht a rule may produce

    During the task creation step, this is ued to compare input and output
    to the target. It also can be used as a lean way to resolve a dependency.

    @param rule The rule to be used.
    @param target The target name to be used. The target does NOT need to exist!
    @param file By default, this is empty. Part of output generation depends on the file.
    @returns A string that represents the possible output.
*/
string estimateRuleOutput(string rule, string target, string file="") {
    os->pushString(rule.c_str());
    os->pushValueById(rules->valueID);
    if(!os->in()) {
        cerr << "[IceTea]: Rule '" << rule << "' does not exist.";
        return "";
    }
    os->pushString(rule.c_str());
    os->getProperty();
    Value::Object myRule(os, os->getAbsoluteOffs(-1));
    Value::Object* outputs = (Value::Object*)myRule["output"];
    Value::String* o_myExpected = (Value::String*)(*outputs)["expected"];
    string myExpected = (*o_myExpected);

    // Replace stuff...
    string basename = filename_part(file);
    string ext = basename_part(file);
    ReplaceStringInPlace(myExpected, "%t", target);
    ReplaceStringInPlace(myExpected, "%b", basename);
    ReplaceStringInPlace(myExpected, "%e", ext);
    ReplaceStringInPlace(myExpected, "%f", file);
    ReplaceStringInPlace(myExpected, "%o", outputDir);

    // Clean up the heap
    delete outputs;
    delete o_myExpected;

    return myExpected;
}

/**
    @brief A default method to be used when cleaning targets.

    If we are in whipe mode, this is given to all those rules that dont have
    a default `clean()` method.

    It is very similar to the `build()` method.

    @ref TargetBuild
*/
OS_FUNC(os_target_clean) {
    Task* t = (Task*)userData;
    //if(is_present(output)) {
        tout << "Deleting: " << t->output << endl;
        print();
        file_delete(t->output);
        folder_delete(folder_part(t->output));
    //}
    return 0;
}

/**
    @brief Create the actual tasks

    This function contains a LOT of logic and represents the heartpiece of the
    entire project. It looks up the rules and compares a file and other information
    against its results. The finalized task is added to the @ref TaskQueue

    This function also calls itself recursively and passes input trough referenced
    objects.

    @param rule The rule to be used in the current iteration.
    @param file The file to be used for the current iteration.
    @param target The target to iterate on.
    @param[out] input The input to be returned to the parent call
    @param[in,out] parent The @ref Task object to be passed as a parent.
    @param[out] success A bool that will be set to true if the iteration was indeed a success. False if otherwise.
    @param finalRule a boolean that is used to tell the __first__ iteration about it.
*/
void __makeTasks(
    string rule, string file, string target,
    string& input, Task*& parent, bool& success, bool finalRule=false
) {
    if(!ruleExists(rule)) {
        cerr << "[IceTea]: Rule \"" << rule << "\" does not exist." << endl;
        return;
    }

    Value::Object* targetObj = (Value::Object*)(*targets)[target];
    Value::Object* ruleObj = (Value::Object*)(*rules)[rule];

    /*
        In most cases, the input file won't match directly.
        However, sometimes it just so happens to be right.
        Example: minifying lots of .css file into .min.css
        But for native code, that wont be the case.
        Example: file=myfile.cpp and the rule produces mytarget.exe
        Therefore, we do a check if we currently are on the right target or not.
    */
    Value::Array* accepts = (Value::Array*)(*ruleObj)["accepts"];
    bool isBaseTarget=false; // Base target = the input file IS the right one. No search needed.
    for(int i=0; i<accepts->length(); i++) {
        Value::String* acceptPattern = (Value::String*)(*accepts)[i];
        if(wildcard( (*acceptPattern), file )) {
            isBaseTarget = true;
            break;
        }
        delete acceptPattern;
    }

    // In any case, we want to create the expected value. Its easy.
    string myExpected = estimateRuleOutput(rule, target, file);

    static map<string, bool> rulesChecked;
    #define RUN_INIT \
        if(rulesChecked.find(rule) == rulesChecked.end()) { \
            os->pushValueById(ruleObj->valueID); \
            os->pushString("prepare"); \
            rulesChecked[rule]=true; \
            if(os->in(-1, -2)) { \
                os->getProperty(-2); \
                if(os->isFunction()) { \
                    os->pushValueById(ruleObj->valueID); \
                    os->callFT(0,1); \
                    if(os->isType(OS_VALUE_TYPE_BOOL) && os->toBool() == false) { \
                        success = false; \
                        return; \
                    } \
                } \
            } \
        }

    if(isBaseTarget) {
        RUN_INIT
        if(finalRule) {
            // The base target is ALSO the only...all we gotta do is return accordingly.
            input = file;
        } else {
            // Do not even go much further, add the target accordingly.
            Value::String* t_rd = (Value::String*)(*ruleObj)["display"];
            string display = (*t_rd);
            delete t_rd;
            Task* t = new Task;
            t->ruleName = rule;
            t->targetName = target;
            t->ruleDisplay = display;
            t->input.push_back(file);
            t->output = myExpected;

            // Find out what type the rule is. A function, or an array of commands?
            os->pushValueById(ruleObj->valueID);
            os->getProperty(-1, runScheme.c_str());
            if(os->getTypeStr() == "array") {
                t->type=COMMAND;
                int objlen = os->getLen();
                for(int k=0; k<objlen; k++) {
                    os->pushNumber(k);
                    os->getProperty();
                    t->commands.push_back( os->toString().toChar() );
                }
            } else if(os->getTypeStr() == "function") {
                t->type=SCRIPT;
            } else {
                if(cli->check("-w")) {
                    // Whipe mode. Replace the emptyness right there with an actual thing.
                    os->pushValueById(ruleObj->valueID);
                    os->pushCFunction(os_target_clean, (void*)t);
                    os->setProperty(-2, runScheme.c_str());
                } else {
                    cerr << "[IceTea]: Rule '"<< rule << "' must have it's build property defined as function or array! "
                         << "'" << os->getTypeStr() << "' was given instead." << endl;
                }
            }

            t->target = (Value::Object*)(*targets)[target];

            // Relate it
            t->child = NULL;
            t->parent = parent;
            parent->child = t;

            print();
            tasks->add(t);

            // We send the output of this task back to the parent call.
            // That is the new input.
            input = myExpected;
        }
    } else {
        /*
            This is much more complicated...
            We have the scenario, that the current file doe snot fit.
            So we have to search for the actual rule.
            But in relation to that, the rule we find must also be able to
            output the kind of file that we want. Therefore, we need:

            - The "final" rule's output pattern
            - The actual rule's output pattern
        */
        os->pushValueById(rules->valueID);
        os->getProperty(-1, "keys");
        Value::Array* ruleNames = new Value::Array(os);
        for(int r=0; r<ruleNames->length(); r++) {
            Value::String* ruleName = (Value::String*)(*ruleNames)[r];
            Value::Object* currRule = (Value::Object*)(*rules)[(*ruleName)];

            /*
                We are going to find out, if the rule we are currently onto
                is capable of delivering the file we want.

                    if( the current rule's output pattern IS within the given rule's accept list ) {
                        Then continue.
                    }
            */
            bool outputMatchesAccepted=false;
            Value::Object* currOutputObj = (Value::Object*)(*currRule)["output"];
            Value::String* currOutputPat = (Value::String*)(*currOutputObj)["pattern"];
            for(int e=0; e<accepts->length(); e++) {
                // Entry in the given rule's accept array
                Value::String* currAccepts = (Value::String*)(*accepts)[e];
                if( (*currOutputPat) == (*currAccepts) ) {
                    outputMatchesAccepted = true;
                }
                delete currAccepts;
                if(outputMatchesAccepted) break;
            }
            //delete currOutputObj;
            //delete currOutputPat;

            /*
                So our output pattern might match an input pattern.
                That means we found a "child rule". An example would be:

                    .exe
                        -> .o-stripped (Child)
                            -> .o (Base)

                Or more complicated:

                    .framework (Final)
                        -> .dylib (Child)
                            -> .o-stripped (Child)
                                -> .o (Base)

                Or even...

                    .framework (Final)
                        -> .dylib (Child)
                            -> .a (final of another target)
                                -> .o-stripped (Child)
                                    -> .o (Base)
                            -> .o-stripped (Child)
                                -> .o (Base)

                The final rule is constructed from the search process (Transformer).
                The base rule then occurs, when the input file exactly matches the input pattern,
                given in the "accepts" object property.
                A child rule is, if the file doesnt match the given rule and if its not the final rule.
                At this state, we have to recursively call ourselves.
            */

            if(outputMatchesAccepted) {
                RUN_INIT
                /*
                    Again, the output of the current rule matches our wanted input.
                    Now we have to verify if we found a base rule, or a child rule.
                    That is done by comparing the current file to the accepted pattern
                    of the current rule. If it matches, it is a base rule.
                    In this case, we call ourself with the original file and found rule.
                    Otherwise, we must peer one level deeper and estimate an input.
                    Example: If we were given the executable rule (.exe as output)
                    and have a .cpp file as input, then we would find, that
                    the executable rule wants '.o-stripped' as its input.
                    The strip rule provides the actual output for that, but requires
                    a '.o' file for its input. So we search for a rule that outputs '.o'
                    and recall ourselves with that:

                        File: myfile.cpp
                        Rule: strip

                    But, strip requires '.o''s! What will happen is, that we first
                    re-execute ourself to find a rule that outputs '.o'. In this case,
                    we will actually find a base-rule, and the very above, first
                    check will pass, and we just create a task.
                    That will send back a .o file that we can then add as input
                    to the strip rule.

                    When the strip rule has added it's stuff, it gives back a '.o-stripped'
                    file, and since .exe doesnt need to make its ownt ask, we return that
                    to the main search function.
                */

                // Let's start by finding out if what we found is a base rule.
                bool currentIsBase=false;
                Value::Array* currAccepts = (Value::Array*)(*currRule)["accepts"];
                for(int n=0; n<currAccepts->length(); n++) {
                    Value::String* accPattern = (Value::String*)(*currAccepts)[n];
                    if(wildcard((*accPattern), file)) {
                        currentIsBase = true;
                    }
                    delete accPattern;
                    if(currentIsBase) break;
                }

                string theExpected = estimateRuleOutput((*ruleName), target, file);
                Value::String* t_rd = (Value::String*)(*currRule)["display"];
                string display = (*t_rd);
                string _ruleName = (*ruleName);
                delete t_rd;

                if(currentIsBase) {
                    // The file we have, and the rule we found, make up for a base.
                    // In this case, recall ourself. The above, first check will pass
                    // and it will execute a base rule.
                    string newInput;
                    __makeTasks((*ruleName), file, target, newInput, parent, success);
                    //if(newInput.empty()) continue;
                    input = newInput;
                } else {
                    /*
                        The current rule provides an output that we can use, but we dont have
                        a suitable input for it. I.e.:

                            strip accepts: *.o
                                  outputs: *.o-stripped
                            file         : myfile.cpp

                        So what do we do?

                        We re-run ourself with the current file, then create
                        a new task with the "returned" output.
                        Beyond that, we return the expected output, as well.
                    */
                    Task* t = new Task;
                    string newInput;
                    __makeTasks((*ruleName), file, target, newInput, t, success);

                    // If the newInput we got is empty, then we probably should skip this rule.
                    if(newInput.empty()) {
                        continue;
                    }

                    t->ruleName = _ruleName;
                    t->targetName = target;
                    t->ruleDisplay = display;
                    t->output = theExpected;

                    os->pushValueById(currRule->valueID);
                    os->getProperty(-1, runScheme.c_str());
                    if(os->getTypeStr() == "array") {
                        t->type=COMMAND;
                        int objlen = os->getLen();
                        for(int k=0; k<objlen; k++) {
                            os->pushNumber(k);
                            os->getProperty();
                            t->commands.push_back( os->toString().toChar() );
                        }
                    } else if(os->getTypeStr() == "function") {
                        t->type=SCRIPT;
                    } else {
                        if(cli->check("-w")) {
                            // Whipe mode. Replace the emptyness right there with an actual thing.
                            os->pushValueById(ruleObj->valueID);
                            os->pushCFunction(os_target_clean, (void*)t);
                            os->setProperty(-2, runScheme.c_str());
                        } else {
                            cerr << "[IceTea]: Rule '"<< rule << "' must have it's '" << runScheme << "' property defined as function or array! "
                                      << "'" << os->getTypeStr() << "' was given instead." << endl;
                        }
                    }

                    t->target = (Value::Object*)(*targets)[target];
                    parent->child = t;
                    t->parent = parent;
                    t->input.push_back(newInput);
                    tasks->add(t);
                    input = theExpected;
                    return; // Done.
                }
            } else {
                // No matching rule was found.
                //cerr << file << " against " << rule << " resulted in nothing." << endl;
            }

            delete currRule;
            delete ruleName;
        }
        // If we have reached here, NO RULE DID MATCH.
        //cout << file << " seriously didnt match ANYTHING." << endl;
    }
    delete targetObj;
    delete ruleObj;
    delete accepts;
}

/**
    @brief The actual wrapper around @ref __makeTasks

    This function simply returns what the actual @ref __makeTasks was giving back.
    This is also called the 0th iteration. It receives its output as the last instance.
*/
string makeTasks(string rule, string file, string target, Task*& parent, bool& successor) {
    string tmp;
    // The first call ALWAYS results in the actual thing.
    __makeTasks(rule, file, target, tmp, parent, successor, true);
    return tmp;
}

/** @section The task processors
   There are a few of them:
        - Initializer: If it exists, call init() on each target. Ommit this, if --help is used.
            * the init() function can add entries to the help screen. Important for configuration.
            * Check if rules that targets reffer to, exist.
        - Preprocessors: Run configure on each object.
            * configure() has access to the config object, command line params (parsed) and can also abort.
            * After doing so, strech out their inputs if its a wildcard
        - Transformers: Create the list of tasks, which must consist of C++ stuff ONLY.
            * Trace down the rules for each individual file and call prepare().
            * Create a new task with the file and the rule information.
            * Create the output variable, which means creating the base folders needed (out/,...)
            * Determine if its a scripted, or command controlled task.
            * If its a scripted task, save a Value::Method, otherwise a list<string> of commands.
            * Save info to a task list, WorkQueue.
        - Runner: THREADED task execution
            * Grab a task using the WorkQueue interface.
            * Test the file system if we can put the estimated output file.
            * If task relies on multiple files, decide what to do....needs try-and-error'ing here.
            * Process the task accordingly (run commands, request OS call)
*/
/**
    @brief Initializes all targets.
    @returns If the iteration was successful.
    @note This should actually change depending on specified actions...
*/
bool Initializer() {
    os->pushValueById(targets->valueID);
    os->getProperty(-1, "keys"); // triggers the getKeys method.
    Value::Array keys(os);
    for(int i=0; i<keys.length(); i++) {
        Value::String* key = (Value::String*)keys[i];
        Value::Object* val = (Value::Object*)(*targets)[(*key)];
        Value::String* ruleName = (Value::String*)(*val)["_"];
        if(!ruleExists(ruleName)) {
            cerr << "Target \"" << (*key) << "\" refferences to non-existing rule \"" << (*ruleName) << "\"." << endl;
            return false;
        }
        os->pushValueById(val->valueID);
        os->pushString("init");
        if(os->in(-1, -2)) {
            os->popString();
            os->getProperty("init");
            if(os->isFunction()) {
                Value::Method mt(os, val, "init");
                mt(0,0);
            } else {
                cerr << "[IceTea]: The init() method in " << key << "is not a function! Found "
                     << os->getTypeStr().toChar() << " instead.";
            }
        } else {
            /*
            cerr << "IceTea: Error detected. Stack:" << endl
                 << "    -1: " << os->getTypeStr(-1).toChar() << " --> " << os->toString(-1).toChar() << endl
                 << "    -2: " << os->getTypeStr(-2).toChar() << " --> " << os->toString(-2).toChar() << endl
                 << "    -3: " << os->getTypeStr(-3).toChar() << " --> " << os->toString(-3).toChar() << endl
                 << "    -4: " << os->getTypeStr(-4).toChar() << " --> " << os->toString(-4).toChar() << endl
                 << "    -5: " << os->getTypeStr(-5).toChar() << " --> " << os->toString(-5).toChar() << endl
                 << endl;
             */
        }

        // cleanup
        delete key;
        delete val;
        delete ruleName;

        // Cause IceTea to exit
        if(os->isExceptionSet()) {
            cout << "Exception??" << endl;
            os->handleException();
            return false;
        }
    }
    return true;
}

/**
    @brief Configure all targets and resolve dependencies

    This function calls the targets configure method in order to make them
    ready for actual compilation.

    It also looks for an existing `need` property and if it exists, grabs the
    other target's configure methods and runs them first. It then combines
    the `exports` property with the local `settings` property, if it exists.

    @returns If the configuring was successful.
*/
bool Preprocessor() {
    // Used to track configured targets.
    static map<string,bool> cfgd;
    os->pushValueById(targets->valueID);
    os->getProperty(-1, "keys"); // triggers the getKeys method.
    Value::Array keys(os);
    for(int i=0; i<keys.length(); i++) {
        Value::String* key = (Value::String*)keys[i];
        Value::Object* val = (Value::Object*)(*targets)[(*key)];
        if(cfgd.find((*key)) != cfgd.end()) {
            // We have configured this already.
            continue;
        }
        cfgd[(*key)]=true;
        os->pushValueById(val->valueID);
        os->pushString("configure");
        if(os->in(-1, -2)) {
            os->getProperty();
            if(os->isFunction()) {
                if(cli->check("-v")) {
                    cout << "(As self)" << endl;
                }
                os->callTF(0,1);
                if(os->toBool() == false) {
                    cerr << "We have a false!" << endl;
                    // Configure returned false. So we quickly drop out.
                    return false;
                }
                os->pop(2); // mixed, function
            } else {
                //cerr << "[IceTea]: The configure() method in " << key << "is not a function! Found "
                //     << os->getTypeStr().toChar() << " instead.";
            }
        }
        os->pop(); // string

        // Now resolve dependencies, if any
        os->pushString("needs");
        os->getProperty();
        if(!os->isNull()) {
            // That thing actually exists!
            if(os->isArray()) {
                int nlen = os->getLen();
                for(int e=0; e<nlen; e++) {
                    os->pushNumber(e);
                    os->getProperty();
                    string tname = os->popString().toChar();
                    Value::Object* tobj = (Value::Object*)(*targets)[tname];

                    os->pushValueById(tobj->valueID);
                    os->pushString("configure");
                    if(os->in(-1,-2)) {
                        os->getProperty();
                        if(os->isFunction() && cfgd.find(tname) == cfgd.end()) {
                            cfgd[tname]=true;
                            if(cli->check("-v")) {
                                cout << "(As dependency)" << endl;
                            }
                            os->callTF(0,1);
                            if(!os->toBool()) {
                                cout << "Dependency target '" << tname << "' returned false." << endl;
                                return false;
                            }
                            os->pop();
                        }
                        os->pop();
                    }
                    os->pop();

                    os->pushString("exports");
                    os->getProperty();
                    int exoff = os->getAbsoluteOffs(-1);
                    if(os->isObject()) {
                        os->pushValueById(val->valueID);
                        //os->pushString("settings");
                        os->getProperty(-2);
                        /*if(!os->isObject()) {
                            os->newObject();
                            os->pushString("settings");
                            os->setProperty(-3);
                        }*/
                        os->getProperty(-1, "__add"); // Thanks to os-std
                        os->pushStackValue(exoff);
                        os->callTF(1,0);
                        os->pop(3);
                    }
                    os->pop(2);
                }
            }
        }

        // clean up
        delete key;
        delete val;

        // Cause IceTea to exit
        if(os->isExceptionSet()) return false;
    }
    return true;
}

/**
    @brief Turn targets into a series of tasks

    This function is mainly responsible for turning targets into tasks. It
    takes note of the specified actions, or uses `all` as a default action.

    `all` __must__ exist. It can not be omitted. Only if the user always
    specifies a different task, thent his will not be required.

    @returns If transformation was successful
    @see Preprocess
    @see Initializer
*/
bool Transformer() {
    // And now lets get serious.
    // We only must push "safe" targets into the queue...
    // There needs to be a mechanism to determine the actual target list.

    // We have to lock OS for this procedure.
    Locker g(OSMutex);

    // List of targets to build.
    vector<string> targetList;

    // Now we have to know the action and see if it exists.
    vector<string> toBuild = cli->getStrayArgs();
    if(toBuild.empty()) {
        // The default target.
        toBuild.push_back("all");
    }
    vector<string>::iterator it=toBuild.begin();
    bool result = true;
    for(; it!=toBuild.end(); ++it) {
        if(!actionExists(*it)) {
            cerr << "Action '"<< *it <<"' does not exist!" << endl;
            return false;
        } else {
            if(cli->check("-w"))
                cout << "* Cleaning: ";
            else
                cout << "* Running: ";
            cout << *it << endl;
            os->pushValueById(actions->valueID);
            os->pushString(it->c_str());
            os->getProperty(-2);
            if(os->isFunction()) {
                // A simple global to avoid the bottom and unneeded task info.
                runTasks = false;
                // We need to give users the IceTea.build function... FIXME
                os->pushGlobals();
                os->pushString(runScheme.c_str());
                os->callFT(1,0);
                if(os->isNumber()) {
                    result = os->toBool();
                }
                result = true;
            } else {
                runTasks = true;
                // Now we have to get the actual targets to run.
                os->pop(2);
                os->pushValueById(actions->valueID);
                os->pushString(it->c_str());
                os->getProperty(-2);
                os->pushString(runScheme.c_str());
                os->getProperty(-2);
                if(!os->isArray()) {
                    cerr << *it << " does not specify a valid " << runScheme << " attribute."
                         << " It is expected to be an array."
                         << " Found a '" << os->getTypeStr() << "' instead" << endl;
                } else {
                    int objLen = os->getLen();
                    for(int sri=0; sri<objLen; sri++) {
                        // find a matching target and add it.
                        // targetList.push_back()
                        os->pushNumber(sri);
                        os->getProperty();
                        string __tname = os->toString().toChar();
                        os->pop();

                        // Now to iterate over all targets and match them.
                        os->pushValueById(targets->valueID);
                        os->getProperty(-1, "keys"); // triggers the getKeys method.
                        int klen = os->getLen();
                        Value::Array* keys = new Value::Array(os);
                        for(int i=0; i<klen; i++) {
                            Value::String* key = (Value::String*)(*keys)[i];
                            Value::Object* val = (Value::Object*)(*targets)[(*key)];
                            string target = (*key);
                            Value::String* ruleName = (Value::String*)(*val)["_"];
                            Value::Array* input = (Value::Array*)(*val)["input"];
                            string rule = (*ruleName);
                            Value::Object* ruleObj = (Value::Object*)(*rules)[rule];
                            Value::String* ruleDisplay = (Value::String*)(*ruleObj)["display"];
                            string display = (*ruleDisplay);
                            if(wildcard(__tname, target)) {
                                // The task we are working on...
                                Task* t = new Task;
                                t->targetName = target;
                                t->target = val;
                                t->ruleName = rule;
                                t->ruleDisplay = display;
                                t->output = estimateRuleOutput(rule, target);

                                for(int j=0; j<input->length(); j++) {
                                    Value::String* v_file = (Value::String*)(*input)[j];
                                    string file = (*v_file);
                                    bool successor=true;
                                    // Now we need to resolve the rules.
                                    // We have rule, file and target name.
                                    string partialInput = makeTasks(rule, file, target, t, successor);
                                    if(partialInput == file) {
                                        t->output = estimateRuleOutput(rule, target, file);
                                        t->input.push_back(file);
                                    } else if(!partialInput.empty()) {
                                        t->input.push_back(partialInput);
                                    } else if(!successor) {
                                        cerr << "IceTea: Failed while preparing rule: " << rule << endl;
                                        return false;
                                    } else {
                                        cerr << "IceTea: Appearently there was no matching routine to process '"
                                             << file << "' with specified '"<< rule << "'. Are your rules properly "
                                             << "connected?" << endl;
                                        continue; // Skip the rest of the loop.
                                    }
                                    delete v_file;
                                }

                                // Fill in the rest...
                                // Find out what type the rule is. A function, or an array of commands?
                                os->pushValueById(ruleObj->valueID);
                                os->getProperty(-1, runScheme.c_str());
                                if(os->getTypeStr() == "array") {
                                    t->type=COMMAND;
                                    for(int k=0; k<os->getLen(); k++) {
                                        os->pushNumber(k);
                                        os->getProperty();
                                        t->commands.push_back( os->toString().toChar() );
                                    }
                                } else if(os->getTypeStr() == "function") {
                                    t->type=SCRIPT;
                                } else {
                                    if(cli->check("-w")) {
                                        // Whipe mode. Replace the emptyness right there with an actual thing.
                                        os->pushValueById(ruleObj->valueID);
                                        os->pushCFunction(os_target_clean, (void*)t);
                                        os->setProperty(-2, runScheme.c_str());
                                    } else {
                                        cerr << "[IceTea]: Rule '"<< rule << "' must have it's '" << runScheme << "' property defined as function or array! "
                                                  << "'" << os->getTypeStr() << "' was given instead." << endl;
                                    }
                                }
                                t->parent = NULL;
                                t->isMaster(true);
                                tasks->add(t);

                                // One thing we forgot.
                                if(__tname == target) break;
                            } else {
                                cerr << "Target '" << __tname << "' does not exist or the pattern doesnt match." << endl;
                                // clean up
                                delete key;
                                delete val;
                                delete input;
                                return false;
                            }
                        }
                    }
                }
            }
        }
    }
    return result;
}

// Because Run() is our thread function, it needs some depdendencies
// The needed struct is above.
static int CurrentTaskCount=0; ///< The current task count.
static map<string, Task*> tlog; ///< Used to log unhandled tasks.
static map<string, Task*> updatelog; ///< Used to store updated tasks
static map<string, bool> taskBuilt; ///< Currently unused. Maybe to store built targets.
static tthread::mutex listMutex; ///< Prevent all threads from simultanous access to the above.

/**
    @brief Used to avoid duplicates

    Searches and prohibits duplicates
*/
void addToMap(map<string,Task*>& target, string key, Task* val) {
    map<string,Task*>::iterator it = target.begin();
    bool found=false;
    for(; it!=target.end(); ++it) {
        if(it->first == key) found = true;
    }
    if(!found) {
        target[key]=val;
    }
}

/**
    @brief Print a task message

    This function is used to properly print the progress message for the task.
*/
void taskPrinter(Task* task) {
    tout << "[#" << CurrentTaskCount << "] "
         << task->targetName << "(" << task->ruleDisplay << ")" << ": ";
    if(task->input.size() == 1 && !task->isMaster()) {
        tout << task->input.front();
    } else {
        tout << task->output;
    }
    tout << endl;
    print();
}

/**
    @brief The task executor.

    This function runs the tasks within the Task queue. It is being spawned by
    the main function.

    It's execution is halted untill @ref ConfigMutex is unlocked.

    @param threadData An instance of @ref WorkQueue.
    @returns Nothing.
    @see OSMutex
    @see WorkQueue
*/
void Run(void* threadData) {
    // We just wait untill we can aquire a lock.
    ConfigMutex.lock(); // Should wait here.
    ConfigMutex.unlock();

    TaskQueue* tasks = (TaskQueue*)threadData;
    Task* task;

    while(!tasks->hasToStop()) {
        if(tasks->remove(task) && task != NULL) {
            if(!task->isBuilt() || task->shouldUpdate()) {
                if(task->isProcessing()) {
                    // Another thread is using this.
                    tasks->add(task);
                    continue;
                }
                // o.o a dangouers goto.
                Run__build:

                // Snag it!
                // FIXME for osme reason, this doesnt work. It deadlocks.
                //if(!task->try_lock()) {
                //    // Nope.
                //    tasks->add(task);
                //    continue;
                //} else {
                //    task->lock();
                //}
                task->isProcessing(true);

                // Check if our files are available
                bool doCheck=true;
                if(!cli->check("-w")) {
                    for(StrList::iterator it = task->input.begin(); it!=task->input.end(); ++it) {
                        if(!is_present(*it)) {
                            // O-lala.
                            if(task->child != NULL) {
                                // do tests against the child
                                if(task->child->output == *it) {
                                    if(!task->child->isBuilt()) {
                                        tasks->add(task->child);
                                    }
                                }
                            }
                            if(!task->isBuilt()) tasks->add(task);
                            doCheck=false;
                            break;
                        }
                    }
                } else {
                    // We are in cleanage mode.
                    doCheck = true;
                }

                // We can not continue from within a for loop.
                if(!doCheck) continue;

                // Do we actually have to build?
                bool doBuild=false;
                bool invokeParent = false;
                #ifdef IT_DEBUG
                tout << task->output << " ";
                #endif
                if(cli->check("-w")) {
                    // Clean, cleaner, cleanest.
                    doBuild = true;
                } else if(!is_present(task->output)) {
                    doBuild = true;
                    #ifdef IT_DEBUG
                    tout << "Hit: !is_present" << endl;
                    print();
                    #endif
                } else if(task->shouldUpdate()) {
                    // Well of course, thats why we're here.
                    doBuild = true;
                    #ifdef IT_DEBUG
                    tout << "Hit: shouldupdate" << endl;
                    print();
                    #endif
                } else if(task->child != NULL && task->child->shouldUpdate()) {
                    doBuild = true;
                    #ifdef IT_DEBUG
                    tout << "Hit: child update" << endl;
                    print();
                    #endif
                } else {
                    listMutex.lock();
                    string cached = fc->get(task->output, "sha2_files");
                    listMutex.unlock();
                    if(cached.empty()) {
                        // Output is not present in the cache.
                        doBuild = true;
                        #ifdef IT_DEBUG
                        tout << "Hit: cache if" << endl;
                        print();
                        #endif
                    } else {
                        #ifdef IT_DEBUG
                        tout << "Hit: cache else" << endl;
                        print();
                        #endif
                        if(cached != file2sha2(task->output)) doBuild=true;
                        for(StrList::iterator nt=task->input.begin(); nt!=task->input.end(); ++nt) {
                            string fhex = file2sha2(*nt);
                            string chex = fc->get(*nt, "sha2_files");
                            if(fhex!=chex) {
                                doBuild = true;
                                break;
                            }
                        }
                    }
                }
                if(doBuild) {
                    // The parent will very likely want to update.
                    if(task->parent != NULL) invokeParent = true;

                    // Im totally NOT abusing this mutex....no....never.
                    listMutex.lock();
                    CurrentTaskCount++;
                    taskPrinter(task);
                    listMutex.unlock();
                    // ...ever.

                    // Its buildtime.
                    bool buildSuccess = false;
                    Run_choice:
                    if(task->type == SCRIPT) {
                        Locker* g = new Locker(OSMutex);
                        os->pushValueById(rules->valueID);
                        os->getProperty(task->ruleName.c_str());
                        os->pushString(runScheme.c_str()); // build, clean.
                        os->getProperty(-2);
                        os->pushValueById(rules->valueID);
                        os->getProperty(task->ruleName.c_str());
                        os->newArray(task->input.size());
                        for(StrList::iterator st=task->input.begin(); st!=task->input.end(); ++st) {
                            os->pushString(st->c_str());
                            os->addProperty(-2);
                        }
                        os->pushString(task->output.c_str());
                        os->pushString(task->targetName.c_str());
                        os->pushValueById(task->target->valueID);
                        os->callFT(4,1);
                        if(os->isArray()) {
                            // We got a set of commands to run.
                            // Switch the type, we're gonna go backwards!
                            task->type = COMMAND;
                            int alen=os->getLen();
                            for(int l=0; l<alen; l++) {
                                os->pushNumber(l);
                                os->getProperty(-2);
                                string prop = os->toString().toChar();
                                os->pop(2);
                                task->commands.push_back( prop );
                            }
                            delete g;
                            goto Run_choice;
                        } else if(os->isType(OS_VALUE_TYPE_BOOL) && os->toBool() == false) {
                            tout << "FAILED: " << task->targetName << "(" << task->ruleDisplay << ")::" << runScheme << endl;
                            tout << "Function returned false." << endl;
                            print();
                            tasks->stop();
                        } else {
                            // We assume a success.
                            buildSuccess = true;
                        }
                        os->pop();
                        delete g;
                    } else {
                        vector<string>::iterator it = task->commands.begin();
                        for(; it!=task->commands.end(); ++it) {
                            string cmdstr(*it);
                            if(cmdstr.empty() || cmdstr.c_str() == NULL) {
                                // The command string points to invalid memory, it seems...
                                //break;
                            }
                            CommandResult rc = it_cmd(cmdstr, vector<string>());
                            if(rc.exit_code != 0) {
                                tout << "FAILED: " << cmdstr << endl;
                                tout << "Program exited with status: " << rc.exit_code << endl;
                                tasks->stop();
                            } else if(rc.p->error()) {
                                tout << "FAILED: " << cmdstr << endl;
                                tout << "Reason: " << rc.p->error_text() << endl;
                                tasks->stop();
                            } else if(!rc.spawned) {
                                tout << "FAILED: " << cmdstr << endl;
                                tout << "Subprocess could not be started." << endl;
                                tasks->stop();
                            } else {
                                // We believe its a success.
                                buildSuccess = true;
                            }
                            if(cli->check("-v") && rc.streams[1].length() > 0) {
                                string sout;
                                stringstream ssout(rc.streams[1]);
                                while(getline(ssout, sout).good()) {
                                    tout << "| " << sout << endl;
                                }
                            }
                            if(rc.streams[2].length() > 0) {
                                string serr;
                                stringstream sserr(rc.streams[2]);
                                while(getline(sserr, serr).good()) {
                                    tout << "> " << serr << endl;
                                }
                            }
                            print();
                        }
                    }
                    if(buildSuccess && !cli->check("-w")) {
                        bool doGoto=false;
                        task->isBuilt(true);
                        task->shouldUpdate(false);
                        string hash = file2sha2(task->output);
                        fc->set(task->output, hash, "sha2_files");
                        // Dont forget to save the inputs.
                        for(StrList::iterator gt=task->input.begin(); gt!=task->input.end(); ++gt) {
                            fc->set(*gt, file2sha2(*gt), "sha2_files");
                        }
                        if(invokeParent && task->parent != NULL) {
                            task->parent->shouldUpdate(true);
                            task->parent->isBuilt(false); // i call spoof.
                            //tasks->add(task->parent);
                            task->unlock();
                            task = task->parent;
                            #ifdef IT_DEBUG
                            tout << "Invoked parent for " << task->output << endl;
                            print();
                            #endif
                            doGoto = true;
                        }
                        fc->sync();
                        if(doGoto) goto Run__build;
                    }
                }
                task->isProcessing(false);
                //task->unlock();
            }
        }
    }
}

/**
    @brief Handle the exception within ObjectScript and exit nicely.

    This function is to print a proper error message and exit out of the application at last.

    A usage might be

    ~~~{.cpp}
    os->eval("throw new Exception('o.o')");
    if(terminator()) {
        exit(itCleanup());
    }
    ~~~

    @returns Wether to exit or not.
    @see itCleanup
*/
inline bool terminator() {
    if(os->isExceptionSet()) {
        cerr << "--- Terminating ---" << endl;
        return true;
    } else {
        return false;
    }
}

/**
    @brief Clean the environment and prepare exiting.

    This function cleans up the global variables, removes an emoty output
    folder if needed and finally returns a proper exit code.

    @returns An exit code. Can be used in `int main(int,char**)` or alike.
*/
int itCleanup() {
    // Do a proper cleanup if needed.
    folder_empty(outputDir) && folder_delete(outputDir);

    // OS might has an info we want.
    int rtc=0;
    if(os->isExceptionSet()) {
        rtc=1;
    } else if(os->isTerminated()) {
        rtc=os->getTerminatedCode();
    }

    delete cli;
    delete targets;
    delete rules;
    delete actions;
    os->release();
    delete tasks;

    return rtc;
}

/// Wrapper around @ref estimateRuleOutput
OS_FUNC(os_estimateRuleOutput) {
    EXPECT_STRING(1)
    EXPECT_STRING(2)
    EXPECT_STRING(3)
    string rule = os->toString(-params+0).toChar();
    string target = os->toString(-params+1).toChar();
    string file = os->toString(-params+2).toChar();
    os->pushString(estimateRuleOutput(rule,target,file).c_str());
    return 1;
}

/**
    @brief Return the object of a target.

    This function can be used to obtain a target's configuration object from
    any place within the script.

    @param name The name of the target
    @returns An object that is associated with the target.
    @note This is a script function, not C++.
*/
OS_FUNC(os_getTarget) {
    EXPECT_STRING(1)
    string target = os->toString(-params+0).toChar();
    if(targetExists(target)) {
        os->pushValueById(
            ((Value::Object*)(*targets)[target])->valueID
        );
    } else {
        os->pushBool(false);
    }
    return 1;
}

/// Comparsion function for TaskQueue
bool task_comparsion(Task* tl, Task* tr, TaskQueue* queue) {
    return (tl->output == tr->output);
}

/**
    @brief The main logic

    Everything that is cool happens in here.
*/
int main(int argc, const char** argv) {
    // Just for debugging
    #ifdef __APPLE__
    signal(SIGSEGV, handler);
    signal(SIGABRT, handler);
    #endif
    // Inner globals
    os = OS::create();

    // Make OS silent about creating files.
    os->setSetting(OS_SETTING_CREATE_TEXT_OPCODES,      0);
    os->setSetting(OS_SETTING_CREATE_TEXT_EVAL_OPCODES, 0);
    os->setSetting(OS_SETTING_CREATE_COMPILED_FILE,     0);
    os->setSetting(OS_SETTING_SOURCECODE_MUST_EXIST,    1);

    // Generate a pretty copyright.
    stringstream cpr;
    cpr << "IceTea 0.1.3 by Ingwie Phoenix" << endl
        << OS_COPYRIGHT << endl
        << "TinyThread++ " << TINYTHREAD_VERSION_MAJOR << "." << TINYTHREAD_VERSION_MINOR << endl
        << "stlplus " << stlplus::version() << endl;

    // Arg stuff.
    cli = new CLI(argc, argv, cpr.str());
    // Fetch thread number beforehand!
    stringstream thrs_sst;
    thrs_sst << thread::hardware_concurrency();
    cli->group("Main options");
    cli->insert("-f", "--file", "<file>", "Select a different built file.", true, "./build.it");
    cli->insert("-b", "--boot", "<file>", "Select a different bootstrap file.", true, "./bootstrap.it");
    cli->insert("-C", "--chdir", "<dir>", "Change to this directory before doing anything.");
    cli->insert("-h", "--help", "", "Show this help.");
    cli->insert("-u", "--usage", "", "Show the usage details of a project plus this help.");
    cli->insert("-d", "--dir", "<path>", "Place where to store output files. Default: ./out", true, "./out");
    cli->insert("-D", "--define", "key=value", "Define a global value within the scripting language's scope.");
    cli->insert(
        "-j", "--jobs", "<N>",
        "Amount of jobs to run at the same time. Default: "+thrs_sst.str(),
        true, thrs_sst.str()
    );
    cli->insert("-p", "--purge", "", "Purge the cache file.");
    cli->insert("-t", "--target", "<target>", "Build only the specified target.");

    cli->group("Developer options");
    cli->insert("", "--dump", "", "Dump the internal bootstrap.it to standart output. Save it using redirection: icetea --dump >./build.it");
    cli->insert("", "--print-targets", "", "Dump all targets in JSON format to standard error output. Export with: icetea --print-targets 2>targets.json");
    cli->insert("", "--print-actions", "", "Dump all actions in JSON format to standard error output. Export with: icetea --print-actions 2>actions.json");
    cli->insert("-r", "--dry-run", "", "Don't actually build, but do a dry-run.");
    cli->insert("-v", "--verbose", "", "Commands are shown instead of the progress indicator.");
    cli->insert("-o", "--os", "<file>", "Run an ObjectScript file. Only it will be ran, other options are ignored.");
    cli->insert("-e", "--os-exec", "<str>", "Run ObjectScript code from string, or if - was given, then from standard input.");
    cli->group("Build options");
    cli->insert(
        "-w", "--wipe", "",
        "Sets the build scheme to 'clear'. Will delete output files."
    );
    cli->insert("-c", "--configure", "", "Only configure the specified projects and do not build.");
    cli->insert("-m", "--arch", "<str>", "If building architecture-dependent apps, supply your architecture here.", true);
    cli->insert("-s", "--vendor", "<str>", "Supply the vendor of your OS. That is the center part of a platform tripple.", true);
    cli->insert("", "--tripple", "<str>", "Supply a platform tripple.", true);
    cli->insert("-g", "--debug", "", "Turn on the global DEBUG variable (set it to true)");
    cli->setStrayArgs("Action", "Action to execute. Defaults to: all");
    cli->parse();

    // Fetching values...
    string bootstrapit = cli->value("-b");
    string buildit = cli->value("-f");
    outputDir = cli->value("-d");

    const char* env_boot = getenv("ICETEA_BOOTSTRAP");
    if(env_boot != NULL) {
        bootstrapit = env_boot;
    }

    // Output folder
    folder_create(outputDir);

    // Cache
    string cacheFile = create_filespec(outputDir, ".cache.it");
    if(cli->check("-p")) {
        file_delete(cacheFile);
    }
    fc = new Filecache( cacheFile );

    if(cli->check("-h")) {
        cli->usage();
        return itCleanup();
    }

    // Initialize ObjectScript with our stuff.
    initIceTeaExt(os, cli);
    initializeDetector(os, fc, cli);
    initializeStdlib(os);
    // Those functions are only seen in here.
    OS::FuncDef itFuncs[] = {
        {OS_TEXT("estimateRuleOutput"), os_estimateRuleOutput},
        {OS_TEXT("getTarget"),          os_getTarget},
        //{OS_TEXT("build")},
        //{OS_TEXT("clean")},
        //{OS_TEXT("ruleExists")},
        //{OS_TEXT("actionExists")},
        //{OS_TEXT("targetExists")},
        {}
    };
    os->getModule("icetea");
    os->setFuncs(itFuncs);
    os->pop();

    // Generic
    OS::StringDef strs[] = {
        {OS_TEXT("__buildit"),     OS_TEXT(buildit.c_str())},
        {OS_TEXT("__bootstrapit"), OS_TEXT(bootstrapit.c_str())},
        {OS_TEXT("__outputdir"),   OS_TEXT(outputDir.c_str())},
        {OS_TEXT("__cachefile"),   OS_TEXT(cacheFile.c_str())},
        {}
    };
    os->pushGlobals();
    os->setStrings(strs);
    os->pop();

    if(cli->check("-g")) {
        os->eval("_G.DEBUG=true");
    } else {
        os->eval("_G.DEBUG=false");
    }

    // Wiping the source tree...
    if(cli->check("-w")) {
        runScheme = "clean";
    }

    // Auto-include
    if(folder_exists(".IceTea")) {
        // Get all *.it files
        vector<string> itFiles = folder_wildcard(".IceTea", "*.it", false, true);
        for(vector<string>::iterator it=itFiles.begin(); it!=itFiles.end(); ++it) {
            string full = create_filespec(".IceTea", *it);
            os->require(full.c_str());
        }
        // Also make require aware of this new path.
        os->pushGlobals();
        os->pushString("require");
        os->getProperty(-2);
        os->pushString("paths");
        os->getProperty(-2);
        string fullDir = create_filespec(folder_current_full(), ".IceTea");
        os->pushString(fullDir.c_str());
        os->addProperty(-2);
        os->pop();
    }

    // Run the script to populate the global objects.
    if(file_exists(bootstrapit)) {
        os->require(bootstrapit.c_str());
        if(terminator()) return itCleanup();
    } else {
        // We'd have to load one from memory...+
        cerr << "IceTea: Using internal bootstrap.it file, since '" << bootstrapit << "' "
             << "was not found." << endl;
    }
    if(file_exists(buildit)) {
        os->require(buildit.c_str());
        if(terminator()) return itCleanup();
    } else {
        if(!cli->check("--os") && !cli->check("--os-exec")) {
            cerr << "File \"" << cli->value("-f") << "\" does not exist! Aborting." << endl;
            return itCleanup();
        }
    }

    // We have a script file to run.
    if(cli->check("--os")) {
        os->require(cli->value("--os").c_str());
        return itCleanup();
    }
    // Or a string.
    if(cli->check("--os-exec")) {
        string code;
        if(cli->value("--os-exec") == "-") {
            cerr << "Press CTRL+D or end input to STDIN to run the code." << endl;
            string line;
            while(getline(cin, line).good()) {
                code += line;
            }
        } else {
            code = cli->value("--os-exec");
        }
        cout << "os> " << code << endl;
        os->eval(code.c_str());
        return itCleanup();
    }

    // Pre-check
    if(cli->check("-u")) cli->group("Project options");

    // init() has added its methods, now its time to put it in.
    cli->parse();

    // Call every target's init() method.
    if(!Initializer()) {
        return itCleanup();
    }

    // Post-check
    if(cli->check("-u")) {
        cli->usage();
        return itCleanup();
    } else {
        // Run configure and strech inputs
        if(!Preprocessor()) return itCleanup();

        // We do this after we configure, since now all the filenames are streched.
        if(cli->check("--print-targets")) {
            os->getGlobal("json");
            os->getProperty("encode");
            os->pushValueById(targets->valueID);
            os->callF(1,1);
            cerr << os->toString() << endl;
            return itCleanup();
        }

        if(cli->check("--print-actions")) {
            os->getGlobal("json");
            os->getProperty("encode");
            os->pushValueById(actions->valueID);
            os->callF(1,1);
            cerr << os->toString() << endl;
            return itCleanup();
        }

        // If the user only wanted to configure, skip away.
        if(cli->check("-c")) {
            return itCleanup();
        }

        // Lock the threads from doing anything
        ConfigMutex.lock();

        // At this point, we can savely begin to create the needed things for threaded work.
        int thrs;
        stringstream tmp;
        tmp << cli->value("-j");
        tmp >> thrs;
        // Now that we have the thread amount value, lets make it happen.
        tasks = new TaskQueue(thrs, Run, os, task_comparsion);

        // Now, we've got a waiting task runner there, we ned to feed it one by one.
        bool transformedYet=Transformer();
        ConfigMutex.unlock();
        if(!transformedYet) {
            // Meep! We have to kill all the threads, at once.
            // We can savely let it go to a fiish by stopping them.
            tasks->stop();
        } else {
            // Run untill the list is empty... and then stop all threads.
            tasks->drain();
            tasks->stop();
        }

        // Make sure all threads come together first.
        tasks->joinAll();

        // Well that might happen.
        if(runTasks && CurrentTaskCount == 0) {
            cout << endl << "No task had to be run." << endl;
        }
    }

    return itCleanup();
}
