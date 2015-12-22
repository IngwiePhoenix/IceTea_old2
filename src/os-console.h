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
