#ifndef os_pfs_H
#define os_pfs_H

#include "IceTea.h"
#include "os-icetea.h" // For OS_FUNC. This extension will become standalone later.

#define EXPECT_STRING(idx) \
    if(!os->isString(-params+(idx-1))) { \
        os->setException("Parameter " # idx " is expected to be a string."); \
        os->pushNull(); \
        return 1; \
    }
#define EXPECT_BOOL(idx) \
    if(!os->isBool(-params+(idx-1))) { \
        os->setException("Parameter " # idx " is expected to be a boolean."); \
        os->pushNull(); \
        return 1; \
    }

#define CALL_STLPLUS_STRING(osfnc, stlfnc) OS_FUNC(osfnc) {                \
        EXPECT_STRING(1)                                            \
        os->pushString( stlfnc(os->toString(-params+0).toChar()).c_str() ); \
        return 1;                                                   \
    }                                                               \

#define CALL_STLPLUS_BOOL(osfnc, stlfnc) OS_FUNC(osfnc) {                \
        EXPECT_STRING(1)                                            \
        os->pushBool( stlfnc(os->toString(-params+0).toChar()) ); \
        return 1;                                                   \
    }                                                               \

#define CALL_STLPLUS_INT(osfnc, stlfnc) OS_FUNC(osfnc) {                \
        EXPECT_STRING(1)                                            \
        os->pushNumber( stlfnc(os->toString(-params+0).toChar()) ); \
        return 1;                                                   \
    }                                                               \

#endif
