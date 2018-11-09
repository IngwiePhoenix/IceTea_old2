/**
    @file
    @brief A header-only command line argument parsing library.

    This small header contains only one class: @ref CLI

    This class can handle command line options - long and short - as well as
    setting and retriving default values.
*/
#ifndef CLI_H
#define CLI_H

#include <map>
#include <list>
#include <vector>
#include <iostream>
#include <sstream>

/**
    @brief Command Line Interface class.

    This class is responsible for handling a command line.
*/
class CLI {
private:
    /**
        @brief Information about an option

        This struct simply holds information about an otpion, which is part
        of the command line string that was given to @ref CLI 's constructor.
    */
    struct Option {
        std::string shortopt; ///< Short option (I.e.: `-h`)
        std::string longopt;  ///< Long option (I.e.: `--help`)
        std::string arg; ///< Short argument description for this option
        std::string desc; ///< Full description to be shown in the help view
        bool optional; ///< For core
        bool present; ///< For core
        std::string value; ///< The value held by the option
    };
    /// A list of options
    typedef std::vector<Option*> OptList;
    /// Mapping groups and options along
    typedef std::map<std::string, OptList> OptGroups;
    /// Holds all groups with their options
    OptGroups ga;
    /// Argument snot associated to any option
    std::vector<std::string> strayArgs;
    /// A list to keep options in line.
    std::list<std::string> orderedOpts;
    /// The original `argc`
    int argc;
    /// The original `argv`
    const char** argv;
    /// A copyright string for the top of the display
    std::string copyright;
    /// For core.
    int leftLength;
    /// For core.
    std::string lastGroup;
    /// Short description for what the stray args are for.
    std::string strayArgsArg;
    /// A longer descriptions for the stray args.
    std::string strayArgsDesc;
    /// Wether the stray args are optional or not.
    bool strayArgsOptional;
public:
    /**
        @brief Initialize the CLI object.

        This constructor takes as input the original `argc` and `argv` and
        an additional copyright string that it will print to the head of the
        usage display.

        @param _argc Argument count
        @param _argv The argument array to parse
        @param _copyright The copyright to be displayed.
    */
    inline CLI(int _argc, const char** _argv, std::string _copyright="") {
        argc=_argc;
        argv=_argv;
        strayArgsDesc="";
        strayArgsArg="";
        strayArgsOptional=true;
        copyright=_copyright;
        leftLength=30;
        lastGroup="";
    }

    /// Destructs @ref CLI and frees the memory.
    inline ~CLI() {
        for(OptGroups::iterator it=ga.begin(); it!=ga.end(); ++it) {
            for(unsigned int ot=0; ot<it->second.size(); ++ot) {
                delete it->second[ot];
            }
        }
    }

    /// Set the width by which arguments are idendet.
    inline void setArgWidth(int len) { leftLength = len; }

    /**
        @brief Add an option.

        This function inserts an option into the current group and saves the
        given information.

        @param shortopt The short option to be used. Empty to have none.
        @param longopt The long option to be used. Empty to have none.
        @param arg A short description what the option's value is for. I.e.: `<DIR>`
        @param desc A description of the option.
        @param optional if true, then this option will be displayed as an optional parameter.
        @param defaultValue The value that should be present by default.

        @note You need either shortopt or longopt - or both. But you can not leave both empty.
        @note Only if you supplied arg, you will be able to pull a value.
    */
    inline void insert(
        std::string shortopt, std::string longopt,
        std::string arg, std::string desc, bool optional=true, std::string defaultValue=""
    ) {
        Option* opt = new Option;
        opt->shortopt=shortopt;
        opt->longopt=longopt;
        opt->arg=arg;
        opt->desc=desc;
        opt->optional=optional;
        opt->present=false;
        opt->value=defaultValue;

        // Insert into last map entry
        if (!lastGroup.empty()) {
            ga[lastGroup].push_back(opt);
        } else {
            ga["Options"].push_back(opt);
            orderedOpts.push_back("Options");
        }
    }

    /// Set a new group.
    /// @param name The group name
    inline void group(std::string name) {
        lastGroup=name;
        orderedOpts.push_back(name);
    }

    /**
        @brief Set the information for stray arguments

        This will configure the stray arguments - similar to @ref insert.

        @param arg The short argument description.
        @param desc A proper description
        @param opitonal Wether stray args are optional or not.
    */
    inline void setStrayArgs(std::string arg, std::string desc, bool optional=true) {
        strayArgsDesc=desc;
        strayArgsArg=arg;
        strayArgsOptional=optional;
    }

    /// Obtain all the stray args that were found.
    /// @returns A vector of all stray args.
    std::vector<std::string> getStrayArgs() { return strayArgs; }

