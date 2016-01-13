#ifndef PLUGINSTORE_H
#define PLUGINSTORE_H

#include <vector>
#include <string>

#include "PluginDef.h"

typedef std::vector<PluginDef> PluginList;

class _PluginStore_impl {
private:
    PluginList plugins;

public:
    _PluginStore_impl();
    void attach(const PluginDef& plugin);
    PluginList* get();
};

extern _PluginStore_impl PluginStore;

#endif
