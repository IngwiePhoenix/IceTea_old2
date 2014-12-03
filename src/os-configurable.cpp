#include "objectscript.h"
#include "os-icetea.h" // OS_FUNC
#include "os-pfs.h" // EXPECT_STRING
#include "os-configurable.h"
#include "os-stacker.h"

using namespace std;
using namespace ObjectScript;

/*
    Syntax: configurable(name) function(head, body)
*/

// We could use OSValue, but this is faster.
struct FuncVal {
    int _id;
    int _argId;
    OS* _os;
    FuncVal(OS* os, int val):_id(val), _os(os) { os->retainValueById(_id); }
    ~FuncVal() { _os->releaseValueById(_id); }
};

// The method that receives the name argument...
OS_FUNC(os_configurable_1) {
    EXPECT_STRING(1)
    string* name = new string(os->toString(-params+0).toChar());
    os->pushCFunction(os_configurable_2, (void*)name);
    return 1;
}

// The function that gets the callback as argument.
// userData == string.
OS_FUNC(os_configurable_2) {
    string* name = (string*)userData;
    if(os->isFunction(-params+0)) {
        int id = os->getValueId(-params+0);
        FuncVal* fn = new FuncVal(os,id);
        os->pushGlobals();
        os->pushCFunction(os_configurable_3, (void*)fn);
        os->setProperty(-2, name->c_str());
    } else {
        os->setException("A configurable requires a callback.");
    }
    delete name;
    return 0;
}

// This function is called if the name of the configurable is called.
OS_FUNC(os_configurable_3) {
    os->newArray();
    for(int i=0; i<params; i++) {
        os->pushStackValue(-params-1+i);
        os->addProperty(-2);
    }
    int vid = os->getValueId(-1);
    ((FuncVal*)userData)->_argId = vid;
    os->retainValueById(vid);
    os->pushCFunction(os_configurable_4, userData);
    return 1;
}

// Now we actually call the configurable.
OS_FUNC(os_configurable_4) {
    FuncVal* fn = (FuncVal*)userData;
    os->pushValueById(fn->_id);
    os->pushValueById(fn->_argId);
    os->move(-3, -1);
    // A configurable may only return one value.
    os->callF(2, 1);
    os->releaseValueById(fn->_argId);
    os->releaseValueById(fn->_id);
    return 1;
}

bool initializeConfigurable(OS* os) {
    os->pushCFunction(os_configurable_1);
    os->setGlobal("configurable");
    return true;
}
