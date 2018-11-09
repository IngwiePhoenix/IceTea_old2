#include <string>
#include <sstream>

#include "IceTea.h"
#include "os-icetea.h"
#include "InternalIceTeaPlugin.h"

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

class IceTeaDetector: public IceTeaPlugin {
public:
    bool configure(IceTea* os) {
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
    string getName() {
        return "Detector";
    }
    string getDescription() {
        return  "Provides means of testing and poking the current maschine.\n"
                "Most of this plugin is actually written in ObjectScript itself. See the lib/ folder in the source.\n"
                "Some notable features:\n"
                "- Check for headers\n"
                "- Check for libraries\n"
                "- Check for functions in libraries or headers\n"
                "- Find and execute tools such as pkg-config or alike";
    }
};
ICETEA_INTERNAL_MODULE(IceTeaDetector);
