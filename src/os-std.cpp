/**
    @file
    @brief ObjectScript standart library extension

    This file contains a minimal extension that adds useful methods to objects and arrays
    and offers functions for general use.
*/

#include <iostream>
#include <vector>

#include "objectscript.h"
#include "os-std.h"

#include "os-icetea.h" // OS_FUNC
#include "os-stacker.h"

using namespace std;
using namespace ObjectScript;

/// The array `+` operator.
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

/// The Object `+` operator
OS_FUNC(object_add) {
    // This is by far more complicated...
    int _this = os->getAbsoluteOffs(-params-1);
    int _arg = os->getAbsoluteOffs(-params+0);
    if(os->isNull() || os->isString() || os->isNumber() || os->isArray()) {
        // Add this to the end of the object.
        os->addProperty(_this);
    } else {
        // This is an object. So we have to do this...
        os->getProperty(_arg, "keys");
        vector<string> keys;
        int len = os->getLen();
        for(int i=0; i<len; i++) {
            os->getProperty(_arg, "keys");
            os->pushNumber(i);
            os->getProperty();
            keys.push_back(os->toString().toChar());
            os->pop();
        }
        for(vector<string>::iterator it=keys.begin(); it!=keys.end(); ++it) {
            os->getProperty(_arg, it->c_str());
            if(os->isNull() || os->isNumber()) {
                // We are only going to replace the original value.
                os->setProperty(_this, it->c_str());
                //os->pop(2);
            } else if(os->isString()) {
                // Oha. concat the string.
                os->pushStackValue(_this);
                os->pushString(it->c_str());
                if(!os->in(-1,-2)) {
                    // Value does not exist, create it.
                    os->pop(2);
                    os->setProperty(_this, it->c_str());
                } else {
                    os->pop(2);
                    // The string exists, so we gonna concat it.
                    string val = os->toString().toChar();
                    string res;
                    os->pop();
                    os->getProperty(_this, it->c_str());
                    res.append(os->popString().toChar());
                    res.append(val);
                    os->pushString(res.c_str());
                    os->setProperty(_this, it->c_str());
                    os->pop();
                }
            } else if(os->isArray() || os->isObject()) {
                // Conveniently, the Array.__add method will solve this easily.
                // But we want to have the array be the right-hand operator here.
                os->getProperty(_this, it->c_str());
                int voff = os->getValueId(-1);
                os->pop();
                // Now set _arg to be the value
                os->setProperty(_this, it->c_str());
                os->getProperty(_this, it->c_str());
                os->getProperty("__add");
                os->getProperty(_this, it->c_str());
                os->pushValueById(voff);
                if(os->isNull()) {
                    // The array that should be combined with...doesnt really contain.
                    os->pop(2);
                } else {
                    os->callFT(1,1);
                    os->pop(3);
                }
            }
        }
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

    os->getGlobalObject("Object");
    os->pushCFunction(object_add);
    os->setProperty(-2, "__add");
    os->pop();
}
