#include <iostream>
#include <stdlib.h>
#include <algorithm>
#include <string>
#include <set>

#include "objectscript.h"
#include "os-detector.h"
#include "os-exec.h"
#include "os-pfs.h"
#include "os-icetea.h"
#include "cli.h"
#include "filecache.hpp"
#include "file_system.hpp"
#include "picosha2.h"
#include "wildcard.hpp"
#include "util.h"

using namespace std;
using namespace ObjectScript;
using namespace stlplus;
using namespace picosha2;

#define HEADER(strm) strm << "\u250C\u2500\u2500\u2500\u2500 "
#define OUTPUT(strm) strm << "\u2502 "
#define FOOTER(strm) strm << "\u2514\u2500\u2500\u2500\u2500 "

struct compiler_t {
    string main;
    string libflag;
    string incdirflag;
    string compileflag;
    string outputflag;
    compiler_t(string ma, string lf, string idf, string cf, string of):
        main(ma),
        libflag(lf),
        incdirflag(idf),
        compileflag(cf),
        outputflag(of) {
            // Nothing but dust.
    }
    compiler_t(string lf, string idf, string cf, string of):
        libflag(lf),
        incdirflag(idf),
        compileflag(cf),
        outputflag(of) {
            // Nothing but dust.
    }
    compiler_t() {}
};

typedef map<string, compiler_t> compiler_map_t;
typedef map<string, compiler_map_t > compiler_list_t;
compiler_list_t compilers;
void initCompilers() {
    // C compilers and their flags...
    compilers["CC"]["gcc"] =      compiler_t("-l %", "-I", "-c", "-o");
    compilers["CC"]["clang"]    = compiler_t("-l %", "-I", "-c", "-o");
    compilers["CC"]["cl"]       = compiler_t("/nologo", "lib%.lib", "/I", "/c", "/Fo");
    compilers["CXX"]["clang-cl"]= compiler_t("%.lib", "/I", "/c", "/Fo");

    // C++ compilers
    compilers["CXX"]["g++"]     = compiler_t("-l %", "-I", "-c", "-o");
    compilers["CXX"]["clang++"] = compiler_t("-l %", "-I", "-c", "-o");
    compilers["CXX"]["cl"]      = compiler_t("/nologo", "%.lib", "/I", "/c", "/Fo");
    compilers["CXX"]["clang-cl"]= compiler_t("%.lib", "/I", "/c", "/Fo");

    // Objective-C
    compilers["OBJC"]["gcc"]     = compiler_t("-l %", "-I", "-c", "-o");
    compilers["OBJC"]["clangcc"] = compiler_t("-l %", "-I", "-c", "-o");

    // Objective-C++
    compilers["OBJCXX"]["g++"]     = compiler_t("-l %", "-I", "-c", "-o");
    compilers["OBJCXX"]["clang++"] = compiler_t("-l %", "-I", "-c", "-o");

    // Java
    compilers["JAVAC"]["javac"] =    compiler_t();

    // Swift
    compilers["SWIFTC"]["swiftc"] =  compiler_t();
}

string have_prefix("HAVE_");
string havelib_prefix("HAVE_LIB");
string havefunc_prefix("HAVE_");
Filecache* cache;
extern CLI* cli; // main.cpp

string toUpper(const string str) {
    string s = str;
    transform(s.begin(), s.end(), s.begin(), ::toupper);
    return s;
}

string have_type(const string& type) {
    string s;
    s.append(have_prefix);
    s.append(toUpper(type));
    return s;
}

string have_header(const string name) {
    string s;
    string n = toUpper(name);
    ReplaceStringInPlace(n, "-", "_");
    ReplaceStringInPlace(n, ".", "_");
    ReplaceStringInPlace(n, "/", "_");
    ReplaceStringInPlace(n, "\\", "_");
    s.append(have_prefix);
    s.append(n);
    return s;
}

string have_tool(const string name) {
    string s;
    string n = toUpper(name);
    ReplaceStringInPlace(n, "-", "_");
    ReplaceStringInPlace(n, ".", "_");
    ReplaceStringInPlace(n, "/", "_");
    ReplaceStringInPlace(n, "\\", "_");
    ReplaceStringInPlace(n, "+", "X");
    s.append(n);
    return s;
}


string have_lib(const string lib) {
    string s;
    s.append(havelib_prefix);
    s.append(toUpper(lib));
    return s;
}

string have_func(const string func) {
    string prefunc = func;
    prefunc = prefunc.substr(0, prefunc.find("(")); // Cut till paranthese.
    // Basic
    ReplaceStringInPlace(prefunc, " ", "_");
    ReplaceStringInPlace(prefunc, ".", "_");
    // For C++
    ReplaceStringInPlace(prefunc, ":", "_");
    ReplaceStringInPlace(prefunc, "<", "_");
    ReplaceStringInPlace(prefunc, ">", "_");
    // ObjC/++
    ReplaceStringInPlace(prefunc, "[", "_");
    ReplaceStringInPlace(prefunc, "]", "_");
    ReplaceStringInPlace(prefunc, "+", "_");
    ReplaceStringInPlace(prefunc, "-", "_");
    string s;
    s.append(havefunc_prefix);
    s.append(toUpper(prefunc));
    return s;
}

