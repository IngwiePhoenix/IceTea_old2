#include "PluginDef.h"
#include "Plugin.h"

#include <string>

using namespace std;

PluginDef::PluginDef(const string& name, PluginCallback* cb, Plugin* self) {
    this->name = name;
    this->cb = cb;
    this->self = self;
}
