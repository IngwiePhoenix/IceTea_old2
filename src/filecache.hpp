#ifndef FILECACHE_H
#define FILECACHE_H

#include <stdlib.h>
#include <iostream>
#include <map>

#include "SimpleIni.h"
#include "tinythread.h"
#include "file_system.hpp"
#include "util.h"

// Simplified functions due to error handling
inline bool _fc_fopen(FILE*& fh, const char* file, const char* mode) {
    fh = fopen(file, mode);
    if(!fh || ferror(fh) != 0) {
        #ifdef DEBUG
        std::cerr << "Filecache: could not open file: " << file << std::flush;
        std::cerr << std::endl;
        #endif
        return false;
    }
    return true;
}
inline long _fc_fsize(FILE*& fh) {
    fseek(fh, 0L, SEEK_END);
    long sz = ftell(fh);
    fseek(fh, 0L, SEEK_SET);
    return sz;
}

class Filecache {
    typedef tthread::lock_guard<tthread::mutex> _guard;
private:
    std::string filename;
    tthread::mutex m;
    CSimpleIniA ini;

    void run() {
        reread();
    }
public:
    // Predefs...
    inline bool sync();
    inline bool reread();
    inline std::map<std::string,std::string> getMap(const std::string& obj) {
        using namespace std;
        _guard g(m);
        map<std::string,std::string> res;
        CSimpleIniA::TNamesDepend keys;
        ini.GetAllKeys(obj.c_str(), keys);
        CSimpleIniA::TNamesDepend::const_iterator i;
        for(i = keys.begin(); i != keys.end(); ++i) {
            const CSimpleIniA::SI_CHAR_T* _key = i->pItem;
            const CSimpleIniA::SI_CHAR_T* _val = ini.GetValue(obj.c_str(), _key, NULL);
            res[std::string(_key)] = std::string(_val);
        }
        return res;
    }

    Filecache(const char* fname)      : filename(fname) { run(); }
    Filecache(const std::string fname): filename(fname) { run(); }
    ~Filecache() { sync(); }

    inline void set(const std::string key, const std::string val, std::string obj="root") {
        using namespace std;
        _guard g(m);
        ini.SetValue(obj.c_str(), key.c_str(), val.c_str());
        #ifdef DEBUG
        v = o;
        std::cerr << "Filecache: Current JSON> " << v.str() << std::endl;
        #endif
    }

    inline std::string get(const std::string key, const std::string obj="root") {
        using namespace std;
        _guard g(m);
        #ifdef DEBUG
        std::cerr << "Filecache: " << key << "..." << std::endl;
        #endif
        const char* out = ini.GetValue(obj.c_str(), key.c_str(), "");
        return string(out);
    }

    inline bool remove(const std::string key, std::string obj="root") {
        _guard g(m);
        ini.Delete(obj.c_str(), key.c_str());
        return false;
    }

    inline void file(const char* fn) {
        filename = std::string(fn);
    }
    inline void file(const std::string fn) {
        filename = fn;
    }
    inline std::string file() {
        return filename;
    }
};

inline bool Filecache::sync() {
    _guard g(m);
    ini.SaveFile(filename.c_str());
    return true;
}

inline bool Filecache::reread() {
    if(stlplus::file_exists(filename)) {
        ini.LoadFile(filename.c_str());
        return true;
    } else {
        // File doesnt exist.
        return false;
    }
}

#endif
