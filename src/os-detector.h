#ifndef OS_DETECTOR_H
#define OS_DETECTOR_H

#include "os-icetea.h"
#include "objectscript.h"
#include "filecache.hpp"
#include "cli.h"

bool initializeDetector(ObjectScript::OS*, Filecache*, CLI*);

#endif
