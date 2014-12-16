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

// Namespaces!
using namespace std;
using namespace tthread;
using namespace ObjectScript;
using namespace stlplus;
using namespace picosha2;

// IceTea stuff
/*
    This will store the targets. Structure is:
    {
        project: {name: {...}}
    }
    Currently its just name:data
*/
Value::Object* targets;

/*
    This holds the rules. Structure:
    {
        shortName: {...}
    }
*/
Value::Object* rules;

// Actions. { name: {...} }
Value::Object* actions;

// A task in C++ representation
enum TaskType { SCRIPT, COMMAND };
struct Task {
    string ruleName;
    string targetName;
    string ruleDisplay;
    list<string> input;
    string output;

    TaskType type;
    // [ cmd1, cmd2, ... ]
    vector<string> commands;
    // Rule.build: function(input, output, target)
    // The function is either called with only one input - or an input array. Thus access to the
    // target is given for settings - such as include dirs and alike.
    Value::Method* onBuild;

    // The related target, pushed to the function.
    Value::Object* target;

    // Increased if target can not be built due to missing dependency.
    int failureCounter;

    // Iteration?
    Task* parent;
    Task* child;

    ~Task() {
        if(onBuild!=NULL) delete onBuild;
        delete target;
    }
};
typedef WorkQueue<Task*, OS*> TaskQueue;

// Other globals...
OS*        os;
CLI*       cli;
Filecache* fc;
TaskQueue* tasks;
string     runScheme("build");
string     outputDir;
// In order to call OS savely, we use this.
static tthread::mutex OSMutex;
static tthread::mutex ConfigMutex;
typedef tthread::lock_guard<tthread::mutex> Locker;

// Tool functions
void getFileList(string pattern, vector<string> &rt_list) {
    string dirName = folder_part(pattern);
    string rpattern = filename_part(pattern);
    if(dirName == "" || dirName == ".") dirName = "./";
    vector<string> res = folder_wildcard(dirName, rpattern);
    for(vector<string>::iterator it=res.begin(); it!=res.end(); ++it) {
        rt_list.push_back( create_filespec(dirName, *it) );
    }
}
void getFileListRec(Value::Array* arr, vector<string> &str_list) {
    for(int i=0; i<arr->length(); i++) {
        Value::String* vs_pat = (Value::String*)(*arr)[i];
        string pat = (*vs_pat);
        getFileList(pat, str_list);
    }
}

int itCleanup();

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
}
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

string estimateRuleOutput(string rule, string target, string file="") {
    os->pushString(rule.c_str());
    os->pushValueById(rules->valueID);
    if(!os->in()) {
        cerr << "[IceTea]: " << rule << " does not exist.";
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
            t->onBuild = new Value::Method(os, ruleObj, runScheme.c_str());
        } else {
            cerr << "[IceTea]: Rule '"<< rule << "' must have it's build property defined as function or array! "
                 << "'" << os->getTypeStr() << "' was given instead." << endl;
        }

        t->target = (Value::Object*)(*targets)[target];

        // Relate it
        t->child = NULL;
        t->parent = parent;
        parent->child = t;

        tasks->add(t);

        // We send the output of this task back to the parent call.
        // That is the new input.
        input = myExpected;
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
        Value::Object* outputObj = (Value::Object*)(*ruleObj)["output"];
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

                Task* t = new Task;
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
                    os->pushValueById(currRule->valueID);
                    Value::Object* theRule = new Value::Object(os);
                    t->onBuild = new Value::Method(os, theRule, runScheme.c_str());
                } else {
                    cerr << "[IceTea]: Rule '"<< rule << "' must have it's build property defined as function or array! "
                         << "'" << os->getTypeStr() << "' was given instead." << endl;
                }

                t->target = (Value::Object*)(*targets)[target];
                parent->child = t;
                t->parent = parent;

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
                    string newInput;
                    __makeTasks((*ruleName), file, target, newInput, t, success);
                    //cout << (*ruleName) << ": Current is CHILD -- " << file << endl;
                    //cout << rule << "@" << (*ruleName) << ": " << newInput << " | " << myExpected << endl;

                    // If the newInput we got is empty, then we probably should skip this rule.
                    if(newInput.empty()) {
                        //cout << "Oh gosh, its empty." << endl;
                        continue;
                    }

                    //cout << (*ruleName) << ": adding with > " << newInput << endl;

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


