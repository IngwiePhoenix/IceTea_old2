#ifndef FILECACHE_H
#define FILECACHE_H

#include <stdlib.h>
#include <iostream>
#include <map>

#include "minijson.h"
#include "tinythread.h"
#include "file_system.hpp"

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
    minijson::value v;
    minijson::object o;
    tthread::mutex m;

    void run() {
        reread();
    }
public:
    // Predefs...
    inline bool sync();
    inline bool reread();
    inline std::map<std::string,std::string> getMap(const std::string obj) {
        using namespace std;
        using namespace minijson;
        _guard g(m);
        map<std::string,std::string> res;
        object ov = o[obj].get<object>();
        for(object::iterator it=ov.begin(); it!=ov.end(); ++it) {
            res[it->first] = it->second.get<minijson::string>();
        }
        return res;
    }

    Filecache(const char* fname)      : filename(fname) { run(); }
    Filecache(const std::string fname): filename(fname) { run(); }
    ~Filecache() { sync(); }

    inline void set(const std::string key, const std::string val, std::string obj="") {
        using namespace minijson;
        _guard g(m);
        if(obj=="") {
            o[key]=val;
        } else {
            object o2;
            if(o.find(obj) != o.end()) {
                // The specified object does exist.
                o2 = o[obj].get<object>();
            }
            o2[key]=val;
            o[obj]=value(o2);
        }
        #ifdef DEBUG
        v = o;
        std::cerr << "Filecache: Current JSON> " << v.str() << std::endl;
        #endif
    }

    inline std::string get(const std::string key, const std::string obj="") {
        using namespace minijson;
        _guard g(m);
        #ifdef DEBUG
        std::cerr << "Filecache: " << key << "..." << std::endl;
        #endif
        if(obj == "") {
            if(o.find(key) == o.end())
                return std::string("");
            else
                return o[key].get<minijson::string>();
        } else {
            if(o.find(obj) != o.end()) {
                #ifdef DEBUG
                std::cerr << "Filecache: Found " << obj << std::endl;
                #endif
                object o2 = o[obj].get<object>();
                if(o2.find(key) == o2.end()) {
                    #ifdef DEBUG
                    std::cerr << "Filecache: " << key << " was not found." << std::endl;
                    #endif
                    return std::string("");
                } else {
                    #ifdef DEBUG
                    std::cerr << "Filecache: " << key << " was found." << std::endl;
                    #endif
                    return o2[key].get<minijson::string>();
                }
            } else {
                #ifdef DEBUG
                std::cerr << "Filecache: " << obj << " not found." << std::endl;
                #endif
                return std::string("");
            }
        }
        return std::string("");
    }

    inline bool remove(const std::string key, std::string obj="") {
        using namespace minijson;
        _guard g(m);
        if(obj == "") {
            if(o.find(key) != o.end()) {
                object::iterator it = o.find(key);
                o.erase(it);
                return true;
            } else {
                return false;
            }
        } else {
            if(o.find(obj) != o.end()) {
                object o2 = o[obj].get<object>();
                if(o2.find(key) != o2.end()) {
                    object::iterator it = o2.find(key);
                    o2.erase(it);
                    return true;
                } else {
                    return false;
                }
            }
        }
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
    FILE* fh;
    _fc_fopen(fh, filename.c_str(), "wb");
    v = o;
    std::string vstr = v.str();
    size_t size = vstr.size();
    #ifdef DEBUG
    std::cerr << "Going to write " << size << " bytes with content: " << vstr << std::endl;
    #endif
    size_t res = fwrite(vstr.c_str(), 1, size, fh);
    fclose(fh);
    return (res==0?true:false);
}

inline bool Filecache::reread() {
    if(stlplus::file_exists(filename)) {
        FILE* fh;
        _fc_fopen(fh, filename.c_str(), "rb");
        size_t size = stlplus::file_size(filename);
        char* buf = (char*)malloc(sizeof(char)*size);
        size_t res = fread(buf, sizeof(char), size, fh);
        if(res != size || ferror(fh) != 0) {
            #ifdef DEBUG
            std::cerr << "Reading failed: " << std::endl;
            std::cerr << "Read: " << res << " | Size: " << size << std::endl;
            #endif
            fclose(fh);
            return false;
        }
        fclose(fh);
        // Parse it now. Or, try to...
        minijson::error e = parse(buf, this->v);
        if(e == minijson::no_error) {
            // Overwrite it...
            if(v.getType() == minijson::object_type) {
                #ifdef DEBUG
                std::cerr << "Filecache: value is an object." << std::endl;
                #endif
                o = v.get<minijson::object>();
            } else {
                #ifdef DEBUG
                std::cerr << "Filecache: value is NOT object." << std::endl;
                #endif
                v.str("{}");
                o = minijson::object();
            }
            return true;
        } else return false;
    } else {
        // Create a dummy...
        v.str("{}");
        o = minijson::object();
        return false;
    }
}

#endif
