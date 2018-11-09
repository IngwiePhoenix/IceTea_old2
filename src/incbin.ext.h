#ifndef INCBIN_ICETEA_EXT_H
#define INCBIN_ICETEA_EXT_H

#ifdef ICETEA_INCBIN_FORCE_EXTERNAL
#undef INCBIN
#define INCBIN(sym, file) INCBIN_EXTERN(sym)
#endif

#endif
