#include <string>
#include <sstream>

#include "IceTea.h"
#include "os-icetea.h"
#include "Plugin.h"

using namespace std;
using namespace ObjectScript;

#define HEADER(strm) strm << "\u250C\u2500\u2500\u2500\u2500 "
#define OUTPUT(strm) strm << "\u2502 "
#define FOOTER(strm) strm << "\u2514\u2500\u2500\u2500\u2500 "

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

bool initializeDetector(IceTea* os) {
    OS::FuncDef dtFuncs[] = {
        // Display and output
        {OS_TEXT("__get@head"), osd_head},
        {OS_TEXT("__get@out"), osd_out},
        {OS_TEXT("__get@footer"), osd_footer},
        {}
    };
    os->getModule("detect");
    os->setFuncs(dtFuncs);
    os->pop();

    return true;
}
ICETEA_MODULE(detector, initializeDetector);