string have_headerfunc(const string header, const string func) {
    stringstream ss;
    ss << have_prefix << func << "_in_" << header;
    string key = ss.str();
    // Basic
    ReplaceStringInPlace(key, " ", "_");
    ReplaceStringInPlace(key, ".", "_");
    // For C++
    ReplaceStringInPlace(key, ":", "_");
    ReplaceStringInPlace(key, "<", "_");
    ReplaceStringInPlace(key, ">", "_");
    // ObjC/++
    ReplaceStringInPlace(key, "[", "_");
    ReplaceStringInPlace(key, "]", "_");
    ReplaceStringInPlace(key, "+", "_");
    ReplaceStringInPlace(key, "-", "_");

    return toUpper(key);
}

// Ugly but efficient.
string have_libfunc(const string lib, const string func) {
    string nlib;
    nlib.append("lib");
    nlib.append(lib);
    return have_headerfunc(nlib, func);
}

string find_tool(vector<string> cmds) {
    for(vector<string>::iterator it=cmds.begin(); it!=cmds.end(); ++it) {
        string tool = path_lookup(*it);
        if(!tool.empty()) {
            return tool;
        }
    }
    return string("");
}

string kind2key(const string kind) {
    string lkind = kind;
    string key;
    // Transform to lowercase...
    transform(lkind.begin(), lkind.end(), lkind.begin(), ::tolower);
    if(lkind == "c")
        key = "CC";
    else if(lkind == "cxx" || lkind == "c++" || lkind == "cc")
        key = "CXX";
    else if(lkind == "objc")
        key = "OBJC";
    else if(lkind == "objc++" || lkind == "objcxx")
        key = "OBJCXX";
    else if(lkind == "swift" || lkind == "swiftc")
        key = "SWIFTC";
    else if(lkind == "java" || lkind == "javac")
        key = "JAVAC";

    return key;
}

string key2ext(const string key) {
    string res;
    if(key == "CC")
        res = "c";
    else if(key == "CXX")
        res = ".cpp";
    else if(key == "OBJC")
        res = ".m";
    else if(key == "OBJCXX")
        res = ".mm";
    else if(key == "JAVAC")
        res = ".java";
    else if(key == "SWIFTC")
        res = ".swift";

    return res;
}

string key2name(const string key) {
    if(key == "CC")
        return "C";
    else if(key == "CXX")
        return "C++";
    else if(key == "OBJC")
        return "Objective-C";
    else if(key == "OBJCXX")
        return "Objective-C++";
    else if(key == "JAVAC")
        return "Java";
    else if(key == "SWIFTC")
        return "Swift";

    return string();
}

string find_compiler(const string kind) {
    static map<string,bool> found;
    vector<string> cmds;
    string skey = kind2key(kind);
    string cname = key2name(skey);
    compiler_map_t::iterator it=compilers[skey].begin();
    for(; it!=compilers[skey].end(); ++it) {
        cmds.push_back(it->first);
    }

    // We reached this part, so we can define this to exist.
    bool doPrint = true;
    if(found.find(skey) != found.end() && found[skey]==true) {
        doPrint=false;
    }

    if(doPrint) OUTPUT(cout) << "Checking for a " << cname << " compiler... ";

    // Maybe the environment has it...?
    char* env_val = getenv(skey.c_str());
    if(env_val != NULL) {
        // Oh boy, it does!
        if(doPrint) cout << "$" << skey << ": " << env_val << endl;
        // Generate a proper string key
        string env_val_str;
        env_val_str.append("\"");
        env_val_str.append(env_val);
        env_val_str.append("\"");
        cache->set(skey, env_val_str, "detector");
        cache->sync();
        found[skey]=true;
        return env_val_str;
    } else if(cache->get(skey, "detector") != "") {
        string rt = cache->get(skey, "detector");
        if(doPrint) cout << rt << " (Cached)" << endl;
        found[skey]=true;
        return rt;
    } else {
        string rt = find_tool(cmds);
        if(rt.empty()) {
            if(doPrint) cout << "No" << endl;
            return string("");
        } else {
            string srt;
            srt.append("\"");
            srt.append(rt);
            srt.append("\"");
            cache->set(skey, srt, "detector");
            if(doPrint) cout << rt << endl;
            cache->sync();
            found[skey]=true;
            return rt;
        }
    }
    // Just so that compilers dont complain about no returns...
    return string("");
}

bool write_file(const string& content, const string& filename) {
    FILE* fh = fopen(filename.c_str(), "wb");
    if(!fh) {
        #ifdef DEBUG
        OUTPUT(cout) << "    Error: open file => " << filename << endl;
        #endif
        return false;
    }

    size_t res = fwrite(content.c_str(), sizeof(char), content.length(), fh);
    fclose(fh);
    if(res != content.length()) {
        #ifdef DEBUG
        OUTPUT(cout) << "    Error: Writing file => " << filename << endl;
        #endif
        return false;
    }
    return true;
}

