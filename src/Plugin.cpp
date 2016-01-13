#include "Plugin.h"
#include "PluginStore.h"
#include "PluginDef.h"
#include <string>

using namespace std;

Plugin::Plugin(const string& name, PluginCallback* cb) {
    PluginDef plugin(name, cb, this);
    PluginStore::attach(plugin);
}
