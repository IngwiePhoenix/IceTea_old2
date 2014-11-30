#ifndef os_pfs_H
#define os_pfs_H

#include "objectscript.h"
#include "os-icetea.h" // For OS_FUNC. This extension will become standalone later.

bool initializeVFS(ObjectScript::OS* os);

// Traditional
OS_FUNC(os_pfs_mkdir);
OS_FUNC(os_pfs_mkdirp);
OS_FUNC(os_pfs_delete);
OS_FUNC(os_pfs_deleteDir);
OS_FUNC(os_pfs_move);

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
    OS_FUNC(os_pfs_file_getBuf);
OS_FUNC(os_pfs_file_close);

OS_FUNC(os_pfs_openDir);
    OS_FUNC(os_pfs_dir_load);
    OS_FUNC(os_pfs_dir_getFile);
    OS_FUNC(os_pfs_dir_getDir);
    OS_FUNC(os_pfs_dir_getFiles);
    OS_FUNC(os_pfs_dir_getDirs);
OS_FUNC(os_pfs_dir_close);

// TTVFS specific
OS_FUNC(os_pfs_getFileList);
OS_FUNC(os_pfs_getDirList);
OS_FUNC(os_pfs_glob);
OS_FUNC(os_pfs_mount);
OS_FUNC(os_pfs_unmount);

// System paths
OS_FUNC(os_pfs_getUserDir);
OS_FUNC(os_pfs_getAppDir);

// Helpers
OS_FUNC(os_pfs_getFileList);
OS_FUNC(os_pfs_getDirList);
OS_FUNC(os_pfs_fileExists);
OS_FUNC(os_pfs_isDir);
OS_FUNC(os_pfs_getFileSize);
OS_FUNC(os_pfs_fixSlashes);
OS_FUNC(os_pfs_fixPath);
OS_FUNC(os_pfs_basename);
OS_FUNC(os_pfs_makeSlashTerminated);
OS_FUNC(os_pfs_stripFileExtension);
OS_FUNC(os_pfs_stripLastPath);
OS_FUNC(os_pfs_wildcardMatch);

#endif