pair<bool,CommandResult> run_task(const string source, const string kind, string add="", bool run=false) {
    bool res=false;
    string tool = find_compiler(kind);
    string fname;
    pair<bool, CommandResult> out;
    hash256_hex_string(source, fname);
    fname = create_filename( fname, key2ext(kind2key(kind)) );
    string bname = create_filename(fname, "bin");

    if(write_file(source, fname)) {
        res = true;
        stringstream cmd;
        // We simply assume, that this works. Should use some @settings of the rule...? FIXME pls.
        cmd << tool << " " << fname << " -o " << fname << ".bin " << add;
        CommandResult rt = it_cmd(cmd.str(), vector<string>());

        if(!run) {
            out.first = res;
            out.second = rt;
        } else {
            if(rt.exit_code == 0) {
                stringstream cmd2;
                cmd2 << create_filespec(".", bname);
                CommandResult cr2 = it_cmd(cmd2.str(), vector<string>());
                out.first = res;
                out.second = cr2;
            } else {
                out.first = false;
                out.second = rt;
            }
        }

        // Try to clean up
        file_delete(fname);
        file_delete(bname);
    } else {
        out.first=res;
    }

    return out;
}

OS_FUNC(osd_header) {
    EXPECT_STRING(1)
    EXPECT_STRING(2)
    string kind = os->toString(-params+0).toChar();
    string header = os->toString(-params+1).toChar();
    string tool = find_compiler(kind);
    string name = key2name(kind2key(kind));
    string stkey = have_header(header);
    OUTPUT(cout) << "Checking for " << name << " header <" << header << ">... ";

    string has_hdr = cache->get(stkey, "detector");
    if( has_hdr == "" ) {
        // We need to perform the lookup.
        stringstream source;
        source << "#include <" << header << ">" << endl
               << "int main(int argc, char** argv) { return 0; }" << endl;
        pair<bool, CommandResult> comres = run_task(source.str(), kind);
        if(comres.second.exit_code==0) {
            cout << "Yes" << endl;
            cache->set(stkey, "1", "detector");
            os->pushBool(true);
        } else {
            cout << "No" << endl;
            cache->set(stkey, "0", "detector");
            os->pushBool(false);
        }
    } else {
        if(has_hdr == "1") {
            // Appearently it is in the cache, which means its defined!
            cout << "Yes";
            os->pushBool(true);
        } else {
            // It's 0...undefined.
            cout << "No";
            os->pushBool(false);
        }
        cout << " (Cached)" << endl;
    }
    cache->sync();
    return 1;
}

OS_FUNC(osd_lib) {
    EXPECT_STRING(1)
    string lib = os->toString(-params+0).toChar();
    string libkey = have_lib(lib);
    string tool = find_compiler("c");
    string libflag;
    string cached = cache->get(libkey, "detector");
    if(wildcard("*cl.exe",tool)) {
        // Windoze.
        libflag = "";
    } else {
        libflag = "-l";
    }

    OUTPUT(cout) << "Checking for library " << lib << "... ";

    if(cached != "") {
        // Its within the cache.
        if(cached == "1") {
            cout << "Yes";
            os->pushBool(true);
        } else {
            cout << "No";
            os->pushBool(false);
        }
        cout << " (Cached)" << endl;
    } else {
        string code = "int main(int argc, char** argv){ return 0; }";
        string LDFLAGS;
        LDFLAGS.append(libflag);
        LDFLAGS.append(lib);
        pair<bool, CommandResult> comres = run_task(code, "c", LDFLAGS);
        if(comres.first && comres.second.exit_code == 0) {
            cout << "Yes" << endl;
            os->pushBool(true);
            cache->set(libkey, "1", "detector");
        } else {
            cout << "No" << endl;
            os->pushBool(false);
            cache->set(libkey, "0", "detector");
        }
    }
    cache->sync();
    return 1;
}

OS_FUNC(osd_compiler) {
    EXPECT_STRING(1)
    string kind = os->toString(-params+0).toChar();
    os->pushString( find_compiler(kind).c_str() );
    return 1;
}


OS_FUNC(osd_func) {
    EXPECT_STRING(1)
    EXPECT_STRING(2)
    string kind = os->toString(-params+0).toChar();
    string func = os->toString(-params+1).toChar();
    string tool = find_compiler(kind);
    string funckey = have_func(func);
    string cached = cache->get(funckey,"detector");

    // extern "C" functions are the same everywhere...
    stringstream code;
    code << "int main(int argc, char** argv) { " << func << "; return 0; }";

    OUTPUT(cout) << "Checking for " << key2name(kind2key(kind)) << " function " << func << "... ";
    if(!cached.empty()) {
        if(cached == "1") {
            cout << "Yes";
            os->pushBool(true);
        } else {
            cout << "No";
            os->pushBool(false);
        }
        cout << " (Cached)" << endl;
    } else {
        pair<bool, CommandResult> comres = run_task(code.str(), kind);
        if(comres.first && comres.second.exit_code == 0) {
            cout << "Yes" << endl;
            os->pushBool(true);
            cache->set(funckey, "1", "detector");
        } else {
            cout << "No" << endl;
            os->pushBool(false);
            cache->set(funckey, "0", "detector");
        }
    }
    cache->sync();
    return 1;
}

