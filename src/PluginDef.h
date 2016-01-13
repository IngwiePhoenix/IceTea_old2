#ifndef PLUGINDEF_H
#define PLUGINDEF_H

#include <string>

// Forward defs
class Plugin;
class IceTea;

typedef bool(PluginCallback)(IceTea*);

struct PluginDef {
    std::string name;
    PluginCallback* cb;
    Plugin* self;

    PluginDef(const std::string&, PluginCallback*, Plugin*);
};

#endif
