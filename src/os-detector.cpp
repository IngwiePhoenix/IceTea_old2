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

typedef map<string, compiler_t*> compiler_map_t;
typedef map< string, compiler_map_t > compiler_list_t;
compiler_list_t compilers;
void initCompilers() {
    // C compilers and their flags...
    compilers["CC"]["gcc"]      = new compiler_t("-l %", "-I", "-c", "-o");
    compilers["CC"]["clang"]    = new compiler_t("-l %", "-I", "-c", "-o");
    compilers["CC"]["cl"]       = new compiler_t("/nologo", "lib%.lib", "/I", "/c", "/Fo");
    compilers["CXX"]["clang-cl"]= new compiler_t("%.lib", "/I", "/c", "/Fo");

    // C++ compilers
    compilers["CXX"]["g++"]     = new compiler_t("-l %", "-I", "-c", "-o");
    compilers["CXX"]["clang++"] = new compiler_t("-l %", "-I", "-c", "-o");
    compilers["CXX"]["cl"]      = new compiler_t("/nologo", "%.lib", "/I", "/c", "/Fo");
    compilers["CXX"]["clang-cl"]= new compiler_t("lib%.lib", "/I", "/c", "/Fo");

    // Objective-C
    compilers["OBJC"]["gcc"]     = new compiler_t("-l %", "-I", "-c", "-o");
    compilers["OBJC"]["clangcc"] = new compiler_t("-l %", "-I", "-c", "-o");

    // Objective-C++
    compilers["OBJCXX"]["g++"]     = new compiler_t("-l %", "-I", "-c", "-o");
    compilers["OBJCXX"]["clang++"] = new compiler_t("-l %", "-I", "-c", "-o");

    // Java
    compilers["JAVAC"]["javac"] = new compiler_t();

    // Swift
    compilers["SWIFTC"]["swiftc"] = new compiler_t();
}

string have_prefix("HAVE_");
string havelib_prefix("HAVE_LIB");
string havefunc_prefix("HAVE_");
Filecache* cache;

string toUpper(const string str) {
    string s = str;
    transform(s.begin(), s.end(), s.begin(), ::toupper);
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
        cache->set(skey, string(env_val), "detector");
        cache->sync();
        found[skey]=true;
        return string(env_val);
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
            cache->set(skey, rt, "detector");
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

OS_FUNC(osd_heder) {
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
        LDFLAGS.append(ReplaceString(compilers["CC"][filename_part(tool)]->libflag, "%", libdl));
        if(runSecond) {
            LDFLAGS.append(" ");
            LDFLAGS.append(ReplaceString(compilers["CC"][filename_part(tool)]->libflag, "%", libname));
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
                    cache->set("DLSYM_SYMBOL_PREFIX", "", "detector");
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

/*
OS_FUNC(osd_headerfunc) {}
OS_FUNC(osd_checkfunc) {}
OS_FUNC(osd_add_lib) {}
OS_FUNC(osd_add_libfunc) {}
OS_FUNC(osd_add_headerfunc) {}
OS_FUNC(osd_add_checkfunc) {}
OS_FUNC(osd_with) {}
OS_FUNC(osd_hasWith) {}
OS_FUNC(osd_withValue) {}
OS_FUNC(osd_enable) {}
OS_FUNC(osd_enabled) {}
OS_FUNC(osd_tryBuild) {}
OS_FUNC(osd_tryRun) {}
OS_FUNC(osd_preprocess) {}
OS_FUNC(osd_tool) {}
OS_FUNC(osd_toolFlag) {}
OS_FUNC(osd_transform) {}
OS_FUNC(osd_write_header) {}
OS_FUNC(osd_write_json) {}
OS_FUNC(osd_set) {}
OS_FUNC(osd_get) {}
OS_FUNC(osd_working_dir) {}
OS_FUNC(osd_cache_file) {}
OS_FUNC(osd_have_prefix) {}
OS_FUNC(osd_havelib_prefix) {}
*/

bool initializeDetector(OS* os, Filecache* _cache, CLI* cli) {
    initCompilers();
    cache = _cache;
    OS::FuncDef dtFuncs[] = {
        // Display and output
        {OS_TEXT("__get@head"), osd_head},
        {OS_TEXT("__get@out"), osd_out},
        {OS_TEXT("__get@footer"), osd_footer},
        // Actual methods
        {OS_TEXT("header"), osd_heder},
        {OS_TEXT("lib"), osd_lib},
        {OS_TEXT("compiler"), osd_compiler},
        {OS_TEXT("func"), osd_func},
        {OS_TEXT("libfunc"), osd_libfunc},
        {}
    };
    os->getModule("detect");
    os->setFuncs(dtFuncs);
    os->pop();

    /*
    OS::FuncDef chFuncs[] = {
        {OS_FUNC("__set"), osc_set},
        {OS_FUNC("__get"), osc_get},
        {OS_TEXT("sync"), osc_sync}
    };
    os->getModule("cache");
    os->setFuncs(chFuncs);
    os->pop();
    */
    return true;
}
