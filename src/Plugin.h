#ifndef PLUGIN_H
#define PLUGIN_H

#include "PluginDef.h"

class Plugin {
public:
    Plugin(const std::string&, PluginCallback*);
};

#define ICETEA_MODULE(name, initFunc) \
    const Plugin p_##name(#name, initFunc)

#endif
