#include <stdlib.h> // getenv, putenv
#include <string>

#include "os-pfs.h" // CALL_STLPLUS_*()
#include "file_system.hpp"
#include "predef.h"
#include "InternalIceTeaPlugin.h"

using namespace stlplus;
using namespace ObjectScript;
using namespace std;

OS_FUNC(os_sys_cwd) {
    os->pushString(folder_current().c_str());
    return 1;
}
OS_FUNC(os_sys_fullCwd) {
    os->pushString(folder_current_full().c_str());
    return 1;
}

CALL_STLPLUS_BOOL(os_sys_cd,      folder_set_current)
CALL_STLPLUS_STRING(os_sys_which, path_lookup)

OS_FUNC(os_sys_getUserDir) {
    os->pushString(folder_home().c_str());
    return 1;
}

OS_FUNC(os_sys_pathSep) {
    os->pushString(PATH_SPLITTER);
    return 1;
}

OS_FUNC(os_sys_osType) {
    #if defined(PREDEF_PLATFORM_UNIX) \
        || defined(PREDEF_PLATFORM_BSD) \
        || defined(PREDEF_OS_MACOSX)
    os->pushString("unix");
    #elif defined(PREDEF_PLATFORM_WIN32)
    os->pushString("windows");
    #else
    os->pushString("not supported");
    os->setException("You are hitting an unsupported platform here. File an issue to get it supported.");
    #endif

    return 1;
}

OS_FUNC(os_sys_dirSep) {
    #if defined(PREDEF_PLATFORM_UNIX) \
        || defined(PREDEF_PLATFORM_BSD) \
        || defined(PREDEF_OS_MACOSX)
    os->pushString("/");
    #elif defined(PREDEF_PLATFORM_WIN32)
    os->pushString("\\");
    #else
    os->pushString("not supported");
    os->setException("You are hitting an unsupported platform here. File an issue to get it supported.");
    #endif

    return 1;
}

OS_FUNC(os_sys_osName) {
    #if defined(PREDEF_OS_MACOSX)
    os->pushString("Mac OS X");
    #elif defined(PREDEF_OS_LINUX)
    os->pushString("GNU/Linux");
    #elif defined(PREDEF_PLATFORM_WIN32)
    os->pushString("Microsoft Windows");
    #else
    os->setException("The name of your OS was not defined. File an issue to get it fixed.");
    os->pushString("Generic");
    #endif
    return 1;
}

OS_FUNC(os_sys_getenv) {
    EXPECT_STRING(1)
    const char* env = os->toString(-params+0).toChar();
    char* val;
    val = getenv(env);
    if(val == NULL) {
        // The variable was not found. Make it empty.
        val = (char*)"";
    }
    os->pushString( val );
    return 1;
}

OS_FUNC(os_sys_putenv) {
    EXPECT_STRING(1)
    EXPECT_STRING(2)
    string str;
    str.append( os->toString(-params+0).toChar() );
    str.append( "=" );
    str.append( os->toString(-params+1).toChar() );
    // Wow, C-like memory management. whut.
    char* nstr = new char[str.length()+1];
    strcpy(nstr, str.c_str());
    os->pushNumber( putenv(nstr) );
    return 1;
}

class IceTeaSystem: public IceTeaPlugin {
public:
    bool configure(IceTea* os) {
        OS::FuncDef sysFuncs[] = {
            {OS_TEXT("cd"),             os_sys_cd},
            {OS_TEXT("which"),          os_sys_which},
            {OS_TEXT("__get@cwd"),      os_sys_cwd},
            {OS_TEXT("__get@fullCwd"),  os_sys_fullCwd},
            {OS_TEXT("__get@userDir"),  os_sys_getUserDir},
            {OS_TEXT("__get@pathSep"),  os_sys_pathSep},
            {OS_TEXT("__get@dirSep"),   os_sys_dirSep},
            {OS_TEXT("__get@type"),     os_sys_osType},
            {OS_TEXT("__get@name"),     os_sys_osName},
            // Env
            {OS_TEXT("getenv"),         os_sys_getenv},
            {OS_TEXT("putenv"),         os_sys_putenv},
            {}
        };

        os->getModule("sys");
        os->setFuncs(sysFuncs);
        os->pop();

        return true;
    }
    string getName() {
        return "System";
    }
    string getDescription() {
        return  "Get this current system's informations.\n"
                "IMPORTANT: Some information is currently aquired at compile-time! That means \n"
                "that some information can be INCORRECT. I will introduce better checks later!";
    }
};
ICETEA_INTERNAL_MODULE(IceTeaSystem);
