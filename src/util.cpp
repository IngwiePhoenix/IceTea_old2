#include <string>
#include <stdlib.h>
#include <stdio.h>
#include "util.h"
#include "picosha2.h"

using namespace std;

std::string ReplaceString(std::string subject, const std::string& search,
                          const std::string& replace) {
    size_t pos = 0;
    while((pos = subject.find(search, pos)) != std::string::npos) {
         subject.replace(pos, search.length(), replace);
         pos += replace.length();
    }
    return subject;
}

void ReplaceStringInPlace(std::string& subject, const std::string& search,
                          const std::string& replace) {
    size_t pos = 0;
    while((pos = subject.find(search, pos)) != std::string::npos) {
         subject.replace(pos, search.length(), replace);
         pos += replace.length();
    }
}

string file2sha2(const string filename) {
    FILE *f = fopen(filename.c_str(), "rb");
    if(!f) return string();
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *cont = (char*)malloc(fsize + 1);
    fread(cont, fsize, 1, f);
    fclose(f);
    cont[fsize] = 0;
    string fcont = cont;
    std::string hash_hex_str;
    picosha2::hash256_hex_string(fcont, hash_hex_str);
    return hash_hex_str;
}