OS_FUNC(osd_libfunc) {
    EXPECT_STRING(1)
    EXPECT_STRING(2)
    string kind = "c"; // Can't check for C++ functions in libs...
    string lib = os->toString(-params+0).toChar();
    string name = os->toString(-params+1).toChar();
    string tool = find_compiler("c");
    string libname = lib;

    // We need 2 cached variables for this run.
    string libkey = have_lib(lib);
    string funckey = have_func(name);
    string libcached = cache->get(libkey, "detector");
    string funccached = cache->get(funckey, "detector");

    // For the scenario, that 0 was passed, we gotta do this right.
    if(lib != "0") {
        string _lib = lib;
        lib = "";
        lib.append("LIBNAME(\"");
        lib.append(_lib);
        lib.append("\")");
    } else {
        lib = "NULL";
    }

    // Generate a bit of C
    stringstream basecode;
    basecode << "#ifdef _WIN32" << endl
         << "#include <windows.h>" << endl
         << "#define LIBNAME(l) TEXT(\"lib\" l \".dll\")" << endl
         << "#else" << endl
         << "#include <unistd.h>" << endl
         << "#include <dlfcn.h>" << endl
         << "#ifdef __APPLE__" << endl
         << "#define SHLIBEXT \".dylib\"" << endl
         << "#else" << endl
         << "#define SHLIBEXT \".so\"" << endl
         << "#endif" << endl
         << "#define LIBNAME(l) \"lib\" l SHLIBEXT" << endl
         << "#endif" << endl
         << "int main(int argc, char** argv) {" << endl
         << "#ifndef _WIN32" << endl
         << "    int rtv=0;" << endl
         << "    <DLOPEN>" << endl
         << "    if(dh == NULL) {"<< endl
         << "        rtv=1;" << endl
         << "        goto end;" << endl
         << "    }" << endl
         << "    void* fh = dlsym(dh, \"" << name << "\");" << endl
         << "    if(fh == NULL) {" << endl
         << "        rtv=2;" << endl
         << "    } else {" << endl
         << "        rtv=4;" << endl
         << "        goto end;" << endl
         << "    }" << endl
         << "    fh = dlsym(dh, \"_" << name << "\");" << endl
         << "    if(fh == NULL) {" << endl
         << "        rtv=3;" << endl
         << "    } else {" << endl
         << "        rtv=5;" << endl
         << "    }" << endl
         << "    end:" << endl
         << "    if(dh!=NULL) dlclose(dh);" << endl
         << "    return rtv;" << endl
         << "#else" << endl
         << "    FARPROC* fh = GetProcAddress(" << endl
         << "        GetModuleHandle(" << lib << "," << endl
         << "        \"" << name << "\"" << endl
         << "    );" << endl
         << "    if(fh == NULL) return 6;" << endl
         << "    return 7;" << endl
         << "#endif" << endl
         << "}";
    stringstream dlopen1;
    dlopen1 << "void* dh = dlopen(" << lib << ", RTLD_LAZY);";
    stringstream dlopen2;
    dlopen2 << "void* dh = dlopen(0, RTLD_LAZY);";
    string code1 = ReplaceString(basecode.str(), "<DLOPEN>", dlopen1.str());
    string code2 = ReplaceString(basecode.str(), "<DLOPEN>", dlopen2.str());

    if(!funccached.empty() && !libcached.empty()) {
        // Both are defined.
        OUTPUT(cout) << "Checking for function " << name << " in library " << libname << "... ";
        if(funccached == "1" && libcached == "1") {
            cout << "Yes";
            os->pushBool(true);
        } else {
            cout << "No";
            os->pushBool(false);
        }
        cout << " (Cached)" << endl;
    } else {
        // None of them exists...now comes the really bitchy part.
        bool runSecond=false;

        // Goto mark
        LIBFUNC_switcher:
        if(runSecond == false) {
            OUTPUT(cout) << "Checking for function " << name << " in library " << libname << "... ";
        } else {
            OUTPUT(cout) << "Re-checking function " << name << " in library " << libname << "... ";
        }
        string LDFLAGS;
        string libdl = (wildcard("*\\cl.exe", tool) ? "Kernel32" : "dl");
        LDFLAGS.append(ReplaceString(compilers["CC"][filename_part(tool)].libflag, "%", libdl));
        if(runSecond) {
            LDFLAGS.append(" ");
            LDFLAGS.append(ReplaceString(compilers["CC"][filename_part(tool)].libflag, "%", libname));
        }
        string compcode = (runSecond==false ? code1 : code2);
        pair<bool,CommandResult> pcom = run_task(compcode, "c", LDFLAGS, true);
        #ifdef IT_WRITE_TESTS
        // Write it, for the lulz...
        string tname;
        tname.append(libname);
        tname.append("-");
        tname.append(name);
        if(runSecond == false) {
            tname.append(".1");
        } else {
            tname.append(".2");
        }
        tname.append(".c");
        write_file(compcode, tname);
        #endif
        // This time we are ACTUALLY running the code...thats far more interesting.
        if(pcom.first) {
            // It passed...
            switch(pcom.second.exit_code) {
                case 2:
                    cout << "No. Loading of '" << name << "' failed." << endl;
                    cache->set(libkey, "1", "detector");
                    os->pushBool(false);
                    break;
                case 3:
                    cout << "No. Loading of (prefixed) '_" << name << "' failed." << endl;
                    cache->set(libkey, "1", "detector");
                    os->pushBool(false);
                    break;
                // Now comes the positive stuff.
                case 4:
                    cout << "Yes." << endl;
                    cache->set("DLSYM_SYMBOL_PREFIX", "\"\"", "detector");
                    cache->set(libkey, "1", "detector");
                    cache->set(funckey, "1", "detector");
                    os->pushBool(true);
                    break;
                case 5:
                    cout << "Yes. Loaded using: '_" << name << "'" << endl;
                    cache->set("DLSYM_SYMBOL_PREFIX", "\"_\"", "detector");
                    cache->set("USE_DLSYM_SYMBOL_PREFIX", "1", "detector");
                    cache->set(libkey, "1", "detector");
                    cache->set(funckey, "1", "detector");
                    os->pushBool(true);
                    break;
                //case 6:
                //case 7:
                    // I need to grab a Windoze and test this myself.
                //    break;
                case 0:
                case 1:
                    // Either my error handling went wrong, OR we actually ran into an dlopen issue.
                    // Therefore we are going to re-test it to see if our guess is right or not.
                    if(runSecond) {
                        // Oops! We came here again. Horrible issue has landed!
                        cout << "Definitifely not. An awkward issue was found." << endl;
                        cache->set(libkey, "0", "detector");
                        os->pushBool(false);
                        break;
                    }
                    cout << "No. Library couldn't be loaded. Attempting re-run." << endl;
                    runSecond = true;
                    goto LIBFUNC_switcher;
            }
        } else {
            cout << "No. Reason: Compilation failed. Library probably missing." << endl;
            // Need to implement logging here.
            os->pushBool(false);
        }
    }

    return 1;
}


