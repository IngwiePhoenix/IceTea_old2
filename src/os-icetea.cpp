#include <iostream>

#include "objectscript.h"
#include "os-value.h"
#include "os-icetea.h"
#include "os-exec.h"
#include "os-pfs.h"
#include "os-sys.h"
#include "os-configurable.h"

#include "picosha2.h"

#include "predef.h"

#include "cli.h"

using namespace std;
using namespace ObjectScript;
using namespace picosha2;

// Pretending some globals...
extern Value::Object* targets;
extern Value::Object* rules;
extern Value::Object* actions;


/* The weirdest function you've ever seen.

        target("name", "rule") { options };

    It's actually legit ObjectScript code.
    In order for it to work, we need 2 C functions and a struct.
    And also a few defines to tell which kind we want to use.
*/
#define IT_TARGET 0x0
#define IT_RULE 0x1
#define IT_ACTION 0x2

struct callInfo_t {
    string name;
    string rule;
    int type;
};

OS_FUNC(shotgunner_2) {
    callInfo_t* ce = (callInfo_t*)userData;
    // Get this, before we loose control.
    int offset = os->getAbsoluteOffs(-params+0);
    if(ce->type == IT_TARGET) {
        // Add to the parameter object...
        os->pushString(ce->rule.c_str());
        os->setProperty(offset, "_");
        os->pushValueById(targets->valueID);
    } else if(ce->type == IT_RULE) {
        os->pushString(ce->rule.c_str());
        os->setProperty(offset, "display");
        os->pushValueById(rules->valueID);
    } else {
        // Assuming action, which doesnt need the 2nd param.
        os->pushValueById(actions->valueID);
    }

    //os->pushValueById(targets->valueID); // FIXME: Could be nicer. Value::* needs setter.
    os->pushStackValue(offset);
    os->setProperty(-2, ce->name.c_str());
    os->pop();
    return 0;
}

OS_FUNC(shotgunner) {
    callInfo_t* ce = new callInfo_t;
    ce->name = string( os->toString(-params+0).toChar() );
    ce->rule = string( os->toString(-params+1).toChar() );
    ce->type = *((int*)(&userData)); // void* -> int* -> int ... please FIXME.
    os->pushCFunction(shotgunner_2, (void*)ce);
    return 1;
}

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
    if(cli->check(key)) {
        os->pushString( cli->value(key).c_str() );
        return 1;
    } else {
        os->pushNull();
        return 1;
    }
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

void initObj(OS* os) {
    // Now, initialize our objects:
    // FIXME: Value::Object and Value::Array could REALLY create new objects/arrays too...
    os->newObject();
    targets = new Value::Object(os);
    os->pop();

    os->newObject();
    rules = new Value::Object(os);
    os->pop();

    os->newObject();
    actions = new Value::Object(os);
    os->pop();
}


void initIceTeaExt(OS* os, CLI* cli) {
    // First things first.
    initObj(os);

    // Bring it in, the targeter! Will be suited for Targets and Rules.
    os->pushCFunction(shotgunner, (void*)IT_TARGET);
    os->setGlobal("target");
    os->pushCFunction(shotgunner, (void*)IT_RULE);
    os->setGlobal("rule");
    os->pushCFunction(shotgunner, (void*)IT_ACTION);
    os->setGlobal("action");

    // Exec
    os->pushCFunction(os_exec);
    os->setGlobal("$");

    // Option parsing/passing
    OS::FuncDef cliFuncs[] = {
        {OS_TEXT("insert"), cli_insert, (void*)cli},
        {OS_TEXT("value"), cli_value, (void*)cli},
        {OS_TEXT("check"), cli_check, (void*)cli},
        {OS_TEXT("group"), cli_group, (void*)cli},
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
    // detect, cDetect in OS
    // Configurables module
    // Misc
    initializeSYS(os);
    initializeConfigurable(os);
}
