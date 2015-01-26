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
    LinePrinter() {
        stream = new std::stringstream();
    }
    std::stringstream* operator()() {
        tthread::lock_guard<tthread::mutex> guard(m);
        return stream;
    }
    void print() {
        tthread::lock_guard<tthread::mutex> guard(m);
        string out(stream->str());
        cout << out << flush;
        //stream->clear();
        stream->str("");
    }
};
tthread::mutex LinePrinter::m;

#endif
