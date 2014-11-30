#ifndef OS_ICETEA_H
#define OS_ICETEA_H

#include "cli.h"

#define OS_FUNC(name) int name(ObjectScript::OS* os, int params, int closure_values, int need_ret_values, void* userData)

void initIceTeaExt(ObjectScript::OS*, CLI*);

#endif
