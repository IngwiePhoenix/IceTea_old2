#include <iostream>
#include <vector>
#include <map>
#include <stdlib.h>

#include "objectscript.h"
#include "IceTea.h"

#include "cli.h"
#include "filecache.hpp"
#include "tinythread.h"
#include "portability_fixes.hpp"
#include "stlplus_version.hpp"
#include "rlutil.h"
#include "PluginStore.h"

// Embed
#include "scripts.rc"
// Little macro to help us out on names
#define INCBIN_DATA(name) g ## name ## Data
#define INCBIN_LEN(name) g ## name ## Size

using namespace ObjectScript;
using namespace std;
using namespace stlplus;
using namespace tthread;

bool callObjectFunction(
    IceTea* self,
    string objName, string funcName,
    int args=0, int expected=0,
    bool cast = false
) {
    self->getGlobalObject(objName.c_str());
    self->pushString(funcName.c_str());
    self->getProperty(-2);

    // Sanity check
    if(self->getTypeStr() != "function") {
        self->pop();
        return false;
    }

    self->callF(args, expected);
    return (cast && expected > 0 ? self->popBool() : true);
}

void performIceTeaEvent(IceTea* it, const string& name, int expected=0) {
    it->getGlobalObject("IceTea");
    it->pushString("CallEvent");
    it->getProperty(-2);
    it->pushString(name.c_str());
    it->callF(1,expected);
}

IceTea::IceTea() : OS() {
    // Fetch thread number beforehand!
    thrs_sst << thread::hardware_concurrency();

    // Should we debug?
    const char* envDebug = getenv("DEBUG");
    this->setDebug(envDebug != NULL);
}

IceTea* IceTea::create(OS::MemoryManager* manager) {
    return OS::create(new IceTea(), manager);
}

void IceTea::initSettings() {
    // Make OS silent about creating files.
    this->setSetting(OS_SETTING_CREATE_TEXT_OPCODES,      0);
    this->setSetting(OS_SETTING_CREATE_TEXT_EVAL_OPCODES, 0);
    this->setSetting(OS_SETTING_CREATE_COMPILED_FILE,     0);
    this->setSetting(OS_SETTING_SOURCECODE_MUST_EXIST,    1);
}

IceTea::~IceTea() {
    delete this->cli;
    delete this->fc;
    // OS::~OS();
}

void IceTea::setupArguments() {
    this->cli->group("Main options");
    {
        this->cli->insert("-f", "--file", "<file>", "Select a different built file.", true, "./build.it");
        this->cli->insert("-b", "--boot", "<file>", "Select a different bootstrap file.", true, "./bootstrap.it");
        this->cli->insert("-C", "--chdir", "<dir>", "Change to this directory before doing anything.");
        this->cli->insert("-h", "--help", "", "Show this help.");
        this->cli->insert("-u", "--usage", "", "Show the usage details of a project plus this help.");
        this->cli->insert("-d", "--dir", "<path>", "Place where to store output files. Default: ./out", true, "./out");
        this->cli->insert("-D", "--define", "key=value", "Define a global value within the scripting language's scope.");
        this->cli->insert(
            "-j", "--jobs", "<N>",
            "Amount of jobs to run at the same time. Default: "+thrs_sst.str(),
            true, thrs_sst.str()
        );
        this->cli->insert("-p", "--purge", "", "Purge the cache file.");
        this->cli->insert("-t", "--target", "<target>", "Build only the specified target.");
    }

    this->cli->group("Display options");
    {
        this->cli->insert("", "--no-color", "", "Disable colors.");
        this->cli->insert("", "--detail-output", "", "Force detailed output.");
    }

    this->cli->group("Developer options");
    {
        this->cli->insert("", "--dump", "", "Dump the internal bootstrap.it to standart output. Save it using redirection: icetea --dump >./build.it");
        this->cli->insert("", "--print-targets", "", "Dump all targets in JSON format to standard error output. Export with: icetea --print-targets 2>targets.json");
        this->cli->insert("", "--print-actions", "", "Dump all actions in JSON format to standard error output. Export with: icetea --print-actions 2>actions.json");
        this->cli->insert("", "--print-rules", "", "Dump all rules in JSON format to standard error output. Export with: icetea --print-rules 2>actions.json");
        this->cli->insert("-r", "--dry-run", "", "Don't actually build, but do a dry-run.");
        this->cli->insert("-v", "--verbose", "", "Commands are shown instead of the progress indicator.");
        this->cli->insert("-x", "--os", "<file>", "Run an ObjectScript file. Only it will be ran, other options are ignored.");
        this->cli->insert("-e", "--exec", "<str>", "Run ObjectScript code from string, or if - was given, then from standard input.");
        this->cli->insert("-W", "--warn", "", "Display warnings.");
    }

    this->cli->group("Build options");
    {
        this->cli->insert(
            "-w", "--wipe", "",
            "Sets the build scheme to 'clear'. Will delete output files."
        );
        this->cli->insert("","--scheme","","Set a custom scheme.");
        this->cli->insert("-c", "--configure", "", "Only configure the specified projects and do not build.");
        this->cli->insert("-g", "--debug", "", "Turn on the global DEBUG variable (set it to true)");
        this->cli->insert("-F", "--force", "", "Run without a mandatory build.it file.");
    }

    this->cli->setStrayArgs("Action", "Action to execute. Defaults to: all");

    this->cli->parse();
}

