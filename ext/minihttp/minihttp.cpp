#include <string>
#include <iostream>

#include "InternalIceTeaPlugin.h"
#include "os-icetea.h" // OS_FUNC
#include "IceTea.h"
#include "minihttp.h"

using namespace std;
using namespace ObjectScript;

class IceTeaHTTP: public IceTeaPlugin {
    static OS_FUNC(Download) {
        if(params <= 0) {
            os->setException("http::download: Expected parameter 1 to be string.");
            return 0;
        }

        const char* url = os->toString(-params+0).toChar();
        char* content = minihttp::Download(url);
        os->pushString(content);
        return 1;
    }
    static OS_FUNC(URLEncode) {
        if(params <= 0) {
            os->setException("http::URLEncode: Expected parameter 1 to be string.");
            return 0;
        }
        string out;
        string in = os->toString(-params+0).toChar();
        minihttp::URLEncode(in, out);
        os->pushString(out.c_str());
        return 1;
    }

    bool configure(IceTea* os) {
        os->getModule("minihttp");
        int minihttpOffs = os->getAbsoluteOffs(-1);

        OS::FuncDef globalFuncs[] = {
            {OS_TEXT("Download"), Download},
            {OS_TEXT("URLEncode"), URLEncode},
            {}
        };
        os->setFuncs(globalFuncs);

        /*
        OS::NumberDef SSLResult_Nums[] = {
            {OS_TEXT("SSLR_OK"), minihttp::SSLResult::SSLR_OK},
            {OS_TEXT("SSLR_NO_SSL"), minihttp::SSLResult::SSLR_NO_SSL},
            {OS_TEXT("SSLR_FAIL"), minihttp::SSLResult::SSLR_FAIL},
            {OS_TEXT("SSLR_CERT_EXPIRED"), minihttp::SSLResult::SSLR_CERT_EXPIRED},
            {OS_TEXT("SSLR_CERT_REVOKED"), minihttp::SSLResult::SSLR_CERT_REVOKED},
            {OS_TEXT("SSLR_CERT_CN_MISMATCH"), minihttp::SSLResult::SSLR_CERT_CN_MISMATCH},
            {OS_TEXT("SSLR_CERT_NOT_TRUSTED"), minihttp::SSLResult::SSLR_CERT_NOT_TRUSTED},
            {OS_TEXT("SSLR_CERT_MISSING"), minihttp::SSLResult::SSLR_CERT_MISSING},
            {OS_TEXT("SSLR_CERT_SKIP_VERIFY"), minihttp::SSLResult::SSLR_CERT_SKIP_VERIFY},
            {OS_TEXT("SSLR_CERT_FUTURE"), minihttp::SSLResult::SSLR_CERT_FUTURE},
            {OS_TEXT("_SSLR_FORCE32BIT"), minihttp::SSLResult::_SSLR_FORCE32BIT},
            {}
        };

        OS::NumberDef HttpCode_Nums[] = {
            {OS_TEXT("HTTP_OK"), minihttp::HttpCode::HTTP_OK},
            {OS_TEXT("HTTP_NOTFOUND"), minihttp::HttpCode::HTTP_NOTFOUND},
            {}
        };

        os->newObject();
            os->setNumbers(SSLResult_Nums);
        os->setProperty(minihttpOffs, "SSLResult");

        os->newObject();
            os->setNumbers(HttpCode_Nums);
        os->setProperty(minihttpOffs, "HttpCode");
        */

        return true;
    }
    string getName() {
        return "miniHTTP";
    }
    string getDescription() {
        return  "Provides HTTP(s) support through mbedTLS and miniHTTP.";
    }
};
ICETEA_INTERNAL_MODULE(IceTeaHTTP);
