#include <iostream>

#include "objectscript.h"
#include "os-std.h"

#include "os-icetea.h" // OS_FUNC
#include "os-stacker.h"

using namespace std;
using namespace ObjectScript;

OS_FUNC(array_add) {
    int _this = os->getAbsoluteOffs(-params-1);
    if(os->isString(-params+0) || os->isNumber(-params+0) || os->isNull(-params+0)) {
        os->pushStackValue(-params+0);
        os->addProperty(_this);
    } else if(os->isArray(-params+0)) {
        int arrlen = os->getLen(-params+0);
        int arrval = os->getAbsoluteOffs(-params+0);
        for(int i=0; i<arrlen; i++) {
            os->pushStackValue(arrval);
            os->pushNumber(i);
            os->getProperty(arrval);
            os->addProperty(_this);
        }
    } else if(os->isObject(-params+0)) {
        os->setException("Don't know how to handle array+object...");
        return 0;
    }
    os->pushStackValue(_this);
    return 1;
}

OS_FUNC(os_exit) {
    os->setTerminated(true, os->toNumber(-params+0));
    return 0;
}

OS_FUNC(os_die) {
    if(!os->isString(-params+0)) {
        os->pushBool(false);
        return 1;
    }
    cout << os->toString(-params+0);
    os->setTerminated(true, 1);
    return 0;
}

void initializeStdlib(OS* os) {
    os->pushCFunction(os_exit);
    os->setGlobal("exit");

    os->pushCFunction(os_die);
    os->setGlobal("abort");

    os->getGlobalObject("Array");
    os->pushCFunction(array_add);
    os->setProperty(-2, "__add");
    os->pop();
}
