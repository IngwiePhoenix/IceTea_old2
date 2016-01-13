#include <iostream>

#include "PluginStore.h"

using namespace std;

PluginList* obtainList() {
    static PluginList plugins;
    return &plugins;
}

void PluginStore::attach(const PluginDef& plugin) {
    obtainList()->push_back(plugin);
}

PluginList* PluginStore::get() {
    return obtainList();
}
