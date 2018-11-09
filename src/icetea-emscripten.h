#ifndef ICETEA_EMSCRIPTEN_H
#define ICETEA_EMSCRIPTEN_H

#ifdef EMSCRIPTEN
// Include it.
#include <emscripten.h>

// stdout, stderr fix
#ifdef stdout
#   undef stdout
#endif
#ifdef stderr
#   undef stderr
#endif

#endif

#endif