OS_FUNC(osd_headerfunc) {
    EXPECT_STRING(1)
    EXPECT_STRING(2)
    EXPECT_STRING(3)
    string kind = os->toString(-params+0).toChar();
    string header = os->toString(-params+1).toChar();
    string func = os->toString(-params+2).toChar();
    string name = key2name(kind2key(kind));
    string funckey = have_func(func);
    string hdrkey = have_header(header);
    string hdrfunckey = have_headerfunc(header, func);
    OUTPUT(cout) << "Checking for function " << func << " in " << name << " header <" << header << ">... ";

    string has_hf = cache->get(hdrfunckey, "detector");
    if(has_hf.empty()) {
        // We need to perform the lookup.
        stringstream source;
        source << "#include <" << header << ">" << endl
               << "int main(int argc, char** argv) {" << endl
               << "    if(" << func << " != (void*)0) {" << endl
               << "        return 0;" << endl
               << "    } else {" << endl
               << "        return 1;" << endl
               << "    }" << endl
               << "    return 2;" << endl
               << "}" << endl;
        pair<bool, CommandResult> comres = run_task(source.str(), kind);
        if(!comres.second.spawned || !comres.first) {
            cout << "No.";
            if(cli->check("-v")) cout << " Compilation failed.";
            cout << endl;
            os->pushBool(false);
        } else if(comres.second.exit_code==0) {
            cout << "Yes" << endl;
            cache->set(hdrfunckey, "1", "detector");
            cache->set(funckey, "1", "detector");
            cache->set(hdrkey, "1", "detector");
            os->pushBool(true);
        } else if(comres.second.exit_code==1) {
            cout << "No" << endl;
            cache->set(hdrkey, "0", "detector");
            os->pushBool(false);
        } else if(comres.second.exit_code==2) {
            cout << "Unknown (Unexpected error returned)" << endl;
            os->pushNull();
        }
    } else {
        if(has_hf == "1") {
            // Appearently it is in the cache, which means its defined!
            cout << "Yes";
            os->pushBool(true);
        } else {
            // It's 0...undefined.
            cout << "No";
            os->pushBool(false);
        }
        cout << " (Cached)" << endl;
    }
    cache->sync();
    return 1;
}
/*
OS_FUNC(osd_checkfunc) {}

OS_FUNC(osd_add_lib) {}
OS_FUNC(osd_add_libfunc) {}
OS_FUNC(osd_add_headerfunc) {}
OS_FUNC(osd_add_checkfunc) {}

OS_FUNC(osd_preprocess) {}
OS_FUNC(osd_transform) {}

OS_FUNC(osd_working_dir) {}
OS_FUNC(osd_cache_file) {}
OS_FUNC(osd_have_prefix) {}
OS_FUNC(osd_havelib_prefix) {}
*/

