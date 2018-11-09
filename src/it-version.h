#ifndef ICETEA_VERSION_HPP
#define ICETEA_VERSION_HPP

// Helpers
#define STRINGIFY(arg)      #arg
#define STRINGIFY_MACRO(name) STRINGIFY(name)

// Version info
#define ICETEA_VERSION_RELEASE          0
#define ICETEA_VERSION_MINOR            2
#define ICETEA_VERSION_PATCH            0
#define ICETEA_VERSION_TAG              "-dev"

#define ICETEA_PLUGIN_VERSION_MIN       1
#define ICETEA_PLUGIN_VERSION_CURRENT   1

// Make it happen! (or rain?)
#define __MAKE_ICETEA_VERSION(release, minor, patch, tag) \
    release "." minor "." patch tag

#define ICETEA_VERSION \
    __MAKE_ICETEA_VERSION( \
        STRINGIFY_MACRO(ICETEA_VERSION_RELEASE), \
        STRINGIFY_MACRO(ICETEA_VERSION_MINOR), \
        STRINGIFY_MACRO(ICETEA_VERSION_PATCH), \
        ICETEA_VERSION_TAG \
    )

#endif
