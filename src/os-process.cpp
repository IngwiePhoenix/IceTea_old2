#include <iostream>

#include "IceTea.h"
#include "os-icetea.h"
#include "InternalIceTeaPlugin.h"

using namespace std;
using namespace ObjectScript;

OS_FUNC(stdout_write) {
    cout << os->toString(-params+0) << flush;
    return 0;
}

OS_FUNC(stderr_write) {
    cerr << os->toString(-params+0) << flush;
    return 0;
}

OS_FUNC(stdout_flush) {
    cout << flush;
    return 0;
}

OS_FUNC(stderr_flush) {
    cerr << flush;
    return 0;
}

class IceTeaProcess: public IceTeaPlugin {
public:
    bool configure(IceTea* os) {
        OS::FuncDef stdoutFuncs[] = {
            {OS_TEXT("write"), stdout_write},
            {OS_TEXT("flush"), stdout_flush},
            {}
        };
        OS::FuncDef stderrFuncs[] = {
            {OS_TEXT("write"), stderr_write},
            {OS_TEXT("flush"), stderr_flush},
            {}
        };

        os->getModule("process");
        int processObj = os->getAbsoluteOffs(-1);

        os->newObject();
        os->setFuncs(stdoutFuncs);
        os->setProperty(processObj, "stdout");

        os->newObject();
        os->setFuncs(stderrFuncs);
        os->setProperty(processObj, "stderr");

        os->pop();

        return true;
    }
    string getName() {
        return "Process";
    }
    string getDescription() {
        return "Provides stdout and stderr raw access. Might do more in the future...";
    }
};
ICETEA_INTERNAL_MODULE(IceTeaProcess);
