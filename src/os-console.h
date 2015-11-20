#include "os-icetea.h"

// Define the various rlutil func wrappers that
// will be appended to $
/*
    Methods:
    - $.setColor($.Color[name])
    - $.anyKey([message])
    - $.locate(x, y)
    - $.getch()
    - $.getKey() / $.__get@key : $.Keyboard
    - $.saveDefaultColor()
    - $.resetColor()
    - $.cls()
    - $.Cursor.hide()
    - $.Cursor.show()
    - $.msleep(milliseconds)
    - $.__get@rows / $.getRows()
    - $.__get@cols / $.getCols()

    Keyboard numbers:
    - $.Keyboard.UP
    - $.Keyboard.DOWN
    - ...

    Color numbers
    - $.Color.RED
    - $.Color.GREEN
    - ...
*/

// Initializer. Its a bit bigger than the others.
// Plus, almost extern-able!
void extendDollar(ObjectScript::OS*);

// Wrappers.
// Man I wish I could automate this.
OS_FUNC(console_setColor);
OS_FUNC(console_anyKey);
OS_FUNC(console_locate);
OS_FUNC(console_getch);
OS_FUNC(console_getKey);
OS_FUNC(console_saveDefaultColor);
OS_FUNC(console_resetColor);
OS_FUNC(console_cls);
OS_FUNC(console_cursor_hide);
OS_FUNC(console_cursor_show);
OS_FUNC(console_msleep);
OS_FUNC(console_getRows);
OS_FUNC(console_getCols);
