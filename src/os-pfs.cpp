#include <sstream>

#include "IceTea.h"
#include "os-pfs.h"
#include "file_system.hpp"
#include "wildcard.hpp"
#include "InternalIceTeaPlugin.h"

#if defined(PREDEF_PLATFORM_WIN32)
    #define ICETEA_DIRSEP "\\"
#else
    #define ICETEA_DIRSEP "/"
#endif

using namespace std;
using namespace ObjectScript;
using namespace stlplus;

// This should help.
typedef vector<string> strVec;

OS_FUNC(os_pfs_mkdir) {
    EXPECT_STRING(1)
    string folder = os->toString(-params+0).toChar();
    bool rt;
    if(is_folder(folder)) {
        rt = true;
    } else if(is_present(folder) && !is_folder(folder)) {
        rt = false;
    } else {
        rt = folder_create(folder);
    }

    os->pushBool(rt);
    return 1;
}
OS_FUNC(os_pfs_mkdirp) {
    EXPECT_STRING(1)
    strVec parts = folder_elements( os->toString(-params+0).toChar() );
    strVec::iterator it = parts.begin();
    for(; it!=parts.end(); ++it) {
        if(!folder_exists(*it)) {
            if(!folder_create(*it)) {
                os->pushBool(false);
                return 1;
            }
        }
    }
    os->pushBool(true);
    return 1;
}
OS_FUNC(os_pfs_delete) {
    EXPECT_STRING(1)
    bool recurse = false;
    if(os->isType(OS_VALUE_TYPE_BOOL, -params+1)) recurse = os->toBool(-params+1);
    string spec = os->toString(-params+0).toChar();
    if(is_file(spec)) {
        os->pushBool(file_delete(spec));
    } else if(is_folder(spec)) {
        os->pushBool(folder_delete(spec, recurse));
    } else {
        os->pushNull();
    }
    return 1;
}
OS_FUNC(os_pfs_move) {
    EXPECT_STRING(1)
    EXPECT_STRING(2)
    string from = os->toString(-params+0).toChar();
    string to = os->toString(-params+1).toChar();
    if(is_file(from)) {
        os->pushBool(file_rename(from, to));
    } else if(is_folder(from)) {
        os->pushBool(folder_rename(from, to));
    } else {
        os->pushNull();
    }
    return 1;
}
OS_FUNC(os_pfs_copy) {
    EXPECT_STRING(1)
    EXPECT_STRING(2)
    string from = os->toString(-params+0).toChar();
    string to = os->toString(-params+1).toChar();
    if(is_file(from)) {
        os->pushBool(file_copy(from, to));
    } else {
        // We can not copy folders yet.
        os->pushNull();
    }
    return 1;
}

// TTVFS specific
OS_FUNC(os_pfs_getFileList) {
    EXPECT_STRING(1)
    string arg = os->toString(-params+0).toChar();
    if(is_folder(arg) && folder_readable(arg)) {
        strVec list = folder_files(arg);
        os->newArray();
        for(strVec::iterator it=list.begin(); it!=list.end(); ++it) {
            os->pushString(it->c_str());
            os->addProperty(-2);
        }
        return 1;
    } else {
        os->pushBool(false);
        return 1;
    }
}
OS_FUNC(os_pfs_getDirList) {
    EXPECT_STRING(1)
    string arg = os->toString(-params+0).toChar();
    if(is_folder(arg) && folder_readable(arg)) {
        strVec list = folder_subdirectories(arg);
        os->newArray();
        for(strVec::iterator it=list.begin(); it!=list.end(); ++it) {
            os->pushString(it->c_str());
            os->addProperty(-2);
        }
        return 1;
    } else {
        os->pushBool(false);
        return 1;
    }
}
OS_FUNC(os_pfs_getAllList) {
    EXPECT_STRING(1)
    string arg = os->toString(-params+0).toChar();
    if(is_folder(arg) && folder_readable(arg)) {
        strVec list = folder_all(arg);
        os->newArray();
        for(strVec::iterator it=list.begin(); it!=list.end(); ++it) {
            os->pushString(it->c_str());
            os->addProperty(-2);
        }
        return 1;
    } else {
        os->pushBool(false);
        return 1;
    }
}

OS_FUNC(os_pfs_glob) {
    EXPECT_STRING(1)
    EXPECT_STRING(2)
    string folder = os->toString(-params+0).toChar();
    string pattern = os->toString(-params+1).toChar();
    bool folders = os->isType(OS_VALUE_TYPE_BOOL, -params+2) ? os->toBool(-params+2) : true;
    bool files = os->isType(OS_VALUE_TYPE_BOOL, -params+3) ? os->toBool(-params+3) : true;
    strVec res = folder_wildcard(folder, pattern, folders, files);
    os->newArray();
    for(strVec::iterator it=res.begin(); it!=res.end(); ++it) {
        string realdir = create_filespec(folder, *it);
        os->pushString(realdir.c_str());
        os->addProperty(-2);
    }
    return 1;
}

OS_FUNC(os_pfs_wildcardMatch) {
    EXPECT_STRING(1)
    EXPECT_STRING(2)
    os->pushBool(wildcard(
        os->toString(-params+0).toChar(),
        os->toString(-params+1).toChar()
    ));
    return 1;
}

// STL links
CALL_STLPLUS_BOOL(os_pfs_fileExists,            file_exists)
CALL_STLPLUS_BOOL(os_pfs_dirExists,             folder_exists)

