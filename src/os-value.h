#include <iostream>
#include "objectscript.h"

namespace ObjectScript {
namespace Value {

    // The base class.
    class Var {
    protected:
        int offset;
    public:
        OS_EValueType type;
        OS* myOS;
        int valueID;
        Var(OS* os, int off);
    };

    // Null, Bool, Int/Float
    class Primitive : public Var {
    private:
        int iVal;
        float fVal;
        OS_NUMBER nVal;
        bool bVal;
    public:
        Primitive(OS* os, int off);
        Primitive(OS* os);
        ~Primitive();
        bool initialize();
        operator int();
        operator float();
        operator OS_NUMBER();
        operator bool();
    };

    class String : public Var {
    public:
        String(OS* os, int off);
        String(OS* os);
        ~String();
        bool initialize();
        operator const char*();
        operator std::string();
    };

    // Array containing primitives or Strings
    class Array : public Var {
    public:
        Array(OS* os, int off);
        Array(OS* os);
        ~Array();
        bool initialize();
        Var* operator [](int index);
        int length();
    };

    // key=>value pairs
    class Object : public Var {
    public:
        Object(OS* os, int off);
        Object(OS* os);
        ~Object();
        bool initialize();
        Var* operator [](std::string index);
    };

    // Callables
    class Function : public Var {
    public:
        Function(OS* os, int off);
        Function(OS* os);
        ~Function();
        bool initialize();
        void operator ()(int args, int rtVals);
    };

    class Method {
    private:
        OS* myOS;
        Object* Obj;
        Function* Mtd;
    public:
        Method(OS* os, Object* obj, const char* methodName);
        ~Method();
        void operator ()(int args, int rtVals);
    };

}
}
