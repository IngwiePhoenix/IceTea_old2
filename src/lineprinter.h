#ifndef LINEPRINTER_H
#define LINEPRINTER_H

#include <iostream>
#include <sstream>
#include "tinythread.h"

class LinePrinter {
private:
    static tthread::mutex m;
    std::stringstream* stream;
public:
    LinePrinter(std::stringstream* str) : stream(str) {}
    void operator()() {
        tthread::lock_guard<tthread::mutex> guard(m);
        cout << stream->str();
        stream->clear();
        stream->str("");
    }
};
tthread::mutex LinePrinter::m;

#endif
