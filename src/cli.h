#ifndef CLI_H
#define CLI_H

#include <unordered_map>
#include <map>
#include <list>
#include <vector>
#include <iostream>
#include <sstream>

/*
    We basically overwrite NBCL::insert, NBCL::usage and add a function to print a group.
*/

class CLI {
private:
    struct Option {
        // Name
        std::string shortopt;
        std::string longopt;
        // Minimal argument description
        std::string arg;
        // Full desc
        std::string desc;
        // for core
        bool optional;
        bool present;
        std::string value;
    };
    typedef std::vector<Option*> OptList;
    typedef std::unordered_map<std::string, OptList> OptGroups;
    // [group] => (...args...)
    OptGroups ga;
    std::vector<std::string> strayArgs;
    std::list<std::string> orderedOpts;
    int argc;
    const char** argv;
    std::string strayArgsDesc;
    std::string copyright;
    int leftLength;
    std::string lastGroup;
public:
    inline CLI(int _argc, const char** _argv, std::string _copyright="") {
        argc=_argc;
        argv=_argv;
        strayArgsDesc="";
        copyright=_copyright;
        leftLength=30;
        lastGroup="";
    }
    inline ~CLI() {
        for(OptGroups::iterator it=ga.begin(); it!=ga.end(); ++it) {
            for(unsigned int ot=0; ot<it->second.size(); ++ot) {
                delete it->second[ot];
            }
        }
    }
    inline void setArgWidth(int len) { leftLength = len; }
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
    inline void group(std::string name) {
        lastGroup=name;
        orderedOpts.push_back(name);
    }
    inline void setStrayArgsDesc(std::string desc) { strayArgsDesc=desc; }
    std::vector<std::string> getStrayArgs() { return strayArgs; }
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
    inline bool parse() {
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
    inline bool check(std::string longopt) {
        std::pair<std::string,int> opt = findOpt(longopt);
        if (opt.second!=-1)
            return ga[opt.first][opt.second]->present;
        else
            return false;
    }
    inline std::string value(std::string optname) {
        std::pair<std::string,int> opt = findOpt(optname);
        if (opt.second!=-1)
            return ga[opt.first][opt.second]->value;
        else
            return std::string("");
    }

    // Now to the formatting
    inline void usage() {
        std::stringstream myout;
        if(copyright!="") std::cout << copyright << std::endl;
        std::cout << "USAGE: " << argv[0];
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
        std::cout << std::endl << myout.str();
    }
};

#endif