bool IceTea::initializeModules() {
    // Print the modules, for debugging.
    PluginList* plugins = PluginStore::get();
    PluginList::iterator it;
    for(it = plugins->begin(); it!=plugins->end(); ++it) {
        (*it).cb(this);
    }

    // Initialize the scripted modules
    // FIXME: Move into Modules.
    struct ScriptMap_t {
        const char*             name;
        const unsigned char*    script;
        int                     len;
    };
    #define _s(file, symbol) {"(internal):" file, INCBIN_DATA(symbol), INCBIN_LEN(symbol)}
    ScriptMap_t scripts[] = {
        _s("std.os", STD),
        _s("underscore.os", Underscore),
        _s("configurable.os", Configurable),
        _s("IceTea.os", libIceTea),
        _s("detect.os", Detector),
        _s("detect.utils.os", DetectorUtils),
        {}
    };
    ScriptMap_t* list = &scripts[0];

    while(list->name && list->script) {
        string script(reinterpret_cast<const char*>(list->script), list->len);
        this->evalFakeFile(list->name, script.c_str());
        if(this->hasEndedExecuting()) return false;
        list++;
    }

    if(this->shouldDebug) {
        this->eval("_G.DEBUG=true");
    } else {
        this->eval("_G.DEBUG=false");
    }

    performIceTeaEvent(this, "beforeInitialize");
    if(this->hasEndedExecuting()) return false;

    if(file_exists(this->bootstrapit)) {
        this->require(this->bootstrapit);
    } else {
        this->printDebug("No bootstrap.it file found. Using internal.");
        this->evalFakeFile(
            "(internal):bootstrap.it",
            INCBIN_DATA(InternalBootstrapIt),
            INCBIN_LEN(InternalBootstrapIt)
        );
    }
    if(this->hasEndedExecuting()) return false;

    performIceTeaEvent(this, "afterInitialize");
    if(this->hasEndedExecuting()) return false;

    return true;
}

void IceTea::setDebug(bool debug) {
    this->shouldDebug = debug;
}

bool IceTea::getDebug() {
    return this->shouldDebug;
}

void IceTea::printDebug(string msg) {
    if(this->shouldDebug || this->cli->check("--debug")) {
        if(!cli->check("--no-color")) {
            rlutil::saveDefaultColor();
            rlutil::setColor(rlutil::GREY);
            cout << "[IceTea]: " << msg << endl;
            rlutil::resetColor();
        } else {
            cout << "[IceTea]: " << msg << endl;
        }
    }
}

IceTea* IceTea::setupCli(vector<string> strings) {
    int argc=strings.size(), i=0;
    const char* argv[argc];
    vector<string>::iterator it;
    for(it=strings.begin(); it != strings.end(); ++it) {
        argv[i++] = it->c_str();
    }

    // Pass the char** to the original method.
    return this->setupCli(argc, argv);
}

