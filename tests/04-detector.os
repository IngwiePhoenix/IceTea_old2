print detect.head .. "Running several tests using the detector module..."

detect.compiler("c");
detect.compiler("c++");
detect.compiler("objc");
detect.compiler("objc++");
detect.compiler("java");
detect.compiler("swift");

detect.header("c", "stdio.h");
detect.header("c", "conio.h");
detect.header("c", "ncurses.h");
detect.header("c++", "iostream");
detect.header("objc", "objc/objc.h");
// objc++, java and swift not supported yet.

// Libs...
detect.lib("curl");
detect.lib("pthread");
detect.lib("pthreads");
detect.lib("socket");

// Functions: Simple
detect.func("c","exit(0)");
//detect.func("c", "bar(\"foobarbaz\")");
detect.func("c++", "malloc(sizeof(char))");
detect.func("objc", "objc_init((void*))");

// Functions in libs
detect.libfunc("libpthread", "pthread_create");
detect.libfunc("pthread", "pthread_create");

// Functions in headers
detect.headerfunc("c", "stdio.h", "printf");
detect.headerfunc("c", "objc/objc.h","objc_msgSend");

// Functions in both


// Compiler flags


// Running tests


// Direct cache access


// Generate

print detect.footer
