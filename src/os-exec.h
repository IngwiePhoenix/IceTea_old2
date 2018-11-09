#ifndef OS_EXEC_H
#define OS_EXEC_H

#include <iostream>
#include <vector>
#include <sstream>

#include "subprocesses.hpp"
#include "objectscript.h"
#include "os-icetea.h"

#ifdef stdout
#   undef stdout
#endif
#ifdef stderr
#   undef stderr
#endif

class SyncProcess: public stlplus::subprocess {
private:
    std::string stdout;
    std::string stderr;
public:
    std::string getStdout();
    std::string getStderr();
    bool callback();
};

class AsyncProcess: public stlplus::async_subprocess {
private:
    std::string stdout;
    std::string stderr;
public:
    std::string getStdout();
    std::string getStderr();
    bool callback();
};

#endif
