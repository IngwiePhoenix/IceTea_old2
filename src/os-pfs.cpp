#include "objectscript.h"
#include "os-pfs.h"
#include "file_system.hpp"

using namespace std;
using namespace ObjectScript;
using namespace stlplus;

/*bool initializeVFS(OS* os) {
    OS::FuncDef vfsFuncs[] = {
        {OS_TEXT("mkdir"),              os_pfs_mkdir},
        {OS_TEXT("mkdirp"),             os_pfs_mkdirp},
        {OS_TEXT("delete"),             os_pfs_delete},
        {OS_TEXT("deleteDir"),          os_pfs_deleteDir},
        {OS_TEXT("move"),               os_pfs_move},
        {OS_TEXT("open"),               os_pfs_open},
        {OS_TEXT("openDir"),            os_pfs_openDir},
        {OS_TEXT("getFileList"),        os_pfs_getFileList},
        {OS_TEXT("getDirList"),         os_pfs_getDirList},
        {OS_TEXT("glob"),               os_pfs_glob},
        {OS_TEXT("mount"),              os_pfs_mount},
        {OS_TEXT("unmount"),            os_pfs_unmount},
        {OS_TEXT("fileExists"),         os_pfs_fileExists},
        {OS_TEXT("isDir"),              os_pfs_isDir},
        {OS_TEXT("getFileSize"),        os_pfs_getFileSize},
        {OS_TEXT("fixSlashes"),         os_pfs_fixSlashes},
        {OS_TEXT("fixPath"),            os_pfs_fixPath},
        {OS_TEXT("basename"),           os_pfs_basename},
        {OS_TEXT("makeSlashTerminate"), os_pfs_makeSlashTerminated},
        {OS_TEXT("stripFileExtension"), os_pfs_stripFileExtension},
        {OS_TEXT("stripLastPath"),      os_pfs_stripLastPath},
        {OS_TEXT("__get@userDir"),      os_pfs_getUserDir},
        {OS_TEXT("__get@appDir"),       os_pfs_getAppDir},
        {}
    };

    os->pushCFunction(os_pfs_wildcardMatch);
    os->setGlobal("wildcard");

    os->getModule("vfs");
    os->setFuncs(vfsFuncs);
    os->pop();

    return true;
}


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
*/
