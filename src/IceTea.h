#ifndef ICETEA_IMPL_H
#define ICETEA_IMPL_H

#include <string>
#include <vector>
#include <fstream>

#include "objectscript.h"
#include "cli.h"
#include "filecache.hpp"

#include "Pluma.hpp"
#include "IceTeaPlugin.h"

#include "it-version.h"

// File related things
#define ICETEA_SCRIPT_EXT ".it"
#define ICETEA_TEMPLATE_EXT ".in"

// Copyright and such.
#define ICETEA_AUTHOR "Kevin \"Ingwie Phoenix\" Ingwersen <ingwie2000@gmail.com>"
#define ICETEA_VERSION_STR \
    "IceTea " ICETEA_VERSION \
    "by " ICETEA_AUTHOR

class IceTea : public ObjectScript::OS {
// Aliases:
    typedef std::string string;
    typedef std::stringstream sstream;
    typedef pluma::Pluma Pluma;
    typedef std::vector<IceTeaPlugin*> ITPlugins;

private:
    sstream     cpr;        ///< Copyright string
    CLI*        cli;        ///< Command Line Interface instance.
    Filecache*  fc;         ///< Filecache instance.
    sstream     thrs_sst;   ///< A stringstream, containing the number of default threads.
    string      bootstrapit;///< Path to a bootstrap.it file, empty of to use internal.
    string      buildit;    ///< Path to a build.it file. Required.
    string      outputDir;  ///< Path to the putput folder.
    string      cacheFile;  ///< Path to the file containing the cache.
    bool        shouldDebug;///< Should we print debug messages?
    Pluma       manager;    ///< Plugin manager
    ITPlugins   plugins;    ///< Internal storage of all loaded, compiled-in plugins.

    // Setup CLI arguments.
    void setupArguments();

    // Initialize the various IceTea modules.
    bool initializeModules();

public:

    // Constructor and destructor
    IceTea();
    ~IceTea();
    static IceTea* create(ObjectScript::OS::MemoryManager* = NULL);

    // Implementation of virtual methods
    void initSettings();

    // Enable or disable debug.
    void setDebug(bool);
    bool getDebug();

    // Print a message if debugging is enabled.
    void printDebug(std::string);

    // Setup the CLI by arguments.
    IceTea* setupCli(std::vector<std::string>);
    IceTea* setupCli(int, const char**);

    // Get the CLI handle. Useful if not using setupCli()
    CLI* getCliHandle();

    // Make the CLI re-parse. Call this after adding args!
    void reparseCli();

    // Get the filecache.
    Filecache* getFilecache();

    // Check and run an inline script.
    bool checkAndRunInline(int&);

    // Decide to run the help menu or not.
    bool checkAndRunHelp();

    // Listing for the modules
    bool checkAndListModules(int&);

    // Handle termination and exceptions
    bool hasEndedExecuting(int&);
    bool hasEndedExecuting();

    // Minimalistic version of require() to open up files and run them.
    void require(std::string);

    // Minimalistic version of eval()
    void eval(std::string);
    void eval(std::string, int);

    // Minimalistic version of evalFakeFile()
    void evalFakeFile(std::string, const unsigned char[], int len);
    void evalFakeFile(const char*, const char*);

    // OS is rightout missing this one xD
    bool isBool(int offs);
    bool isBool();

    // Setup the environment, load stuff and such.
    // Also creates the output folder and cache file!
    bool startup();

    // Run the main code - building, installing or cleaning.
    void runMain(int&);

    // Run IceTea. Will be split further later.
    int run();
};

#endif // ICETEA_IMPL_H
