#ifndef OS_EXEC_H
#define OS_EXEC_H

#include <iostream>
#include <vector>
#include <sstream>

#include "exec-stream.h"
#include "objectscript.h"
#include "os-icetea.h"

struct CommandResult {
    int         exit_code;
    std::string streams[3];
};

// This is the actual command function.
CommandResult it_cmd(const std::string& cmd, std::vector<std::string>& replaces);

OS_FUNC(os_exec);

#endif