OS_FUNC(osd_write_header) {
    EXPECT_STRING(1)
    string arg1 = os->toString(-params+0).toChar();
    string haveit = have_header(arg1);
    // We have to gather the values
    stringstream hdr;

    // Begin constructing.
    hdr << "// Generated by IceTea" << endl;
    hdr << "#ifndef ICETEA_" << haveit << endl;
    hdr << "#define ICETEA_" << haveit << endl;

    hdr << "// Detected information" << endl;
    map<string,string> detector = cache->getMap("detector");
    map<string,string>::iterator it = detector.begin();
    if(!detector.empty()) {
        for(; it!=detector.end(); ++it) {
            if(it->second != "0") {
                // If the value is not plain 0, then we can use it.
                hdr << "#define " << it->first << " " << ReplaceString(it->second, "\\\"", "\"") << endl;
            }
        }
    }

    hdr << "// Defines..." << endl;
    map<string,string> defines = cache->getMap("defines");
    map<string,string>::iterator it2 = defines.begin();
    if(!defines.empty()) {
        for(; it2!=defines.end(); ++it2) {
            hdr << "#define " << it2->first << " " << ReplaceString(it2->second, "\\\"", "\"") << endl;
        }
    }

    hdr << "#endif // IceTea generated header." << endl;

    // Some nice output...
    OUTPUT(cout) << "Writing header: " << arg1 << endl;

    os->pushBool(write_file(hdr.str(), arg1));
    return 0;
}
//OS_FUNC(osd_write_json) {}


void CleanString(string& str) {
    ReplaceStringInPlace(str, " ", "_");
    ReplaceStringInPlace(str, ".", "_");
    ReplaceStringInPlace(str, "+", "_");
    ReplaceStringInPlace(str, "{", "_");
    ReplaceStringInPlace(str, "}", "_");
    ReplaceStringInPlace(str, ":", "_");
}

OS_FUNC(osd_set) {
    EXPECT_STRING(1)
    EXPECT_STRING(2)
    string key = os->toString(-params+0).toChar();
    string val = os->toString(-params+1).toChar();
    CleanString(key);
    cache->set(key, val, "defines");
    return 0;
}
OS_FUNC(osd_get) {
    EXPECT_STRING(1)
    string key = os->toString(-params+0).toChar();
    CleanString(key);
    os->pushString(cache->get(key, "defines").c_str());
    return 1;
}

OS_FUNC(osd_with) {
    EXPECT_STRING(1)
    EXPECT_STRING(2)
    EXPECT_STRING(3)
    string name;
    name.append("--with-");
    name.append(os->toString(-params+0).toChar());
    string desc = os->toString(-params+1).toChar();
    string arg = os->toString(-params+2).toChar();
    string def;
    if(os->isString(-params+3)) {
        def = os->toString(-params+3).toChar();
    } else {
        def = "on";
    }
    cli->insert("", name, arg, desc, true, def);
    return 0;
}
OS_FUNC(osd_hasWith) {
    EXPECT_STRING(1)
    string name;
    name.append("--with-");
    name.append(os->toString(-params+0).toChar());
    os->pushBool(cli->check(name));
    return 1;
}
OS_FUNC(osd_withValue) {
    EXPECT_STRING(1)
    string name;
    name.append("--with-");
    name.append(os->toString(-params+0).toChar());
    os->pushString(cli->value(name).c_str());
    return 1;
}
OS_FUNC(osd_enable) {
    EXPECT_STRING(1)
    EXPECT_STRING(2)
    string name;
    name.append("--enable-");
    name.append(os->toString(-params+0).toChar());
    string desc = os->toString(-params+1).toChar();
    cli->insert("", name, "", desc);
    return 1;
}
OS_FUNC(osd_enabled) {
    EXPECT_STRING(1)
    string name;
    name.append("--enable-");
    name.append(os->toString(-params+0).toChar());
    os->pushBool(cli->check(name));
    return 1;
}

OS_FUNC(osd_type) {
    EXPECT_STRING(1)
    string tname = os->toString(-params+0).toChar();
    string headerline;
    string header;
    string kind = "c";
    string typekey = have_type(tname);
    string cached = cache->get(typekey, "detector");
    string add;
    // Header?
    if(os->isString(-params+1)) {
        headerline.append("#include <");
        headerline.append(os->toString(-params+1));
        headerline.append(">");
        header = os->toString(-params+1).toChar();
    }
    // Kind specification
    if(os->isString(-params+2)) {
        kind = os->toString(-params+2).toChar();
    }
    // Include flags
    if(os->isString(-params+3)) {
        add = os->toString(-params+3).toChar();
    }
    stringstream src;
    if(!headerline.empty()) src << headerline << endl;
    src << "int main(int argc, char** argv) {" << endl;
    src << "    " << tname << " tmp;" << endl;
    src << "    return 0;" << endl;
    src << "}" << endl;

    pair<bool,CommandResult> rt = run_task(src.str(), kind, add, false);

    OUTPUT(cout) << "Checking for " << key2name(kind2key(kind)) << " type " << tname;
    if(!header.empty()) cout << " in header <" << header << ">";
    cout << "...";

    if(cached.empty()) {
        // Do it
        if(rt.first) {
            // It went trough.
            if(rt.second.exit_code == 0) {
                cout << " Yes." << endl;
                cache->set(typekey, "1", "detector");
            } else {
                cout << " No." << endl;
                cache->set(typekey, "0", "detector");
            }
        }
        os->pushBool((rt.first && rt.second.exit_code == 0));
    } else {
        if(cached == "0") {
            cout << "Yes (cached)" << endl;
            os->pushBool(true);
        } else {
            cout << "No (cached)" << endl;
            os->pushBool(false);
        }
    }
    cache->sync();
    return 1;
}

