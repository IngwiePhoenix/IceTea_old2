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
#include "Plugin.h"

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

bool initializeStdlib(IceTea* os) {
    os->pushCFunction(os_exit);
    os->setGlobal("exit");

    os->pushCFunction(os_die);
    os->setGlobal("abort");

    OS::FuncDef wcFuncs[] = {
        {OS_TEXT("match"), wildcard_match},
        {}
    };
    os->getModule("wildcard");
    os->setFuncs(wcFuncs);
    os->pop();

    return true;
}
ICETEA_MODULE(stdlib, initializeStdlib);
