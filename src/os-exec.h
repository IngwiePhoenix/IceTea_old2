#ifndef OS_EXEC_H
#define OS_EXEC_H

#include <iostream>
#include <vector>
#include <sstream>

#include "subprocesses.hpp"
#include "objectscript.h"
#include "os-icetea.h"

// This is a minimal interface...
struct IceTeaProcess: public stlplus::subprocess {
    std::string stdout;
    std::string stderr;
    bool callback();
};

struct CommandResult {
    int            exit_code;
    std::string    streams[3]; // 0, 1, 2
    IceTeaProcess* p;
};

// This is the actual command function.
CommandResult it_cmd(const std::string& cmd, std::vector<std::string> replaces);

OS_FUNC(os_exec);

#endif