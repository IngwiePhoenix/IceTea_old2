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
#include "APConsoleLib.h"
#include "APHelper.h"

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
    vector<string> files;
    string output;

    TaskType type;
    // [ cmd1, cmd2, ... ]
    vector<string> commands;
    // Rule.build: function(input, output, target)
    // The function is always called with only one input, but access to the
    // target is given for settings - such as include dirs and alike.
    Value::Method* onBuild;

    // The related target, pushed to the function.
    Value::Object* target;
};
typedef WorkQueue<Task*, OS*> TaskQueue;

// Since OS is not thread safe, we have to make thread-safe accessors
bool obj_access(string index, Value::Object* obj, Value::Var& output) {
    static tthread::mutex m;
    tthread::lock_guard<tthread::mutex> guard(m);
    cout << "o.o" << endl;
    return true;
}

// Tool functions
void getFileList(string pattern, list<string> &rt_list) {
    StringList fList;
    string dirName = StripLastPath(pattern);
    if(dirName == "" || dirName == ".") dirName = "./";
    GetFileList(dirName.c_str(), fList);
    for(StringList::iterator j=fList.begin(); j != fList.end(); j++) {
        if(WildcardMatch((j->c_str()), PathToFileName(pattern.c_str()))) {
            rt_list.push_back(dirName + *j);
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

// In order to call OS savely, we use this.
tthread::mutex OSMutex;
void lockOS() { OSMutex.lock(); }
void unlockOS() { OSMutex.unlock(); }

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
    for(int i=os->getLen(); i>0; i--) {
        os->nextIteratorStep();
        Value::String key(os, -2);
        Value::Object val(os);
        os->getProperty("_"); // Guarantee'd to exist.
        Value::String ruleName(os);
        if(!ruleExists(&ruleName, os)) {
            cerr << "Target \"" << key << "\" refferences to non-existing rule \"" << ruleName << "\"." << endl;
            return false;
        }
        os->pushValueById(val.valueID);
        os->pushString("init");
        if(os->in(-1, -2)) {
            os->popString();
            os->getProperty("init");
            if(os->isFunction()) {
                Value::Method mt(os, &val, "init");
                mt(0,0);
            } else {
                cerr << "[IceTea]: The init() method in " << key << "is not a function! Found "
                     << os->getTypeStr().toChar() << " instead.";
            }
        } else {
            cerr << "IceTea: Error detected. Stack:" << endl
                 << "    -1: " << os->getTypeStr(-1).toChar() << " --> " << os->toString(-1).toChar() << endl
                 << "    -2: " << os->getTypeStr(-2).toChar() << " --> " << os->toString(-2).toChar() << endl
                 << "    -3: " << os->getTypeStr(-3).toChar() << " --> " << os->toString(-3).toChar() << endl
                 << "    -4: " << os->getTypeStr(-4).toChar() << " --> " << os->toString(-4).toChar() << endl
                 << "    -5: " << os->getTypeStr(-5).toChar() << " --> " << os->toString(-5).toChar() << endl
                 << endl;
        }
        // Cause IceTea to exit
        if(os->isExceptionSet()) return false;
    }
    return true;
}
bool Preprocessor(OS* os) {
    os->pushValueById(targets->valueID);
    for(int i=os->getLen(); i>0; i--) {
        os->nextIteratorStep();
        Value::String key(os, -2);
        Value::Object val(os);
        os->pushString("configure");
        if(os->in(-1, -2)) {
            os->popString();
            os->getProperty("configure");
            if(os->isFunction()) {
                Value::Method mt(os, &val, "configure");
                mt(0,0);
            } else {
                cerr << "[IceTea]: The configure() method in " << key << "is not a function! Found "
                     << os->getTypeStr().toChar() << " instead.";
            }
        }
        os->pop(); // string: configure

        // Now strech the input parameter.
        Value::Array* input = (Value::Array*)val["input"];
        list<string> fileList;
        getFileListRec(input, fileList);
        os->getProperty("input");
        os->newArray(); // Overwrite the previous
        for(list<string>::iterator it=fileList.begin(); it!=fileList.end(); ++it) {
            os->pushString((*it).c_str());
            os->addProperty(-2);
        }
        os->pop();

        // Cause IceTea to exit
        if(os->isExceptionSet()) return false;
    }
    return true;
}
bool Transformer(OS* os, TaskQueue* queue) {
    // The following blob is to emulate a possible, incomplete build logic.
    /*
    // Now that it has ran, it's time to take stuff apart.
    Value::Object* icetea = (Value::Object*)(*targets)["IceTea"];
    Value::Array* files = (Value::Array*)(*icetea)["input"];
    list<string> fileList;
    getFileListRec(files, fileList);
    delete files;

    // Let's try to call IceTea's configure
    Value::Method cg(os, icetea, "configure");
    cg(0,0);
    // Now lets try to build it...staticaly for now.
    Value::Object* cxx = (Value::Object*)(*rules)["cxx"];
    Value::Method cxx_p(os, cxx, "prepare");
    Value::Method cxx_b(os, cxx, "build");
    Value::Object* exe = (Value::Object*)(*rules)["exe"];
    Value::Method exe_p(os, exe, "prepare");
    Value::Method exe_b(os, exe, "build");
    // Call the two preparational methods first.
    cxx_p(0,0);
    exe_p(0,0);
    // Now we "build"
    os->newArray();
    for(list<string>::iterator ft=fileList.begin(); ft!=fileList.end(); ++ft) {
        os->pushString( (ft->c_str()) );
        os->addProperty(-2);
    }
    cxx_b(1,0);
    os->newArray();
    for(list<string>::iterator ft2=fileList.begin(); ft2!=fileList.end(); ++ft2) {
        os->pushString( (ft2->c_str()) );
        os->addProperty(-2);
    }
    exe_b(1,0);
    delete cxx;
    delete exe;
    delete icetea;
    */

    // And now lets get serious.
    // We only must push "safe" targets into the queue...
    lockOS(); // We are prepping targets. It must be completely locked first.
    os->pushValueById(targets->valueID);
    for(int i=os->getLen(); i>0; i--) {
        os->nextIteratorStep();
        Value::String key(os, -2);
        Value::Object val(os);
        Value::Array* input = (Value::Array*)val["input"];
        Value::String* ruleName = (Value::String*)val["_"];
        for(int j=input->length(); j>0; j--) {
            // We walk the files backwards, its easier to maintain.
            Value::String* v_file = (Value::String*)(*input)[j];
            string file = (*v_file);
            string rule = (*ruleName);
            // Now we need to resolve the rules.
        }
    }
    unlockOS();
    return false;
}

// Because Run() is our thread function, it needs some depdendencies
// The needed struct is above.
void Run(void* threadData) {
    TaskQueue* tasks = (TaskQueue*)threadData;
    OS* os = tasks->data["os"];
    Task* task;
    while(!tasks->hasToStop()) {
        if(tasks->remove(task)) {
            bool doContinue=true;
            for(int e=0; e>task->files.size(); e++) {
                if(!FileExists( task->files[e].c_str() )) {
                    // Put the task back in, we cant run it now.
                    tasks->add(task);
                    doContinue=false;
                }
            }
            if(doContinue) {
                if(task->type == SCRIPT) {
                    lockOS();
                    // ...
                    unlockOS();
                } else {
                    // Just run the commands at once.
                }
            }
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

    // Arg stuff.
    CLI* cli = new CLI(argc, argv, "IceTea by Ingwie Phoenix");
    // Fetch thread number beforehand!
    stringstream thrs_sst;
    thrs_sst << thread::hardware_concurrency();
    cli->group("Main options");
    cli->insert("-f", "--file", "<file>", "Select a different built file.", true, "./build.it");
    cli->insert("-b", "--boot", "<file>", "Select a different bootstrap file.", true, "./boot.it");
    cli->insert("-h", "--help", "", "Show this help.");
    cli->insert("-u", "--usage", "", "Show the usage details of a project plus this help.");
    cli->insert("-d", "--define", "key=value", "Define a global value within the scripting language's scope.");
    cli->insert("", "--debug", "", "Run a debug build; rules and targets may use specialized settings for this build.");
    cli->insert("-v", "--verbose", "", "Commands are shown instead of the progress indicator.");
    cli->insert(
        "-j", "--jobs", " ",
        "Amount of jobs to run at the same time. Default: "+thrs_sst.str(),
        true, thrs_sst.str()
    );
    cli->insert("-t", "--target", "<target>", "Build only the specified target.");
    cli->parse();

    if(cli->check("-h")) {
        cli->usage();
        goto itCleanup;
    }

    // Initialize ObjectScript with our stuff.
    initIceTeaExt(os, cli);

    // Pre-check
    if(cli->check("-u")) cli->group("Project options");

    // Introduce bootstrap.it later...

    // Run the script to populate the global objects.
    if(FileExists(cli->value("-f").c_str()))
        os->require(cli->value("-f").c_str());
    else {
        cerr << "File \"" << cli->value("-f") << "\" does not exist! Aborting." << endl;
        goto itCleanup;
    }

    // Call every target's init() method.
    if(!Initializer(os)) {
        goto itCleanup;
    }

    // Post-check
    if(cli->check("-u")) {
        cli->usage();
        goto itCleanup;
    } else {
        // Run configure and strech inputs
        if(!Preprocessor(os)) goto itCleanup;

        // At this point, we can savely begin to create the needed things for threaded work.
        int thrs;
        stringstream tmp;
        tmp << cli->value("-j");
        tmp >> thrs;
        // Now that we have the thread amount value, lets make it happen.
        TaskQueue* tasks = new TaskQueue(thrs, Run);

        // Save our OS instance to the thread stuff.
        tasks->data["os"] = os;

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

    // Clean up.
    itCleanup:
    delete cli;
    delete targets;
    delete rules;
    delete actions;
    os->release();
    return 0;
}
