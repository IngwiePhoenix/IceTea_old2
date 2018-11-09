#ifndef ICETEA_INTERNAL_PLUGIN_H
#define ICETEA_INTERNAL_PLUGIN_H

#include "IceTeaPlugin.h"
#include <vector>

typedef std::vector<IceTeaPluginProvider*> IceTeaInternalPlugins;

class InternalPluginStore {
public:
    static void attach(IceTeaPluginProvider*);
    static IceTeaInternalPlugins* getStore();
};

#define ICETEA_INTERNAL_MODULE(name) \
    ICETEA_MODULE(name) \
    volatile class name##InternalPlugin { \
    private: \
        name##Provider* provider; \
    public: \
        name##InternalPlugin() { \
            this->provider = new name##Provider(); \
            InternalPluginStore::attach(this->provider); \
        } \
        name##Provider* getProvider() { return this->provider; } \
    } name##PluginInstance;

#endif
