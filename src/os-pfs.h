#ifndef os_pfs_H
#define os_pfs_H

#include "objectscript.h"
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


bool initializePFS(ObjectScript::OS* os);

// Traditional
OS_FUNC(os_pfs_mkdir);
OS_FUNC(os_pfs_mkdirp);
OS_FUNC(os_pfs_copy);
OS_FUNC(os_pfs_delete);
OS_FUNC(os_pfs_move);

/* To be implemented soon...
OS_FUNC(os_pfs_open);
    // Traditional file functions
    OS_FUNC(os_pfs_file_read);
    OS_FUNC(os_pfs_file_write);
    OS_FUNC(os_pfs_file_tell);
    OS_FUNC(os_pfs_file_seek);
    OS_FUNC(os_pfs_file_eof);
    OS_FUNC(os_pfs_file_flush);
    // Non-traditional
    OS_FUNC(os_pfs_file_seekRel);
    OS_FUNC(os_pfs_file_getSize);
    OS_FUNC(os_pfs_file_getContents);
OS_FUNC(os_pfs_file_close);
OS_FUNC(os_pfs_file_write_contents);
OS_FUNC(os_pfs_file_get_contents);
*/

// TTVFS specific
OS_FUNC(os_pfs_getFileList);
OS_FUNC(os_pfs_getDirList);
OS_FUNC(os_pfs_getAllList);
OS_FUNC(os_pfs_glob);

// System paths
OS_FUNC(os_pfs_getUserDir);

// Helpers
OS_FUNC(os_pfs_wildcardMatch);
OS_FUNC(os_pfs_fileExists);
OS_FUNC(os_pfs_dirExists);

OS_FUNC(os_pfs_isPresent);
OS_FUNC(os_pfs_isFile);
OS_FUNC(os_pfs_isDir);

OS_FUNC(os_pfs_getFileSize);
OS_FUNC(os_pfs_fileReadable);
OS_FUNC(os_pfs_fileWritable);
OS_FUNC(os_pfs_fileCreated);
OS_FUNC(os_pfs_fileModified);
OS_FUNC(os_pfs_fileAccessed);

OS_FUNC(os_pfs_dirReadable);
OS_FUNC(os_pfs_dirWritable);
OS_FUNC(os_pfs_dirEmpty);

OS_FUNC(os_pfs_basename);
OS_FUNC(os_pfs_filename);
OS_FUNC(os_pfs_extname);
OS_FUNC(os_pfs_dirname);

// Working directory stuff
OS_FUNC(os_pfs_isFullPath);
OS_FUNC(os_pfs_isRelativePath);

OS_FUNC(os_pfs_lookup);
OS_FUNC(os_pfs_join);

OS_FUNC(os_pfs_writeFile);

#endif
