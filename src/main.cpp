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

// Embed
#include "scripts.rc"
// Little macro to help us out on names
#define INCBIN_DATA(name) (char*)g ## name ## Data
#define INCBIN_LEN(name) g ## name ## Size

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
#define OS_EVAL(_os, code)               _os->eval(code,0,0,OS_SOURCECODE_PLAIN,true,false)
#define OS_EVAL_FAKE_FILE(_os, file, src) _os->evalFakeFile(file,src,0,0,OS_SOURCECODE_PLAIN,true,false)
#define OS_REQUIRE(_os, file)             _os->require(file,true,0,OS_SOURCECODE_PLAIN,true,false)

// FS access
#include "file_system.hpp"
#include "wildcard.hpp"

// Hashing
#include "picosha2.h"

// Misc
#include "predef.h"
#include "cli.h"
#include "util.h"
#include "stlplus_version.hpp"
#include "filecache.hpp"
#include "getMemorySize.h"

// Because I have to
/*
#ifdef __APPLE__
#include <execinfo.h>
#include <signal.h>
#include "stacktrace.h"
void handler(int sig) {
  // print out all the frames to stderr
  cerr << Backtrace() << endl;
  exit(1);
}
#endif
*/

// Namespaces!
using namespace std;
using namespace tthread;
using namespace ObjectScript;
using namespace stlplus;
using namespace picosha2;

OS*        os; ///< Global ObjectScript instance.
CLI*       cli; ///< Global Command Line Interface instance.
Filecache* fc; ///< Global Filecache instance.

/**
    @brief This macro is a shortcut to short-chircuit the main() routine.
*/
#define EXIT_ON_FAILURE()           \
    if(os->isExceptionSet()) {      \
        os->handleException();      \
        return 1;                   \
    } else if(os->isTerminated()) { \
        return 2;                   \
    }

/**
    @brief The main logic

    Everything that is cool happens in here.
*/
int main(int argc, const char** argv) {
    // Just for debugging
    #ifdef __APPLE__
    //signal(SIGSEGV, handler);
    //signal(SIGABRT, handler);
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
    cli->insert("", "--print-rules", "", "Dump all rules in JSON format to standard error output. Export with: icetea --print-rules 2>actions.json");
    cli->insert("-r", "--dry-run", "", "Don't actually build, but do a dry-run.");
    cli->insert("-v", "--verbose", "", "Commands are shown instead of the progress indicator.");
    cli->insert("-o", "--os", "<file>", "Run an ObjectScript file. Only it will be ran, other options are ignored.");
    cli->insert("-e", "--os-exec", "<str>", "Run ObjectScript code from string, or if - was given, then from standard input.");
    cli->insert("-W", "--warn", "", "Display warnings.");
    cli->group("Build options");
    cli->insert(
        "-w", "--wipe", "",
        "Sets the build scheme to 'clear'. Will delete output files."
    );
    cli->insert("","--scheme","","Set a custom scheme.");
    cli->insert("-c", "--configure", "", "Only configure the specified projects and do not build.");
    cli->insert("-g", "--debug", "", "Turn on the global DEBUG variable (set it to true)");
    cli->setStrayArgs("Action", "Action to execute. Defaults to: all");
    cli->parse();

    // Fetching values...
    string bootstrapit = cli->value("-b");
    string buildit = cli->value("-f");
    string outputDir = cli->value("-d");

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
        return 0;
    }

    // Generic
    OS::StringDef strs[] = {
        {OS_TEXT("__buildit"),     OS_TEXT(buildit.c_str())},
        {OS_TEXT("__bootstrapit"), OS_TEXT(bootstrapit.c_str())},
        {OS_TEXT("__outputdir"),   OS_TEXT(outputDir.c_str())},
        {OS_TEXT("__cachefile"),   OS_TEXT(cacheFile.c_str())},
        {OS_TEXT("__sourcedir"),   OS_TEXT(folder_current_full().c_str())},
        {}
    };
    os->pushGlobals();
    os->setStrings(strs);
    os->pop();

    // Initialize ObjectScript with our stuff.
    initIceTeaExt(os, cli);
    initializeDetector(os, fc, cli);
    initializeStdlib(os);
    OS_EVAL_FAKE_FILE(os, "Configurable.os", INCBIN_DATA(Configurable));
    OS_EVAL_FAKE_FILE(os, "IceTea.os", INCBIN_DATA(libIceTea));
    OS_EVAL_FAKE_FILE(os, "main.os", INCBIN_DATA(main));

    if(cli->check("-g")) {
        OS_EVAL(os, "_G.DEBUG=true");
    } else {
        OS_EVAL(os, "_G.DEBUG=false");
    }

    // Run the script to populate the global objects.
    if(file_exists(bootstrapit)) {
        OS_REQUIRE(os, bootstrapit.c_str());
        EXIT_ON_FAILURE()
    }

    // Auto-include
    if(folder_exists(".IceTea")) {
        // Get all *.it files
        vector<string> itFiles = folder_wildcard(".IceTea", "*.it", false, true);
        for(vector<string>::iterator it=itFiles.begin(); it!=itFiles.end(); ++it) {
            string full = create_filespec(".IceTea", *it);
            OS_REQUIRE(os, full.c_str());
            EXIT_ON_FAILURE()
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

    if(file_exists(buildit)) {
        OS_REQUIRE(os, buildit.c_str());
        EXIT_ON_FAILURE()
    } else {
        if(!cli->check("--os") && !cli->check("--os-exec")) {
            cerr << "File \"" << cli->value("-f") << "\" does not exist! Aborting." << endl;
            return 1;
        }
    }

    // We have a script file to run.
    if(cli->check("--os")) {
        OS_REQUIRE(os, cli->value("--os").c_str());
        EXIT_ON_FAILURE()
        return 0;
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
        OS_EVAL(os, code.c_str());
        EXIT_ON_FAILURE()
        return 0;
    }

    // new code here
    os->getGlobal("main");
    os->newArray(argc);
    for(int i = 0; i<argc; i++) {
        os->pushString(argv[i]);
        os->addProperty(-2);
    }
    os->callF(1,1);

    EXIT_ON_FAILURE()

    // Prepare return value
    OS::String rt_typeid = os->getTypeStr();
    int c = 0;
    if(rt_typeid == "number") {
        c = os->popNumber();
    }
    return c;
}
