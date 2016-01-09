#ifndef OS_DETECTOR_H
#define OS_DETECTOR_H

#include <iostream>
#include <stdlib.h>
#include <algorithm>
#include <string>
#include <set>

#include "os-icetea.h"
#include "os-exec.h"
#include "IceTea.h"
#include "filecache.hpp"
#include "cli.h"

bool initializeDetector(IceTea*);
void initCompilers();
std::string toUpper(const std::string str);
std::string have_header(const std::string name);
std::string have_lib(const std::string lib);
std::string have_func(const std::string func);
std::string find_tool(std::vector<std::string> cmds);
std::string find_compiler(const std::string kind);
bool write_file(const std::string& content, const std::string& filename);
std::pair<bool,CommandResult> run_task(const std::string source, const std::string kind, std::string add, bool run);


#endif
