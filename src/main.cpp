#include <iostream>
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

// FS access
#include "VFS.h"
#include "VFSTools.h"

// Hashing
#include "picosha2.h"

// Console
#include "lineprinter.h"

// Misc
#include "predef.h"
#include "cli.h"

// Namespaces!
using namespace std;
using namespace tthread;
using namespace ObjectScript;
using namespace ttvfs;
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

    ~Task() {
        if(onBuild!=NULL) delete onBuild;
        delete target;
    }
};
typedef WorkQueue<Task*, OS*> TaskQueue;

// Tool functions
void getFileList(string pattern, list<string> &rt_list) {
    StringList fList;
    string dirName = StripLastPath(pattern);
    if(dirName == "" || dirName == ".") dirName = "./";
    GetFileList(dirName.c_str(), fList);
    for(StringList::iterator j=fList.begin(); j != fList.end(); j++) {
        if(WildcardMatch((j->c_str()), PathToFileName(pattern.c_str()))) {
            rt_list.push_back(dirName + "/" + *j);
        }
    }
}
void getFileListRec(Value::Array* arr, list<string> &str_list) {
    for(int i=0; i<arr->length(); i++) {
        Value::String* vs_pat = (Value::String*)(*arr)[i];
        string pat = (*vs_pat);
        getFileList(pat, str_list);
    }
}