CALL_STLPLUS_BOOL(os_pfs_isPresent,             is_present)
CALL_STLPLUS_BOOL(os_pfs_isFile,                is_file)
CALL_STLPLUS_BOOL(os_pfs_isDir,                 is_folder)

CALL_STLPLUS_INT(os_pfs_getFileSize,            file_size)
CALL_STLPLUS_BOOL(os_pfs_fileReadable,          file_readable)
CALL_STLPLUS_BOOL(os_pfs_fileWritable,          file_writable)
CALL_STLPLUS_INT(os_pfs_fileCreated,            file_created)
CALL_STLPLUS_INT(os_pfs_fileModified,           file_modified)
CALL_STLPLUS_INT(os_pfs_fileAccessed,           file_accessed)

CALL_STLPLUS_BOOL(os_pfs_dirReadable,           folder_readable)
CALL_STLPLUS_BOOL(os_pfs_dirWritable,           folder_writable)
CALL_STLPLUS_BOOL(os_pfs_dirEmpty,              folder_empty)

CALL_STLPLUS_STRING(os_pfs_basename,            basename_part)
CALL_STLPLUS_STRING(os_pfs_filename,            filename_part)
CALL_STLPLUS_STRING(os_pfs_extname,             extension_part)
CALL_STLPLUS_STRING(os_pfs_dirname,             folder_part)

// Working directory stuff
CALL_STLPLUS_BOOL(os_pfs_isFullPath,            is_full_path)
CALL_STLPLUS_BOOL(os_pfs_isRelativePath,        is_relative_path)

OS_FUNC(os_pfs_lookup) {
    EXPECT_STRING(1)
    string file = os->toString(-params+0).toChar();
    string path;
    if(os->isArray(-params+1)) {
        int len = os->getLen(-params+1);
        for(int i=0; i<len; i++) {
            os->pushNumber(i);
            os->getProperty(-2);
            path.append(os->toString().toChar());
            if(i != (len-1)) path.append(PATH_SPLITTER);
            os->pop();
        }
    } else if(os->isString(-params+1)) {
        path = os->toString(-params+1).toChar();
    } else {
        os->setException("pfs.lookup expected second parameter to be either string or array.");
        os->pushNull();
        return 1;
    }
    os->pushString( lookup(file, path).c_str() );
    return 1;
}

OS_FUNC(os_pfs_join) {
    if(params < 2) {
        os->setException("pfs.path: Expected at least 2 parameters.");
        return 0;
    }
    string path = os->toString(-params+0).toChar();
    for(int i=1; i<params; i++) {
        string tmp = os->toString(-params+i).toChar();
        path = filespec_to_path(path, tmp);
    }
    os->pushString(path.c_str());
    return 1;
}

class IceTeaPFS: public IceTeaPlugin {
public:
    bool configure(IceTea* os) {
        OS::FuncDef pfsFuncs[] = {
            {OS_TEXT("mkdir"),              os_pfs_mkdir},
            {OS_TEXT("mkdirp"),             os_pfs_mkdirp},
            {OS_TEXT("delete"),             os_pfs_delete},
            {OS_TEXT("copy"),               os_pfs_copy},
            {OS_TEXT("move"),               os_pfs_move},

            //{OS_TEXT("open"),               os_pfs_open},

            {OS_TEXT("getFileList"),        os_pfs_getFileList},
            {OS_TEXT("getDirList"),         os_pfs_getDirList},
            {OS_TEXT("getAllList"),         os_pfs_getAllList},
            {OS_TEXT("glob"),               os_pfs_glob},

            {OS_TEXT("fileExists"),         os_pfs_fileExists},
            {OS_TEXT("dirExists"),          os_pfs_dirExists},

            {OS_TEXT("isDir"),              os_pfs_isDir},
            {OS_TEXT("isFile"),             os_pfs_isFile},
            {OS_TEXT("isPresent"),          os_pfs_isPresent},

            {OS_TEXT("getFileSize"),        os_pfs_getFileSize},
            {OS_TEXT("fileReadable"),       os_pfs_fileReadable},
            {OS_TEXT("fileWritable"),       os_pfs_fileWritable},
            {OS_TEXT("fileCreated"),        os_pfs_fileCreated},
            {OS_TEXT("fileModified"),       os_pfs_fileModified},
            {OS_TEXT("fileAccessed"),       os_pfs_fileAccessed},

            {OS_TEXT("dirReadable"),        os_pfs_dirReadable},
            {OS_TEXT("dirWritable"),        os_pfs_dirWritable},
            {OS_TEXT("dirEmpty"),           os_pfs_dirEmpty},

            {OS_TEXT("basename"),           os_pfs_basename},
            {OS_TEXT("extname"),            os_pfs_extname},
            {OS_TEXT("dirname"),            os_pfs_dirname},
            {OS_TEXT("filename"),           os_pfs_filename},

            {OS_TEXT("isFullPath"),         os_pfs_isFullPath},
            {OS_TEXT("isRelativePath"),     os_pfs_isRelativePath},

            {OS_TEXT("lookup"),             os_pfs_lookup},
            {OS_TEXT("join"),               os_pfs_join},
            {}
        };

        os->pushCFunction(os_pfs_wildcardMatch);
        os->setGlobal("wildcard");

        os->getModule("pfs");
        os->setFuncs(pfsFuncs);
        os->pop();

        return true;
    }
    string getName() {
        return "PFS: FileSystem+";
    }
    string getDescription() {
        return  "FileSystem functionality provided through STLPlus.";
    }
};
ICETEA_INTERNAL_MODULE(IceTeaPFS);
