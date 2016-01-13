#include "PluginStore.h"

// private: std::vector<PluginDef> plugins;

_PluginStore_impl::_PluginStore_impl() {
    // We dont do anything yet.
}

void _PluginStore_impl::attach(const PluginDef& plugin) {
    this->plugins.push_back(plugin);
}

PluginList* _PluginStore_impl::get() {
    return &(this->plugins);
}

_PluginStore_impl PluginStore;