string makeTasks(string rule, string file, string target, Task*& parent, bool& successor) {
    string tmp;
    // The first call ALWAYS results in the actual thing.
    __makeTasks(rule, file, target, tmp, parent, successor, true);
    return tmp;
}

/* The task processors
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
// Processors
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
bool Preprocessor() {
    os->pushValueById(targets->valueID);
    os->getProperty(-1, "keys"); // triggers the getKeys method.
    Value::Array keys(os);
    for(int i=0; i<keys.length(); i++) {
        Value::String* key = (Value::String*)keys[i];
        Value::Object* val = (Value::Object*)(*targets)[(*key)];
        os->pushValueById(val->valueID);
        os->pushString("configure");
        if(os->in(-1, -2)) {
            os->popString();
            os->getProperty("configure");
            if(os->isFunction()) {
                Value::Method mt(os, val, "configure");
                mt(0,1);
                if(os->toBool() == false) {
                    cerr << "We have a false!" << endl;
                    // Configure returned false. So we quickly drop out.
                    return false;
                }
            } else {
                //cerr << "[IceTea]: The configure() method in " << key << "is not a function! Found "
                //     << os->getTypeStr().toChar() << " instead.";
            }
        }

        // Now strech the input parameter.
        Value::Array* input = (Value::Array*)(*val)["input"];
        vector<string> fileList;
        getFileListRec(input, fileList);
        os->pushValueById(val->valueID);
        int ojoff = os->getAbsoluteOffs(-1);
        os->newArray(); // Overwrite the previous
        int aroff = os->getAbsoluteOffs(-1);
        for(vector<string>::iterator it=fileList.begin(); it!=fileList.end(); ++it) {
            os->pushString((*it).c_str());
            os->addProperty(aroff);
        }
        os->setProperty(ojoff, "input");
        os->pop();

        // clean up
        delete key;
        delete val;
        delete input;

        // Cause IceTea to exit
        if(os->isExceptionSet()) return false;
    }
    return true;
}
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
            cout << "–– Running: " << *it << endl;
            os->pushValueById(actions->valueID);
            os->pushString(it->c_str());
            os->getProperty(-2);
            if(os->isFunction()) {
                // We need to give users the IceTea.build function... FIXME
                os->callF(0,0);
                if(os->isNumber()) {
                    result = os->toBool();
                }
                result = true;
            } else {
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
                        Value::Array* keys = new Value::Array(os);
                        for(int i=0; i<keys->length(); i++) {
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
                                    #ifdef IT_DEBUG
                                        cerr << "Before makeTaks: " << os->ref_count << endl;
                                        cerr << "makeTasks(" << rule << ", "<< file << ", "<< target <<", ...)" << endl;
                                    #endif
                                    string partialInput = makeTasks(rule, file, target, t, successor);
                                    // The final rule itself however, is reserved to be done by hand.
                                    if(!partialInput.empty()) {
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
                                #ifdef IT_DEBUG
                                    cerr << "after MakeTasks: " << os->ref_count << endl;
                                #endif

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
                                    t->onBuild = new Value::Method(os, ruleObj, runScheme.c_str());
                                } else {
                                    cerr << "[IceTea]: Rule '"<< rule << "' must have it's build property defined as function or array! "
                                         << "'" << os->getTypeStr() << "' was given instead." << endl;
                                }
                                t->parent = NULL;
                                tasks->add(t);

                                #ifdef IT_DEBUG
                                    cerr << "Transformer, after | OS' refcount: " << os->ref_count << endl;
                                #endif
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
int CurrentTaskCount=0;
static map<string, Task*> tlog;
static map<string, Task*> updatelog;
void Run(void* threadData) {
    // We just wait untill we can aquire a lock.
    ConfigMutex.lock(); // Should wait here.
    ConfigMutex.unlock();

    stringstream s;
    LinePrinter p(&s);
    TaskQueue* tasks = (TaskQueue*)threadData;
    Task* task;
    bool cancel=false;

    while(!tasks->hasToStop()) {
        if(cancel || !tasks->remove(task)) break;
        if(task!=NULL) {
            bool canExecute=false;
            for(list<string>::iterator it2=task->input.begin(); it2!=task->input.end(); ++it2) {
                if(!file_exists( *it2 )) {
                    // Put the task back in, we cant run it now.
                    tasks->add(task);
                    canExecute=false;
                    break;
                } else {
                    canExecute=true;
                }
            }

            if(canExecute) {
                bool reallyRun=true;
                bool updateHash=false;
                map<string,string> updateHashes;
                for(list<string>::iterator at=task->input.begin(); at!=task->input.end(); ++at) {
                    string cached = fc->get(*at, "sha2_files");
                    string hex = file2sha2(*at);
                    if(cached.empty()) {
                        // The file is not in the cache, add it.
                        if(hex.empty()) {
                            s << "Error while building SHA2 sum of '" << *at << "'" << endl;
                            p();
                            tasks->stop();
                            reallyRun = false;
                            break;
                        } else {
                            fc->set(*at, hex, "sha2_files");
                        }
                    } else {
                        if(
                            (
                                task->child != NULL
                                && fc->get(task->child->output, "sha2_files") != file2sha2(task->child->output)
                            ) || (
                                task->child != NULL
                                && updatelog.find(task->child->output) != updatelog.end()
                            )
                        ) {
                            reallyRun = true;
                            updateHash = true;
                            break;
                        } else if(cached == hex && file_exists(task->output)) {
                            // The file is up2date
                            reallyRun = false;
                            tlog[task->output]=task;
                        } else {
                            // It's not and we gotta do soemthing about it!
                            updatelog[task->output]=task;
                            reallyRun = true;
                            updateHash=true;
                            updateHashes[*at]=hex;
                        }
                    }
                    fc->sync();
                    if(updateHash) {
                        if(task->parent != NULL) {
                            if(task->parent->parent != NULL) {
                                file_delete(task->parent->parent->output);
                            }
                            file_delete(task->parent->output);
                        }
                    }
                }

                // Sanity check
                if(!reallyRun) continue;

                // Do this test.
                bool doHexCheck=false;
                string outhex;
                if(file_exists(task->output)) {
                    outhex = file2sha2(task->output);
                    doHexCheck=true;
                }

                CurrentTaskCount++;
                s << "[#" << CurrentTaskCount;
                if(cli->check("-v")) {
                    s << " | Remaining: " << tasks->size();
                }
                s << "]";
                s << " " << task->targetName << "(" << task->ruleDisplay << ")" << ": ";
                if(task->input.size() == 1 ) {
                    s << task->input.front();
                } else {
                    s << task->output;
                }
                s << endl;
                #ifdef IT_DEBUG
                    s << "Run | OS' refcount: " << os->ref_count << endl;
                #endif
                p();
                Run_choice:
                if(task->type == SCRIPT) {
                    Locker* g = new Locker(OSMutex);
                    os->newArray(task->input.size());
                    for(list<string>::iterator it=task->input.begin(); it!=task->input.end(); ++it) {
                        os->pushString(it->c_str());
                        os->addProperty(-2);
                    }
                    os->pushString(task->output.c_str());
                    os->pushString(task->targetName.c_str());
                    os->pushValueById(task->target->valueID);
                    (*task->onBuild)(4,1);
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
                        s << "FAILED: " << task->targetName << "(" << task->ruleDisplay << ")::build" << endl;
                        s << "Function returned false." << endl;
                        p();
                        tasks->stop();
                        cancel=true;
                    }
                    os->pop();
                    delete g;
                } else {
                    vector<string>::iterator it = task->commands.begin();
                    for(; it!=task->commands.end(); ++it) {
                        CommandResult rc = it_cmd(*it, vector<string>());
                        if(rc.exit_code != 0) {
                            s << "FAILED: " << *it << endl;
                            s << "Program exited with status is: " << rc.exit_code << endl;
                            s << "STDERR: " << rc.streams[2];
                            s << "STDOUT: " << rc.streams[1];
                            tasks->stop();
                            cancel=true;
                        } else if(rc.p->error()) {
                            s << "FAILED: " << *it << endl;
                            s << "Reason: " << rc.p->error_text() << endl;
                            tasks->stop();
                            cancel=true;
                        } else if(!rc.spawned) {
                            s << "FAILED: " << *it << endl;
                            s << "Subprocess could not be started." << endl;
                            tasks->stop();
                            cancel=true;
                        }
                        if(rc.streams[1].length() > 0) {
                            string sout;
                            stringstream ssout(rc.streams[1]);
                            while(getline(ssout, sout).good()) {
                                s << "| " << sout << endl;
                            }
                        }
                        if(rc.streams[2].length() > 0) {
                            string serr;
                            stringstream sserr(rc.streams[2]);
                            while(getline(sserr, serr).good()) {
                                s << "> " << serr << endl;
                            }
                        }
                        p();
                    }
                }
                if(updateHash) {
                    for(map<string,string>::iterator mt=updateHashes.begin(); mt!=updateHashes.end(); ++mt) {
                        fc->set(mt->first, mt->second, "sha2_files");
                    }
                    fc->sync();
                }
                if(doHexCheck && outhex == file2sha2(task->output)) {
                    s << task->output << " was left UNCHANGED." << endl;
                }
                task = NULL;
                delete task;
            }
        }
    }
}

// Handle errors properly.
inline bool terminator() {
    if(os->isExceptionSet()) {
        cerr << "--- Terminating ---" << endl;
        return true;
    } else {
        return false;
    }
}

int itCleanup() {
    // Do a proper cleanup if needed.
    folder_empty(outputDir) && folder_delete(outputDir);

    delete cli;
    delete targets;
    delete rules;
    delete actions;
    os->release();
    delete tasks;

    return 0;
}

int main(int argc, const char** argv) {
    // Inner globals
    os = OS::create();

    // Make OS silent about creating files.
    os->setSetting(OS_SETTING_CREATE_TEXT_OPCODES,      0);
    os->setSetting(OS_SETTING_CREATE_TEXT_EVAL_OPCODES, 0);
    os->setSetting(OS_SETTING_CREATE_COMPILED_FILE,     0);
    os->setSetting(OS_SETTING_SOURCECODE_MUST_EXIST,    1);

    // Generate a pretty copyright.
    stringstream cpr;
    cpr << "IceTea 0.0.1 by Ingwie Phoenix" << endl
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
    const char* bootstrapit = cli->value("-b").c_str();
    const char* buildit = cli->value("-f").c_str();
    outputDir = cli->value("-d");

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

#ifdef IT_DEBUG
    cerr << "DEBUG: Before InitIceTeaExt()" << endl;
    cerr << "ObjectScript: ref count -> " << os->ref_count << endl;
#endif

    // Initialize ObjectScript with our stuff.
    initIceTeaExt(os, cli);
    initializeDetector(os, fc, cli);

    if(cli->check("-g")) {
        os->eval("DEBUG=true");
    } else {
        os->eval("DEBUG=false");
    }

    // Wiping the source tree...
    if(cli->check("-w")) {
        runScheme = "clean";
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
            string line;
            while(getline(cin, line)) {
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

    // Run the script to populate the global objects.
    if(file_exists(buildit)) {
        OS::StringDef strs[] = {
            {OS_TEXT("__buildit"),     OS_TEXT(buildit)},
            {OS_TEXT("__bootstrapit"), OS_TEXT(bootstrapit)},
            {OS_TEXT("__outputdir"),   OS_TEXT(outputDir.c_str())},
            {}
        };
        os->pushGlobals();
        os->setStrings(strs);
        os->pop();
        if(file_exists(bootstrapit)) {
            os->require(bootstrapit);
            if(terminator()) return itCleanup();
        } else {
            // We'd have to load one from memory...+
            cerr << "IceTea: Using internal bootstrap.it file, since '" << bootstrapit << "' "
                 << "was not found." << endl;
        }
        os->require(buildit);
        if(terminator()) return itCleanup();
    } else {
        cerr << "File \"" << cli->value("-f") << "\" does not exist! Aborting." << endl;
        return itCleanup();
    }

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
        #ifdef IT_DEBUG
            cerr << "Before Preprocessor..." << endl;
            cerr << "ObjectScript: ref count -> " << os->ref_count << endl;
        #endif

        // Run configure and strech inputs
        if(!Preprocessor()) return itCleanup();

        #ifdef IT_DEBUG
            cerr << "After Preprocessor..." << endl;
            cerr << "ObjectScript: ref count -> " << os->ref_count << endl;
        #endif

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
        #ifdef IT_DEBUG
            cerr << "Initializing with " << thrs << " threads." << endl;
        #endif
        tasks = new TaskQueue(thrs, Run, os);

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
        if(CurrentTaskCount == 0) {
            cout << endl << "No work had to be done." << endl;
        }

        // Its now save to delete the pool!
        return itCleanup();
    }

    #ifdef IT_DEBUG
        cerr << "After everything..." << endl;
        cerr << "ObjectScript: ref count -> " << os->ref_count << endl;
    #endif

    return itCleanup();
}