bool ruleExists(Value::String* ruleName, OS* os) {
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
bool ruleExists(const string ruleName, OS* os) {
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


std::string ReplaceString(std::string subject, const std::string& search,
                          const std::string& replace) {
    size_t pos = 0;
    while((pos = subject.find(search, pos)) != std::string::npos) {
         subject.replace(pos, search.length(), replace);
         pos += replace.length();
    }
    return subject;
}

void ReplaceStringInPlace(std::string& subject, const std::string& search,
                          const std::string& replace) {
    size_t pos = 0;
    while((pos = subject.find(search, pos)) != std::string::npos) {
         subject.replace(pos, search.length(), replace);
         pos += replace.length();
    }
}

// In order to call OS savely, we use this.
static tthread::mutex OSMutex;
typedef tthread::lock_guard<tthread::mutex> Locker;

string estimateRuleOutput(string rule, string target, OS* os, string file="") {
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
    string basename = PathToFileName(file.c_str());
    string ext = StripFileExtension(basename);
    ReplaceStringInPlace(myExpected, "%t", target);
    ReplaceStringInPlace(myExpected, "%b", basename);
    ReplaceStringInPlace(myExpected, "%e", ext);
    ReplaceStringInPlace(myExpected, "%f", file);

    // Clean up the heap
    delete outputs;
    delete o_myExpected;

    return myExpected;
}


void __makeTasks(
    string rule, string file, string target,
    TaskQueue* queue, OS* os,
    string& input, bool finalRule=false
) {
    if(!ruleExists(rule, os)) {
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
        if(WildcardMatch( file.c_str(), (*acceptPattern) )) {
            isBaseTarget = true;
            break;
        }
        delete acceptPattern;
    }

    // In any case, we want to create the expected value. Its easy.
    string myExpected = estimateRuleOutput(rule, target, os, file);

    if(isBaseTarget) {
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
        os->getProperty(-1, "build");
        if(os->getTypeStr() == "array") {
            t->type=COMMAND;
            for(int k=0; k<os->getLen(); k++) {
                os->pushNumber(k);
                os->getProperty();
                t->commands.push_back( os->popString().toChar() );
            }
        } else if(os->getTypeStr() == "function") {
            t->type=SCRIPT;
            t->onBuild = new Value::Method(os, ruleObj, "build");
        } else {
            cerr << "[IceTea]: Rule '"<< rule << "' must have it's build property defined as function or array! "
                 << "'" << os->getTypeStr() << "' was given instead." << endl;
        }

        t->target = (Value::Object*)(*targets)[target];
        queue->add(t);

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
                    if(WildcardMatch(file.c_str(), (*accPattern))) {
                        currentIsBase = true;
                    }
                    delete accPattern;
                    if(currentIsBase) break;
                }

                if(currentIsBase) {
                    // The file we have, and the rule we found, make up for a base.
                    // In this case, recall ourself. The above, first check will pass
                    // and it will execute a base rule.
                    string newInput;
                    __makeTasks((*ruleName), file, target, queue, os, newInput);
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
                    __makeTasks((*ruleName), file, target, queue, os, newInput);
                    //cout << (*ruleName) << ": Current is CHILD -- " << file << endl;
                    //cout << rule << "@" << (*ruleName) << ": " << newInput << " | " << myExpected << endl;

                    // If the newInput we got is empty, then we probably should skip this rule.
                    if(newInput.empty()) {
                        //cout << "Oh gosh, its empty." << endl;
                        continue;
                    }

                    string theExpected = estimateRuleOutput((*ruleName), target, os, file);
                    Value::String* t_rd = (Value::String*)(*currRule)["display"];
                    string display = (*t_rd);
                    string _ruleName = (*ruleName);
                    delete t_rd;

                    Task* t = new Task;
                    t->ruleName = _ruleName;
                    t->targetName = target;
                    t->ruleDisplay = display;
                    t->input.push_back(newInput);
                    t->output = theExpected;

                    os->pushValueById(currRule->valueID);
                    os->getProperty(-1, "build");
                    if(os->getTypeStr() == "array") {
                        t->type=COMMAND;
                        for(int k=0; k<os->getLen(); k++) {
                            os->pushNumber(k);
                            os->getProperty();
                            t->commands.push_back( os->popString().toChar() );
                        }
                    } else if(os->getTypeStr() == "function") {
                        t->type=SCRIPT;
                        os->pushValueById(currRule->valueID);
                        Value::Object* theRule = new Value::Object(os);
                        t->onBuild = new Value::Method(os, theRule, "build");
                    } else {
                        cerr << "[IceTea]: Rule '"<< rule << "' must have it's build property defined as function or array! "
                             << "'" << os->getTypeStr() << "' was given instead." << endl;
                    }

                    t->target = (Value::Object*)(*targets)[target];
                    queue->add(t);
                    input = theExpected;

                    //cout << (*ruleName) << ": adding with > " << newInput << endl;

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


string makeTasks(string rule, string file, string target, TaskQueue* queue, OS* os) {
    string tmp;
    // The first call ALWAYS results in the actual thing.
    __makeTasks(rule, file, target, queue, os, tmp, true);
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
bool Initializer(OS* os) {
    os->pushValueById(targets->valueID);
    os->getProperty(-1, "keys"); // triggers the getKeys method.
    Value::Array keys(os);
    for(int i=0; i<keys.length(); i++) {
        Value::String* key = (Value::String*)keys[i];
        Value::Object* val = (Value::Object*)(*targets)[(*key)];
        Value::String* ruleName = (Value::String*)(*val)["_"];
        if(!ruleExists(ruleName, os)) {
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
        if(os->isExceptionSet()) return false;
    }
    return true;
}
bool Preprocessor(OS* os) {
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
        list<string> fileList;
        getFileListRec(input, fileList);
        os->pushValueById(val->valueID);
        int ojoff = os->getAbsoluteOffs(-1);
        os->newArray(); // Overwrite the previous
        int aroff = os->getAbsoluteOffs(-1);
        for(list<string>::iterator it=fileList.begin(); it!=fileList.end(); ++it) {
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
bool Transformer(OS* os, TaskQueue* queue) {
    // And now lets get serious.
    // We only must push "safe" targets into the queue...
    // There needs to be a mechanism to determine the actual target list.
    // Its easy, really. For now, lets just do all of them.
    Locker g(OSMutex);
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

        // The task we are working on...
        Task* t = new Task;
        t->targetName = target;
        t->target = val;
        t->ruleName = rule;
        t->ruleDisplay = display;
        t->output = estimateRuleOutput(rule, target, os);

        for(int j=0; j<input->length(); j++) {
            Value::String* v_file = (Value::String*)(*input)[j];
            string file = (*v_file);
            // Now we need to resolve the rules.
            // We have rule, file and target name.
            #ifdef IT_DEBUG
                cerr << "Before makeTaks: " << os->ref_count << endl;
                cerr << "makeTasks(" << rule << ", "<< file << ", "<< target <<", ...)" << endl;
            #endif
            string partialInput = makeTasks(rule, file, target, queue, os);
            // The final rule itself however, is reserved to be done by hand.
            if(!partialInput.empty()) {
                t->input.push_back(partialInput);
            } else {
                cout << "Empty and Partial: " << partialInput << endl;
            }
            delete v_file;
        }
        #ifdef IT_DEBUG
            cerr << "after MakeTasks: " << os->ref_count << endl;
        #endif

        // Fill in the rest...
        // Find out what type the rule is. A function, or an array of commands?
        os->pushValueById(ruleObj->valueID);
        os->getProperty(-1, "build");
        if(os->getTypeStr() == "array") {
            t->type=COMMAND;
            for(int k=0; k<os->getLen(); k++) {
                os->pushNumber(k);
                os->getProperty();
                t->commands.push_back( os->popString().toChar() );
            }
        } else if(os->getTypeStr() == "function") {
            t->type=SCRIPT;
            t->onBuild = new Value::Method(os, ruleObj, "build");
        } else {
            cerr << "[IceTea]: Rule '"<< rule << "' must have it's build property defined as function or array! "
                 << "'" << os->getTypeStr() << "' was given instead." << endl;
        }

        queue->add(t);

        // clean up
        delete key;
        delete val;
        delete input;

        #ifdef IT_DEBUG
            cerr << "Transformer, after | OS' refcount: " << os->ref_count << endl;
        #endif
    }
    return true;
}

// Because Run() is our thread function, it needs some depdendencies
// The needed struct is above.
int CurrentTaskCount=0;
void Run(void* threadData) {
    stringstream s;
    LinePrinter p(&s);
    TaskQueue* tasks = (TaskQueue*)threadData;
    OS* os = tasks->getData();
    Task* task;

    while(!tasks->hasToStop() && tasks->remove(task) && task != NULL) {
        bool canExecute=false;
        for(list<string>::iterator it2=task->input.begin(); it2!=task->input.end(); ++it2) {
            if(!FileExists( it2->c_str() )) {
                // Put the task back in, we cant run it now.
                //tasks->add(task);
            }
            canExecute=true;
            break;
        }
        if(canExecute) {
            CurrentTaskCount++;
            s << "[" << CurrentTaskCount << "/" << tasks->countAll() << "]";
            s << " " << task->targetName << " -> " << task->ruleDisplay << ": ";
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
                (*task->onBuild)(4,0);
                if(os->isArray()) {
                    // We got a set of commands to run.
                } else if(os->isNumber() && os->toBool()) {
                    // Success.
                } else if(os->isNumber() && !os->toBool()) {
                    // Something is wrong.
                }
                os->pop();
                delete g;
            } else {
                // Just run the commands at once.
            }
            //delete task;
        }
    }
}

// Handle errors properly.
void terminator(OS* os) {
    cerr << "--- Terminating ---" << endl;
}

int main(int argc, const char** argv) {
    // Inner globals
    OS* os = OS::create();

    // Make OS silent about creating files.
    os->setSetting(OS_SETTING_CREATE_TEXT_OPCODES,      0);
    os->setSetting(OS_SETTING_CREATE_TEXT_EVAL_OPCODES, 0);
    os->setSetting(OS_SETTING_CREATE_COMPILED_FILE,     0);
    os->setSetting(OS_SETTING_SOURCECODE_MUST_EXIST,    1);

    // Generate a pretty copyright.
    stringstream cpr;
    cpr << "IceTea 0.0.1 by Ingwie Phoenix" << endl
        << OS_COPYRIGHT << endl
        << "TinyThread++ " << TINYTHREAD_VERSION_MAJOR << "." << TINYTHREAD_VERSION_MINOR
        << endl;

    // Arg stuff.
    CLI* cli = new CLI(argc, argv, cpr.str());
    // Fetch thread number beforehand!
    stringstream thrs_sst;
    thrs_sst << thread::hardware_concurrency();
    cli->group("Main options");
    cli->insert("-f", "--file", "<file>", "Select a different built file.", true, "./build.it");
    cli->insert("-b", "--boot", "<file>", "Select a different bootstrap file.", true, "./boot.it");
    cli->insert("-h", "--help", "", "Show this help.");
    cli->insert("-u", "--usage", "", "Show the usage details of a project plus this help.");
    cli->insert("-d", "--dir", "<path>", "Place where to store output files. Default: ./out", "./out");
    cli->insert("-D", "--define", "key=value", "Define a global value within the scripting language's scope.");
    cli->insert(
        "-j", "--jobs", "<N>",
        "Amount of jobs to run at the same time. Default: "+thrs_sst.str(),
        true, thrs_sst.str()
    );
    cli->insert("-t", "--target", "<target>", "Build only the specified target.");
    cli->group("Developer options");
    cli->insert("", "--dump", "", "Dump the internal bootstrap.it to standart output. Save it using redirection: icetea --dump >./build.it");
    cli->insert("", "--debug", "", "Run a debug build; rules and targets may use specialized settings for this build.");
    cli->insert("", "--print-targets", "", "Dump all targets in JSON format to standard error output. Export with: icetea --print-targets 2>targets.json");
    cli->insert("-r", "--dry-run", "", "Don't actually build, but do a dry-run.");
    cli->insert("-v", "--verbose", "", "Commands are shown instead of the progress indicator.");
    cli->insert("-o", "--os", "<file>", "Run an ObjectScript file. Only it will be ran, other options are ignored.");
    cli->insert("-e", "--os-exec", "<str>", "Run ObjectScript code from string, or if - was given, then from standard input.");
    cli->setStrayArgs("Action", "Action to execute. Defaults to: all");
    cli->parse();

    // Fetching values...
    const char* bootstrapit = cli->value("-b").c_str();
    const char* buildit = cli->value("-f").c_str();
    string outputDir = cli->value("-d");

    if(cli->check("-h")) {
        cli->usage();
        goto itCleanup;
    }

#ifdef IT_DEBUG
    cerr << "DEBUG: Before InitIceTeaExt()" << endl;
    cerr << "ObjectScript: ref count -> " << os->ref_count << endl;
#endif

    // Initialize ObjectScript with our stuff.
    initIceTeaExt(os, cli);

    // We have a script file to run.
    if(cli->check("--os")) {
        os->require(cli->value("--os").c_str());
        goto itCleanup;
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
        os->eval(code.c_str());
        goto itCleanup;
    }

    // Pre-check
    if(cli->check("-u")) cli->group("Project options");

    // Introduce bootstrap.it later...

    // Run the script to populate the global objects.
    if(FileExists(buildit)) {
        #ifdef IT_DEBUG
            cerr << "After InitIceTeaExt() and before require(build.it)" << endl;
            cerr << "ObjectScript: ref count -> " << os->ref_count << endl;
        #endif
        OS::StringDef strs[] = {
            {OS_TEXT("__filename"), OS_TEXT(buildit)},
            {OS_TEXT("__dirname"), OS_TEXT(string(StripLastPath(buildit)+"/").c_str())},
            {}
        };
        os->pushGlobals();
        os->setStrings(strs);
        os->pop();
        os->require(buildit);
    } else {
        cerr << "File \"" << cli->value("-f") << "\" does not exist! Aborting." << endl;
        goto itCleanup;
    }

    // init() has added its methods, now its time to put it in.
    cli->parse();

    // Call every target's init() method.
    if(!Initializer(os)) {
        goto itCleanup;
    }

    // Post-check
    if(cli->check("-u")) {
        cli->usage();
        goto itCleanup;
    } else {
        #ifdef IT_DEBUG
            cerr << "Before Preprocessor..." << endl;
            cerr << "ObjectScript: ref count -> " << os->ref_count << endl;
        #endif

        // Run configure and strech inputs
        if(!Preprocessor(os)) goto itCleanup;

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
            goto itCleanup;
        }


        // At this point, we can savely begin to create the needed things for threaded work.
        int thrs;
        stringstream tmp;
        tmp << cli->value("-j");
        tmp >> thrs;
        // Now that we have the thread amount value, lets make it happen.
        #ifdef IT_DEBUG
            cerr << "Initializing with " << thrs << " threads." << endl;
        #endif
        TaskQueue* tasks = new TaskQueue(thrs, Run, os);

        // Now, we've got a waiting task runner there, we ned to feed it one by one.
        if(!Transformer(os, tasks)) {
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
        // Its now save to delete the pool!
        delete tasks;
    }

    #ifdef IT_DEBUG
        cerr << "After everything..." << endl;
        cerr << "ObjectScript: ref count -> " << os->ref_count << endl;
    #endif

    // Clean up.
    itCleanup:
    delete cli;
    delete targets;
    delete rules;
    delete actions;
    os->release();
    return 0;
}
