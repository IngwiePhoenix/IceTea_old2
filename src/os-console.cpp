#include "objectscript.h"
#include "os-console.h"
#include "rlutil.h"

using namespace ObjectScript;
using namespace rlutil;

// Set the text color.
OS_FUNC(console_setColor) {
    setColor(os->toInt(-params+0));
    return 0;
}

// Wait for any key to be pressed.
OS_FUNC(console_anyKey) {
    if(os->isString(-params+0)) {
        anykey(os->toString(-params+0).toChar());
    } else {
        anykey();
    }
    return 0;
}

// Move the cursor to that very position
OS_FUNC(console_locate) {
    if(os->isNumber(-params+0) && os->isNumber(-params+1)) {
        int x = os->toInt(-params+0);
        int y = os->toInt(-params+1);
        locate(x, y);
    } else {
        os->setException("$.locate(x,y): Argument 1 and 2 must be numbers.");
    }
    return 0;
}

// Get a character
OS_FUNC(console_getch) {
    os->pushNumber(getch());
    return 1;
}

// Get a key
OS_FUNC(console_getKey) {
    os->pushNumber(getkey());
    return 1;
}

// Save the default color.
// Can only be called once.
OS_FUNC(console_saveDefaultColor) {
    saveDefaultColor();
    return 0;
}

// Reset the color, pretty much like a deinit function.
OS_FUNC(console_resetColor) {
    resetColor();
    return 0;
}

// Clear the screen.
OS_FUNC(console_cls) {
    cls();
    return 0;
}

// Hide the cursor
OS_FUNC(console_cursor_hide) {
    hidecursor();
    return 0;
}

// Unhide the cursor
OS_FUNC(console_cursor_show) {
    showcursor();
    return 0;
}

// Sleep the app for N milliseconds.
OS_FUNC(console_msleep) {
    msleep(os->toInt(-params+0));
    return 0;
}

// Get the rows in the current window.
OS_FUNC(console_getRows) {
    os->pushNumber(trows());
    return 1;
}

// Get the cols
OS_FUNC(console_getCols) {
    os->pushNumber(tcols());
    return 1;
}

// Now comes the fun part!
void extendDollar(OS* os) {
    OS::FuncDef consoleFuncs[] = {
        // Keyboard
        {OS_TEXT("anyKey"),                 console_anyKey},
        {OS_TEXT("getch"),                  console_getch},
        {OS_TEXT("getKey"),                 console_getKey},
        {OS_TEXT("__get@key"),              console_getKey},

        // Color
        {OS_TEXT("setColor"),               console_setColor},
        {OS_TEXT("saveDefaultColor"),       console_saveDefaultColor},
        {OS_TEXT("resetColor"),             console_resetColor},

        // Screen
        {OS_TEXT("cls"),                    console_cls},

        // Cursor
        {OS_TEXT("locate"),                 console_locate},

        // Rows and cols, getters
        {OS_TEXT("__get@rows"),             console_getRows},
        {OS_TEXT("getRows"),                console_getRows},
        {OS_TEXT("__get@cols"),             console_getCols},
        {OS_TEXT("getCols"),                console_getCols},
        {}
    };
    // Cursor
    OS::FuncDef cursorFuncs[] = {
        {OS_TEXT("show"),   console_cursor_show},
        {OS_TEXT("hide"),   console_cursor_hide},
        {}
    };

    // Keyboard keys
    OS::NumberDef keyboardKeys[] = {
        {OS_TEXT("ESCAPE"), KEY_ESCAPE},
        {OS_TEXT("ENTER"), KEY_ENTER},
        {OS_TEXT("SPACE"), KEY_SPACE},

        {OS_TEXT("INSERT"), KEY_INSERT},
        {OS_TEXT("HOME"), KEY_HOME},
        {OS_TEXT("PGUP"), KEY_PGUP},
        {OS_TEXT("DELETE"), KEY_DELETE},
        {OS_TEXT("END"), KEY_END},
        {OS_TEXT("PGDOWN"), KEY_PGDOWN},

        {OS_TEXT("UP"), KEY_UP},
        {OS_TEXT("DOWN"), KEY_DOWN},
        {OS_TEXT("LEFT"), KEY_LEFT},
        {OS_TEXT("RIGHT"), KEY_RIGHT},

        {OS_TEXT("F1"), KEY_F1},
        {OS_TEXT("F2"), KEY_F2},
        {OS_TEXT("F3"), KEY_F3},
        {OS_TEXT("F4"), KEY_F4},
        {OS_TEXT("F5"), KEY_F5},
        {OS_TEXT("F6"), KEY_F6},
        {OS_TEXT("F7"), KEY_F7},
        {OS_TEXT("F8"), KEY_F8},
        {OS_TEXT("F9"), KEY_F9},
        {OS_TEXT("F10"), KEY_F10},
        {OS_TEXT("F11"), KEY_F11},
        {OS_TEXT("F12"), KEY_F12},

        {OS_TEXT("NUMDEL"), KEY_NUMDEL},
        {OS_TEXT("NUMPAD0"), KEY_NUMPAD0},
        {OS_TEXT("NUMPAD1"), KEY_NUMPAD1},
        {OS_TEXT("NUMPAD2"), KEY_NUMPAD2},
        {OS_TEXT("NUMPAD3"), KEY_NUMPAD3},
        {OS_TEXT("NUMPAD4"), KEY_NUMPAD4},
        {OS_TEXT("NUMPAD5"), KEY_NUMPAD5},
        {OS_TEXT("NUMPAD6"), KEY_NUMPAD6},
        {OS_TEXT("NUMPAD7"), KEY_NUMPAD7},
        {OS_TEXT("NUMPAD8"), KEY_NUMPAD8},
        {OS_TEXT("NUMPAD9"), KEY_NUMPAD9},
        {}
    };

    // Color codes
    OS::NumberDef colorCodes[] = {
        {OS_TEXT("BLACK"),          BLACK},
        {OS_TEXT("BLUE"),           BLUE},
        {OS_TEXT("GREEN"),          GREEN},
        {OS_TEXT("CYAN"),           CYAN},
        {OS_TEXT("RED"),            RED},
        {OS_TEXT("MAGENTA"),        MAGENTA},
        {OS_TEXT("BROWN"),          BROWN},
        {OS_TEXT("GREY"),           GREY},
        {OS_TEXT("DARKGREY"),       DARKGREY},
        {OS_TEXT("LIGHTBLUE"),      LIGHTBLUE},
        {OS_TEXT("LIGHTGREEN"),     LIGHTGREEN},
        {OS_TEXT("LIGHTCYAN"),      LIGHTCYAN},
        {OS_TEXT("LIGHTRED"),       LIGHTRED},
        {OS_TEXT("LIGHTMAGENTA"),   LIGHTMAGENTA},
        {OS_TEXT("YELLOW"),         YELLOW},
        {OS_TEXT("WHITE"),          WHITE},
        {}
    };

    os->getGlobal("$");
    int dollar = os->getAbsoluteOffs(-1);

    // Console funcs
    os->setFuncs(consoleFuncs);

    // Cursor funcs
    os->newObject();
    os->setFuncs(cursorFuncs);
    os->setProperty(dollar, "Cursor");

    // Keyboard props
    os->newObject();
    os->setNumbers(keyboardKeys);
    os->setProperty(dollar, "Keyboard");

    // Color codes
    os->newObject();
    os->setNumbers(colorCodes);
    os->setProperty(dollar, "Colors");

    os->pop();
}
