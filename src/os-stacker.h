#ifndef OS_STACKER_H
#define OS_STACKER_H

#include <iostream>

#define OS_STACKDUMP(os) \
    std::cout << "--- STACK DUMP ---" << std::endl; \
    for(int _si=-1; _si>=-5; --_si) std::cout << "    " << _si << ": " \
                                           << os->getTypeStr(_si) \
                                           << " => " \
                                           << os->toString(_si) \
                                           << std::endl; \
    std::cout << "--- END STACK DUMP ---" << std::endl

#endif
