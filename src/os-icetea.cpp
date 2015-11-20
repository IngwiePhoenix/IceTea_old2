#include <iostream>
#include <vector>

#include "objectscript.h"
#include "os-value.h"
#include "os-icetea.h"
#include "os-exec.h"
#include "os-pfs.h"
#include "os-sys.h"
#include "os-configurable.h"
#include "os-console.h"

#include "picosha2.h"

#include "predef.h"

#include "cli.h"

using namespace std;
using namespace ObjectScript;
using namespace picosha2;

OS_FUNC(os_sh2_string) {
    // We can be cool and export this, so it can be used inside build.it's.
    std::string src_str = string( os->popString(-params+0).toChar() );
    std::string hash_hex_str;
    hash256_hex_string(src_str, hash_hex_str);
    os->pushString( hash_hex_str.c_str() );
    return 1;
}

OS_FUNC(cli_insert) {
    CLI* cli = (CLI*)userData;
    string shortopt;
    string longopt;
    string arg="";
    string desc="";
    bool optional=true;
    string defaultValue="";
    // We're expecting the first param to be an object.
    if(!os->isObject(-params+0)) {
        os->setException("cli.insert expects first parameter to be an object.");
        return 0;
    } else {
        int off = os->getAbsoluteOffs(-params+0);
        os->pushString("shortopt"); // -2
        os->pushString("longopt"); // -1
        if(os->in(-2, off) && !os->in(-1, off)) {
            // Shortopt was supplied.
            os->getProperty(off, "shortopt");
            shortopt = string( os->popString().toChar() );
            longopt = "";
        } else if(!os->in(-2, off) && os->in(-1, off)) {
            // Longopt only was supplied
            os->getProperty(off, "longopt");
            shortopt = "";
            longopt = string( os->popString().toChar() );
        } else if(os->in(-2, off) && os->in(-1, off)) {
            // We goth both.
            os->getProperty(off, "longopt");
            longopt = string( os->popString().toChar() );
            os->getProperty(off, "shortopt");
            shortopt = string( os->popString().toChar() );
        } else if(!os->in(-2, off) && !os->in(-1, off)) {
            os->setException("cli.insert expects either shortopt OR longopt OR both to be present.");
            return 0;
        }

        // Test for some other stuff.
        os->pushString("arg");
        if(os->in(-1, off)) {
            os->getProperty(off, "arg");
            arg = string(os->popString().toChar());
        }
        os->pushString("desc");
        if(os->in(-1, off)) {
            os->getProperty(off, "desc");
            desc = string(os->popString().toChar());
        }
        os->pushString("optional");
        if(os->in(-1, off)) {
            os->getProperty(off, "optional");
            optional = os->toBool();
            os->pop();
        }
        os->pushString("default");
        if(os->in(-1, off)) {
            os->getProperty(off, "default");
            defaultValue = string(os->popString().toChar());
        }
        cli->insert(shortopt, longopt, arg, desc, optional, defaultValue);
        return 0;
    }
}
OS_FUNC(cli_value) {
    CLI* cli = (CLI*)userData;
    string key( os->toString(-params+0).toChar() );
    os->pushString( cli->value(key).c_str() );
    return 1;
}
OS_FUNC(cli_check) {
    CLI* cli = (CLI*)userData;
    string key( os->toString(-params+0).toChar() );
    os->pushBool( cli->check(key) );
    return 1;
}

OS_FUNC(cli_group) {
    CLI* cli = (CLI*)userData;
    string group( os->toString(-params+0).toChar() );
    cli->group(group);
    return 0;
}

OS_FUNC(cli_parse) {
    CLI* cli = (CLI*)userData;
    cli->parse();
    return 0;
}

OS_FUNC(cli_usage) {
    CLI* cli = (CLI*)userData;
    cli->usage();
    return 0;
}

OS_FUNC(cli_getStrayArgs) {
    CLI* cli = (CLI*)userData;
    vector<string> stray = cli->getStrayArgs();
    os->newArray();
    vector<string>::iterator it = stray.begin();
    for(; it!=stray.end(); ++it) {
        os->pushString((*it).c_str());
        os->addProperty(-2);
    }
    return 1;
}

void initIceTeaExt(OS* os, CLI* cli) {
    // Exec
    os->pushCFunction(os_exec);
    os->setGlobal("$");
    os->pushCFunction(os_system);
    os->setGlobal("shell");

    // Extend the $ with console funcs.
    extendDollar(os);


    // Option parsing/passing
    OS::FuncDef cliFuncs[] = {
        {OS_TEXT("insert"), cli_insert, (void*)cli},
        {OS_TEXT("value"), cli_value, (void*)cli},
        {OS_TEXT("__get"), cli_value, (void*)cli},
        {OS_TEXT("check"), cli_check, (void*)cli},
        {OS_TEXT("group"), cli_group, (void*)cli},
        {OS_TEXT("parse"), cli_parse, (void*)cli},
        {OS_TEXT("usage"), cli_usage, (void*)cli},
        {OS_TEXT("__get@stray"), cli_getStrayArgs, (void*)cli},
        {OS_TEXT("getStrayArgs"), cli_getStrayArgs, (void*)cli},
        {}
    };
    os->getModule("cli");
    os->setFuncs(cliFuncs);
    os->pop();

    // Hashing
    OS::FuncDef sh2Funcs[] = {
        {OS_TEXT("string"), os_sh2_string},
        {}
    };
    os->getModule("sha2");
    os->setFuncs(sh2Funcs);
    os->pop();

    // PFS, aka our FS module
    initializePFS(os);

    // Misc
    initializeSYS(os);
    initializeConfigurable(os);
}
