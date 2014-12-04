#ifndef FILECACHE_HPP
#define FILECACHE_HPP

#include <map>
#include <string>
#include <sstream>
#include <stdlib.h>
#include <stdio.h>

#include "file_system.hpp"

#define KV_SEP '='

template<typename key, typename val> class Filecache {
private:
    std::string outputfile;
    bool is_open;
public:
    // The store
    std::map<key, val> store;

    Filecache(const std::string filename) : outputfile(filename) {
        if(stlplus::file_exists(filename)) {
            // Read existing cache
            FILE* fh = fopen(filename.c_str(), "rb");
            if(!fh) {
                fputs("Filecache: Error: Can not seem to open file!\n", stderr);
                is_open=false;
                return;
            } else {
                is_open=true;
            }
            fseek(fh, 0, SEEK_END);
            long fhSize = ftell(fh);
            rewind(fh);
            // Get the whole thing in at once.
            char* buffer = (char*)malloc(sizeof(char)*fhSize);
            if(buffer == NULL) {
                fputs("Filecache: Memory error\n", stderr);
                is_open=false;
                return;
            }
            size_t result = fread(buffer, 1, fhSize, fh);
            if(result != fhSize) {
                fputs("Filecache: The read file does not match with the estimated length.\n", stderr);
                is_open=false;
                return;
            }

            // Now process the cache file.
            std::stringstream _key;
            std::stringstream _val;
            bool writeVal=false;
            for(long i=0; i<fhSize; i++) {
                if(buffer[i] != KV_SEP && buffer[i] != '\n') {
                    if(!writeVal)
                        _key << buffer[i];
                    else
                        _val << buffer[i];
                } else if(buffer[i] == KV_SEP) {
                    if(!writeVal) {
                        // We currently are writing the key, switch.
                        writeVal=true;
                    }
                } else if(buffer[i] == '\n') {
                    store[_key.str()]=_val.str();
                    _key.clear();
                    _val.clear();
                    writeVal=false;
                }
            }
        }
    }
    ~Filecache() { write(); }

    inline bool isOpen() { return is_open; }
    inline void setOutput(const std::string& fn) { outputfile=fn; }

    // Always overwrites the cache...for now.
    inline bool write() {
        FILE* fh = fopen(outputfile.c_str(), "wb");
        if(fh==NULL) {
            fputs("Error while opening for write.\n", stderr);
            return false;
        }
        typename std::map<key, val>::iterator it=store.begin();
        for(; it!=store.end(); ++it) {
            std::stringstream data;
            data << it->first;
            data << KV_SEP;
            data << it->second;
            data << '\n';
            std::string out = data.str();
            fwrite(out.c_str(), sizeof(char), sizeof(out.c_str()), fh);
            data.clear();
        }
        fclose(fh);
        return true;
    }

    inline bool unlink() {
        if(stlplus::file_exists(outputfile)) {
            return stlplus::file_delete(outputfile);
        }
        return false;
    }
};

#endif
