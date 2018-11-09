/**
    @file
    @brief ObjectScript standart library extension

    This file contains a minimal extension that adds useful methods to objects and arrays
    and offers functions for general use.
*/

#include <iostream>
#include <vector>

#include "IceTea.h"
#include "os-icetea.h" // OS_FUNC
#include "os-stacker.h"

#include "wildcard.hpp"
#include "InternalIceTeaPlugin.h"

using namespace std;
using namespace ObjectScript;
using namespace stlplus;

OS_FUNC(wildcard_match) {
    if(params < 2) {
        os->setException("Two parameters are required.");
        return 0;
    }
    OS::String pat = os->toString(-params+0);
    OS::String str = os->toString(-params+1);

    os->pushBool(wildcard(pat.toChar(), str.toChar()));
    return 1;
}

OS_FUNC(os_exit) {
    os->setTerminated(true, os->toNumber(-params+0));
    return 0;
}

OS_FUNC(os_die) {
    if(os->isString(-params+0)) {
        cout << os->toString(-params+0);
    }
    os->setTerminated(true, 1);
    return 0;
}

class IceTeaSTD: public IceTeaPlugin {
public:
    bool configure(IceTea* os) {
        os->pushCFunction(os_exit);
        os->setGlobal("exit");

        os->pushCFunction(os_die);
        os->setGlobal("abort");

        os->getModule("wildcard");
        os->pushCFunction(wildcard_match);
        os->setProperty(-2, "match");

        return true;
    }
    string getName() {
        return "Standard Library Extension";
    }
    string getDescription() {
        return  "Partially native, partially in OS itself, this extends ObjectScript's primitives.\n"
                "For example:\n"
                "- Add += support for arrays and objects\n"
                "- Add a public wildcard() function for glob-like matching\n"
                "- Provide exit() and abort()";
    }
};
ICETEA_INTERNAL_MODULE(IceTeaSTD);