IceTea* IceTea::setupCli(int argc, const char** argv) {
    // Generate a pretty copyright.
    cpr << "IceTea 0.2.0 by Ingwie Phoenix" << endl
        << OS_COPYRIGHT << endl
        << "TinyThread++ " << TINYTHREAD_VERSION_MAJOR << "." << TINYTHREAD_VERSION_MINOR << endl
        << "stlplus " << stlplus::version() << endl;

    // Arg stuff.
    this->cli = new CLI(argc, argv, cpr.str());
    setupArguments();

    // Fetching values...
    this->bootstrapit = this->cli->value("-b");
    this->buildit = this->cli->value("-f");
    this->outputDir = this->cli->value("-d");
    this->cacheFile = create_filespec(this->outputDir, ".cache.it");


    const char* env_boot = getenv("ICETEA_BOOTSTRAP");
    if(env_boot != NULL) {
        this->bootstrapit = env_boot;
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
    this->pushGlobals();
    this->setStrings(strs);
    this->pop();

    return this;
}

// Helper methods
CLI* IceTea::getCliHandle()         { return this->cli; }
void IceTea::reparseCli()           { this->cli->parse(); }
Filecache* IceTea::getFilecache()   { return this->fc; }

bool IceTea::checkAndRunInline(int &rt) {
    if(this->cli->check("--os") || this->cli->check("--exec")) {
        // Execute an inline script - file, or string.
        if(this->cli->check("--os")) {
            this->require(this->cli->value("--os"));
        } else if(this->cli->check("--exec")) {
            // Or a string.
            string code;
            if(this->cli->value("--exec") == "-") {
                cerr << "Press CTRL+D or end input to STDIN to run the code." << endl;
                string line;
                while(getline(cin, line).good()) {
                    code += line;
                }
            } else {
                code = this->cli->value("--exec");
            }
            this->eval(code, 1);
        }
        this->hasEndedExecuting(rt);
        cout << this->toString() << endl;
        return true;
    }

    // No inline script to be run.
    return false;
}

bool IceTea::checkAndRunHelp() {
    if(this->cli->check("--help")) {
        this->cli->usage();
        return true;
    }
    return false;
}

// Handle termination and exceptions
bool IceTea::hasEndedExecuting(int &rtVal) {
    if(this->isExceptionSet()) {
        this->handleException();
        rtVal = 1;
        return true;
    } else if(this->isTerminated()) {
        rtVal = 2;
        return true;
    }

    // Nothing happened.
    return false;
}

bool IceTea::hasEndedExecuting() {
    int dummy;
    return hasEndedExecuting(dummy);
}

void IceTea::require(string file) {
    OS::require(file.c_str(), true, 0, OS_SOURCECODE_PLAIN, true, false);
}


void IceTea::eval(string code) {
    this->eval(code, 0);
}
void IceTea::eval(string code, int expected) {
    OS::eval(code.c_str(), 0, expected, OS_SOURCECODE_PLAIN, true, false);
}


void IceTea::evalFakeFile(string fileName, const unsigned char* src, int len) {
    string script(reinterpret_cast<const char*>(src), len);
    this->evalFakeFile(fileName.c_str(), script.c_str());
}
void IceTea::evalFakeFile(const char* name, const char* source) {
    OS::evalFakeFile(name, source, 0, 0, OS_SOURCECODE_PLAIN, true, false);
}

bool IceTea::isBool(int offs) {
    return this->getType(offs) == OS_VALUE_TYPE_BOOL;
}
bool IceTea::isBool() {
    return this->getType() == OS_VALUE_TYPE_BOOL;
}

bool IceTea::startup() {
    static bool hasSetup = false;

    if(hasSetup) return hasSetup; // Dont set up twice!

    // Output folder
    folder_create(this->outputDir);

    // Cache
    this->cli->check("-p") && file_delete(this->cacheFile);
    this->fc = new Filecache(this->cacheFile);

    // First, load the native modules.
    if(!this->initializeModules()) {
        // Something did not initiaize.
        // We can't work with a half-initialized environment.
        return 1;
    }

    if(file_exists(this->buildit)) {
        this->require(this->buildit);
        if(this->hasEndedExecuting()) return false;
    } else {
        if(!this->cli->check("--force")) {
            cerr << "No build.it file found. Please create one, or use --force/-F to go without one." << endl;
            this->cli->usage();
            return false;
        }
    }

    hasSetup = true;
    return hasSetup;
}

int IceTea::run() {
    // Return code tracker
    int rt = 0;

    // Setup everything.
    if(!this->startup()) {
        this->printDebug("Startup failed.");
        return -1;
    }

    // Initialize all the targets, rules, actions, and possibly more.
    callObjectFunction(this, "IceTea", "Initializer", 0, 0, false);
    if(this->hasEndedExecuting(rt)) {
        this->printDebug("Initializer failed.");
        return rt;
    } else {
        rt = this->popNumber();
    }

    if(this->checkAndRunHelp()) {
        // We ran the help. Exit with 0.
        return 0;
    } if(this->checkAndRunInline(rt)) {
        // We ran an inline script, return.
        return rt;
    }

    // Is there anything else?
    performIceTeaEvent(this, "postprocessCli", 1);
    if(this->hasEndedExecuting(rt)) {
        this->printDebug("event:postprocessCli failed.");
        return rt;
    }

    bool shouldContinue = this->isType(OS_VALUE_TYPE_BOOL) ? this->popBool() : false;
    if(!shouldContinue) {
        // We did something in postprocessCli, so we should exit.
        this->printDebug("Closing after event:postprocessCli");
        return 0;
    }

    // If we came thus far, then we really want to configure, stack and run.
    callObjectFunction(this, "IceTea", "Main");
    if(this->hasEndedExecuting(rt)) {
        this->printDebug("Main failed.");
        return rt;
    } else {
        rt = this->popNumber();
    }

    return rt;
}
