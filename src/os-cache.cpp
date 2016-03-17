#include <string>
#include <iostream>

#include "IceTea.h"
#include "os-icetea.h"
#include "filecache.hpp"
#include "SimpleIni.h"
#include "InternalIceTeaPlugin.h"

using namespace ObjectScript;
using namespace std;

// In OS, the "this" object is at -params-1!

struct OSFileCache {
    static OS_FUNC(__construct) {
        int _this = os->getAbsoluteOffs(-params-1);

        if(params == 1) {
            // __construct(String cachename)
            string name = os->toString(-params+0).toChar();
            string msg = "Cache::__construct: ";
            msg.append(name);
            ((IceTea*)os)->printDebug(msg);
            os->setProperty(_this, "cacheName", false);
        } else {
            os->setException("Cache: Expected parameter 1 to be string.");
        }
        return 0;
    }

    static OS_FUNC(__set) {
        if(!os->isString(-params+0)) {
            os->setException("Cache.__set: Expected string.");
            return 0;
        }
        IceTea* it = (IceTea*)os;
        Filecache* fc = it->getFilecache();
        int _this = os->getAbsoluteOffs(-params-1);
        string key = os->toString(-params+0).toChar();
        string val = os->toString(-params+1).toChar();
        // this.cacheName
        os->getProperty(_this, "cacheName", false);
        string obj = os->toString().toChar();
        string msg = "Cache::__set(";
        msg.append(obj); msg.append("): "); // <- my face when things dont work.
        msg.append(key); msg.append(" = "); msg.append(val);
        it->printDebug(msg);
        fc->set(key, val, obj);
        fc->sync();
        return 0;
    }

    static OS_FUNC(__get) {
        if(!os->isString(-params+0)) {
            os->setException("Cache.__get: Expected string.");
            return 0;
        }
        IceTea* it = (IceTea*)os;
        Filecache* fc = it->getFilecache();
        int _this = os->getAbsoluteOffs(-params-1);
        string key = os->toString(-params+0).toChar();
        // this.cacheName
        os->getProperty(_this, "cacheName", false);
        string obj = os->toString().toChar();
        string msg = "Cache::__get(";
        msg.append(obj); msg.append("): Getting key: ");
        msg.append(key);
        ((IceTea*)os)->printDebug(msg);

        string val = fc->get(key, obj);
        os->pushString(val.c_str());
        return 1;
    }

    static OS_FUNC(__del) {
        if(!os->isString(-params+0)) {
            os->setException("Cache.__del: Expected string.");
            return 0;
        }
        IceTea* it = (IceTea*)os;
        Filecache* fc = it->getFilecache();
        int _this = os->getAbsoluteOffs(-params-1);
        string key = os->toString(-params+0).toChar();
        // this.cacheName
        os->getProperty(_this, "cacheName", false);
        string obj = os->toString().toChar();

        fc->remove(key, obj);
        return 0;
    }

    static OS_FUNC(__isset) {
        if(!os->isString(-params+0)) {
            os->setException("Cache.__isset: Expected string.");
            return 0;
        }
        IceTea* it = (IceTea*)os;
        Filecache* fc = it->getFilecache();
        int _this = os->getAbsoluteOffs(-params-1);
        string key = os->toString(-params+0).toChar();
        // this.cacheName
        os->getProperty(_this, "cacheName", false);
        string obj = os->toString().toChar();

        os->pushBool(!fc->get(key, obj).empty());
        return 1;
    }

    static OS_FUNC(getKeys) {
        IceTea* it = (IceTea*)os;
        Filecache* fc = it->getFilecache();
        map<string,string> names;
        // this.cacheName
        os->getProperty(-params-1, "cacheName", false);
        const char* obj = os->toString().toChar();

        os->newArray();
        names = fc->getMap(obj);
        if(!names.empty()) {
            map<string,string>::iterator iter;
            for(iter=names.begin(); iter!=names.end(); ++iter) {
                os->pushString(iter->first.c_str());
                os->addProperty(-2);
            }
        }
        return 1;
    }

    static OS_FUNC(sync) {
        ((IceTea*)os)->getFilecache()->sync();
        return 0;
    }
};

class IceTeaCache: public IceTeaPlugin {
public:
    bool configure(IceTea* it) {
        OS::FuncDef cacheFuncs[] = {
            {OS_TEXT("__construct"),    OSFileCache::__construct},
            {OS_TEXT("__get"),          OSFileCache::__get},
            {OS_TEXT("__set"),          OSFileCache::__set},
            {OS_TEXT("__del"),          OSFileCache::__del},
            {OS_TEXT("__isset"),        OSFileCache::__isset},
            {OS_TEXT("__get@keys"),     OSFileCache::getKeys},
            {OS_TEXT("getKeys"),        OSFileCache::getKeys},
            {OS_TEXT("sync"),           OSFileCache::sync},
            {}
        };

        it->getGlobalObject("Cache");
        it->setFuncs(cacheFuncs, false);
        it->pop();

        return true;
    }
    string getName() {
        return "Cache";
    }
    string getDescription() {
        return  "This plugin allows IceTea to cache data on disk.\n"
                "It uses a file within the output directory to do so.";
    }
};

ICETEA_INTERNAL_MODULE(IceTeaCache);