int osd_tryTask(OS* os, int params, bool run) {
    EXPECT_STRING(1)
    string source = os->toString(-params+0).toChar();
    string kind;
    if(os->isString(-params+1)) {
        kind = os->toString(-params+1).toChar();
    } else {
        kind = "c";
    }
    string add="";
    if(os->isString(-params+2))
        add = os->toString(-params+2).toChar();
    pair<bool,CommandResult> rt = run_task(source, kind, add, run);

    // Convert the pair into two return values. its a bit different than $(...)
    // but thats on purpose. This function is ment for "internal" use. Tests, etc.
    os->pushBool(rt.first);
    os->newObject();
    os->newArray();
    os->pushString(rt.second.streams[0].c_str());
    os->addProperty(-2);
    os->pop(2);
    os->pushString(rt.second.streams[1].c_str());
    os->addProperty(-2);
    os->pop(2);
    os->pushString(rt.second.streams[2].c_str());
    os->addProperty(-2);
    os->pop(2);
    os->setProperty(-2, "streams");
    os->pop();
    os->pushNumber(rt.second.exit_code);
    os->setProperty(-2, "exit_code");
    os->pop();

    return 2;
}

OS_FUNC(osd_tryRun) {
    return osd_tryTask(os, params, true);
}
OS_FUNC(osd_tryBuild) {
    return osd_tryTask(os, params, false);
}

OS_FUNC(osd_head) {
    stringstream st;
    HEADER(st);
    os->pushString(st.str().c_str());
    return 1;
}
OS_FUNC(osd_out) {
    stringstream st;
    OUTPUT(st);
    os->pushString(st.str().c_str());
    return 1;
}
OS_FUNC(osd_footer) {
    stringstream st;
    FOOTER(st);
    os->pushString(st.str().c_str());
    return 1;
}

OS_FUNC(osd_havelib) {
    EXPECT_STRING(1)
    os->pushString(
        have_lib(os->toString(-params+0).toChar()).c_str()
    );
    return 1;
}
OS_FUNC(osd_havefunc) {
    EXPECT_STRING(1)
    os->pushString(
        have_func(os->toString(-params+0).toChar()).c_str()
    );
    return 1;
}
OS_FUNC(osd_havetool) {
    EXPECT_STRING(1)
    os->pushString(
        have_tool(os->toString(-params+0).toChar()).c_str()
    );
    return 1;
}
OS_FUNC(osd_haveheader) {
    EXPECT_STRING(1)
    os->pushString(
        have_header(os->toString(-params+0).toChar()).c_str()
    );
    return 1;
}

OS_FUNC(osd_find_tool) {
    if(os->isArray(-params+0)) {
        int len = os->getLen();
        vector<string> cmds;
        for(int i=0; i<len; i++) {
            os->pushNumber(i);
            os->getProperty();
            cmds.push_back(os->toString().toChar());
            os->pop(2);
        }
        os->pushString(find_tool(cmds).c_str());
    } else {
        os->pushBool(false);
    }
    return 1;
}

OS_FUNC(osd_kind2key) {
    EXPECT_STRING(1)
    string kind = os->toString(-params+0).toChar();
    os->pushString(kind2key(kind).c_str());
    return 1;
}

OS_FUNC(osd_key2name) {
    EXPECT_STRING(1)
    string key = os->toString(-params+0).toChar();
    os->pushString(key2name(key).c_str());
    return 1;
}

OS_FUNC(osd_key2ext) {
    EXPECT_STRING(1)
    string key = os->toString(-params+0).toChar();
    os->pushString(key2ext(key).c_str());
    return 1;
}

OS_FUNC(osd_tool) {
    EXPECT_STRING(1)
    string tool = os->toString().toChar();
    string toolkey;
    if(os->isString(-params+1)) {
        toolkey = os->toString(-params+1).toChar();
    } else {
        toolkey = have_tool(tool);
    }
    string cached = cache->get(toolkey, "detector");
    static map<string,bool> toolsfound;
    bool doPrint=true;
    if(toolsfound.find(toolkey) != toolsfound.end()) {
        doPrint = false;
    }
    if(doPrint) OUTPUT(cout) << "Checking for " << tool << "... ";
    if(cached.empty()) {
        string realtool = path_lookup(tool);
        if(realtool.empty()) {
            if(doPrint) cout << "No" << endl;
            cache->set(toolkey, "0", "detector");
            os->pushBool(false);
        } else {
            if(doPrint) cout << realtool << endl;
            cache->set(toolkey, realtool, "detector");
            os->pushBool(true);
        }
        os->pushString(realtool.c_str());
        cache->sync();
    } else {
        if(cached == "0") {
            if(doPrint) cout << "No (Cached)" << endl;
            os->pushBool(false);
            os->pushString("");
        } else {
            if(doPrint) cout << cached << " (Cached)" << endl;
            os->pushBool(true);
            os->pushString(cached.c_str());
        }
    }
    return 2;
}

