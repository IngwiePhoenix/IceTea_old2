#include "InternalIceTeaPlugin.h"

using namespace std;

void InternalPluginStore::attach(IceTeaPluginProvider* plugin) {
    InternalPluginStore::getStore()->push_back(plugin);
}

// Simple: Just a container.
// I could probably move it into the class itself...
// ...but this seems more convenient o.O"
IceTeaInternalPlugins* InternalPluginStore::getStore() {
    static IceTeaInternalPlugins store;
    return &store;
}
