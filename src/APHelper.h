#ifndef AP_HELPER
#define AP_HELPER

#ifdef WIN32
#include <windows.h>

// Console attributes.
HANDLE hConsole;
WORD saved_attributes;
CONSOLE_SCREEN_BUFFER_INFO consoleInfo;

void initConsole() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
    WORD saved_attributes;

    /* Save current attributes */
    GetConsoleScreenBufferInfo(hConsole, &consoleInfo);
    saved_attributes = consoleInfo.wAttributes;
}

void deinitConsole() {
    /* Restore original attributes */
    SetConsoleTextAttribute(hConsole, saved_attributes);
}

// Nothing to be done here.
void resetColor() { deinitConsole(); }

#else
#include <stdio.h>

// Nothing to do here
void initConsole() {}

void deinitConsole() {
    printf("\e[0m");
}

void resetColor() { deinitConsole(); }

#endif

#endif