    /// For core.
    std::pair<std::string, int> findOpt(std::string name) {
        for(OptGroups::iterator it=ga.begin(); it!=ga.end(); ++it) {
            OptList opl = it->second;
            for (unsigned int opt = 0; opt < opl.size(); opt++) {
                if (!opl[opt]->shortopt.compare(name) ||
                    !opl[opt]->longopt.compare(name))
                        return std::pair<std::string, int>(it->first, opt);
            }
        }
        return std::pair<std::string, int>("", -1);
    }

    /// Tell the instance to parse the given argc and argv.
    inline bool parse() {
        // This has to be done to avoid multiples if we parse multiple times.
        strayArgs.clear();
        int argn;
        std::pair<std::string, int> opt;
        for(argn=1; argn < argc; argn++) { /* Read command line. */
            opt = findOpt(argv[argn]);
            if(opt.second!=-1) { /* This argument is an option switch. */
                if(ga.find(opt.first)!=ga.end() && !ga[opt.first][opt.second]->arg.empty()) { /* It takes an argument. */
                    if(argn + 1 < argc) {
                        ga[opt.first][opt.second]->value = argv[argn+1]; /* Add next argument as the option value. */
                        argn++;
                    } else return false;
                }
                ga[opt.first][opt.second]->present = true; /* Tell the option it exists. */
            } else if(argv[argn][0] == '-') /* Nonexistent option. */
    #ifdef __APPLE__
                /* Mac OSX 10.7 sends some arguments to its applications on launch. Ignore these. */
                continue;
    #else
                return false;
    #endif
            else /* This argument is a stray. */
                strayArgs.push_back(argv[argn]);
        }
        return true;
    }

    /// Check if an option (by its short- or longopt) is given.
    /// @param longopt A short OR longopt.
    /// @returns True if it exists.
    inline bool check(std::string longopt) {
        std::pair<std::string,int> opt = findOpt(longopt);
        if (opt.second!=-1)
            return ga[opt.first][opt.second]->present;
        else
            return false;
    }

    /// Get the value that is given to an option.
    /// @param optname The option's name.
    /// @returns The Value. Always a string.
    inline std::string value(std::string optname) {
        std::pair<std::string,int> opt = findOpt(optname);
        if (opt.second!=-1)
            return ga[opt.first][opt.second]->value;
        else
            return std::string("");
    }

    // Now to the formatting
    /// Print a usage screen.
    inline void usage() {
        std::stringstream myout;
        if(copyright!="") std::cout << copyright << std::endl;
        std::cout << "Usage: " << argv[0];
        for(std::list<std::string>::iterator it=orderedOpts.begin(); it!=orderedOpts.end(); ++it) {
            std::string groupName = *it;
            OptList ol = ga[groupName];
            if(ol.empty()) continue;
            myout << std::endl << groupName << ":" << std::endl;
            for(int iopt=0; iopt<ol.size(); iopt++) {
                Option* opt = ol[iopt];
                if(!opt->shortopt.empty()) {
                    if(opt->optional)
                        std::cout << " [" << opt->shortopt << (!opt->arg.empty()?" "+opt->arg:"") << "]";
                    else
                        std::cout << " " << opt->shortopt << (!opt->arg.empty()?" "+opt->arg:"") << opt->arg;
                }
                // In order to fill a line correct, y we do a little calculation here.
                int spacesLeft=leftLength;
                myout << "  ";
                if(!opt->shortopt.empty()) {
                    myout << opt->shortopt;
                    spacesLeft -= opt->shortopt.length();
                }
                if(!opt->shortopt.empty() && !opt->longopt.empty()) {
                    myout << " | ";
                    spacesLeft -= 3;
                }
                if(!opt->longopt.empty()) {
                    myout << opt->longopt;
                    spacesLeft -= opt->longopt.length();
                }
                spacesLeft -= opt->arg.length();
                myout << " " << opt->arg;
                if(spacesLeft >= 0) {
                    for(int c=1; c<=spacesLeft; c++)
                        myout << " ";
                } else {
                    myout << std::endl;
                    for(int c=30+2; c>=0; c--)
                        myout << " ";
                }
                myout << opt->desc << std::endl;
            }
        }
        if(!strayArgsArg.empty()) {
            std::cout << " ";
            if(strayArgsOptional) std::cout << "[";
            std::cout << strayArgsArg;
            if(strayArgsOptional) std::cout << "]";
            std::cout << std::endl << std::endl << strayArgsArg << ": " << strayArgsDesc;
        }

        std::cout << std::endl << myout.str();
    }
};

#endif
