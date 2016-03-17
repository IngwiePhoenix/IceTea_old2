#ifndef ICETEA_PLUGIN_H
#define ICETEA_PLUGIN_H

#include "Pluma.hpp"
#include <string>

// Forward declaration...
class IceTea;

class IceTeaPlugin {
public:
    virtual bool configure(IceTea*)  =0;
    virtual std::string getName()  =0;
    virtual std::string getDescription() =0;
    // Missing:
    // - getVersion()
    // - getIdentifier() to check via IceTea.isPluginLoaded(...)
};

PLUMA_PROVIDER_HEADER(IceTeaPlugin);

#define ICETEA_MODULE(name) \
    PLUMA_INHERIT_PROVIDER(name, IceTeaPlugin)

#endif
