#ifndef PLUGINSTORE_H
#define PLUGINSTORE_H

#include <vector>
#include <string>

#include "PluginDef.h"

typedef std::vector<PluginDef> PluginList;

class PluginStore {
public:
    static void attach(const PluginDef& plugin);
    static PluginList* get();
};

#endif