OS_FUNC(osd_toolFlag) {
    EXPECT_STRING(1)
    EXPECT_STRING(2)
    string confVar = os->toString(-params+0).toChar();
    string flag = os->toString(-params+1).toChar();
    string cmd;
    string tool = cache->get(confVar, "detector");
    if(!tool.empty()) {
        cmd.append(tool);
        cmd.append(" ");
        cmd.append(flag);
        CommandResult res = it_cmd(cmd, vector<string>());
        os->pushBool(res.spawned);
        os->pushNumber(res.exit_code);
    } else {
        // Should I throw an exception?
        os->pushBool(false);
        os->pushNull();
    }
    return 2;
}

// Cache objects
OS_FUNC(osc_set) {
    int _this = os->getAbsoluteOffs(-params-1);
    EXPECT_STRING(1)
    string key = os->toString(-params+0).toChar();
    string val = os->toString(-params+1).toChar();
    os->getProperty(_this, "cache_name", false);
    string obj = os->toString().toChar();

    // Fast-escape value
    ReplaceStringInPlace(val, "\"", "\\\"");

    cache->set(key, val, obj);
    return 0;
}
OS_FUNC(osc_get) {
    int _this = os->getAbsoluteOffs(-params-1);
    EXPECT_STRING(1)
    string key = os->toString().toChar();
    os->getProperty(_this, "cache_name", false);
    string obj = os->toString().toChar();
    os->pushString(cache->get(key, obj).c_str());
    return 1;
}
OS_FUNC(osc_del) {
    int _this = os->getAbsoluteOffs(-params-1);
    EXPECT_STRING(1)
    string key = os->toString().toChar();
    os->getProperty(_this, "cache_name", false);
    string obj = os->toString().toChar();
    os->pushBool(cache->remove(key, obj));
    return 1;
}
OS_FUNC(osc_sync) {
    os->pushBool(cache->sync());
    return 1;
}
OS_FUNC(osc_new) {
    // __construct(name)
    EXPECT_STRING(1)
    string name = os->toString(-params+0).toChar();
    int _this = os->getAbsoluteOffs(-params-1);
    os->pushString(name.c_str());
    os->setProperty(_this, "cache_name", false);
    os->pushStackValue(_this);
    return 1;
}


bool initializeDetector(OS* os, Filecache* _cache, CLI* _cli) {
    initCompilers();
    cache = _cache;
    OS::FuncDef dtFuncs[] = {
        // Display and output
        {OS_TEXT("__get@head"), osd_head},
        {OS_TEXT("__get@out"), osd_out},
        {OS_TEXT("__get@footer"), osd_footer},
        // Actual methods
        {OS_TEXT("header"), osd_header},
        {OS_TEXT("lib"), osd_lib},
        {OS_TEXT("compiler"), osd_compiler},
        {OS_TEXT("func"), osd_func},
        {OS_TEXT("libfunc"), osd_libfunc},
        {OS_TEXT("headerfunc"), osd_headerfunc},
        {OS_TEXT("type"), osd_type},
        {OS_TEXT("tool"), osd_tool},
        {OS_TEXT("toolFlag"), osd_toolFlag},

        // For your own tests
        {OS_TEXT("tryRun"), osd_tryRun},
        {OS_TEXT("tryBuild"), osd_tryBuild},

        // Flags
        {OS_TEXT("enable"), osd_enable},
        {OS_TEXT("enabled"), osd_enabled},
        {OS_TEXT("with"), osd_with},
        {OS_TEXT("hasWith"), osd_hasWith},
        {OS_TEXT("withValue"), osd_withValue},

        // Inner cache
        {OS_TEXT("define"), osd_set},
        {OS_TEXT("set"), osd_set},
        {OS_TEXT("get"), osd_get},

        // Utilities
        {OS_TEXT("have_lib"), osd_havelib},
        {OS_TEXT("have_func"), osd_havefunc},
        {OS_TEXT("have_header"), osd_haveheader},
        {OS_TEXT("have_tool"), osd_havetool},
        {OS_TEXT("find_tool"), osd_find_tool},
        {OS_TEXT("kind2key"),  osd_kind2key},
        {OS_TEXT("key2name"), osd_key2name},
        {OS_TEXT("key2ext"), osd_key2ext},

        // Output
        {OS_TEXT("write_header"), osd_write_header},
        {}
    };
    os->getModule("detect");
    os->setFuncs(dtFuncs);
    os->pop();

    OS::FuncDef chFuncs[] = {
        {OS_TEXT("__construct"), osc_new},
        {OS_TEXT("__get"), osc_get},
        {OS_TEXT("__del"), osc_del},
        {OS_TEXT("__set"), osc_set},
        {OS_TEXT("sync"), osc_sync},
        {}
    };
    os->getModule("cache");
    int cmid = os->getAbsoluteOffs(-1);
    os->setFuncs(chFuncs, false);
    os->pushBool(true);
    os->setProperty(cmid, "__instantiable", false);
    os->pop();

    return true;
}
