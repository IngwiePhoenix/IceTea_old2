print detect.head .. "Running several tests using the detector module..."

detect.findCompiler(detect.name2kind("c"));
detect.findCompiler(detect.name2kind("c++"));
detect.findCompiler(detect.name2kind("objc"));
detect.findCompiler(detect.name2kind("objc++"));
//detect.findCompiler(detect.name2kind("java"));
detect.findCompiler(detect.name2kind("swift"));

detect.header("c", "stdio.h");
detect.header("c", "conio.h");
detect.header("c", "ncurses.h");
detect.header("c++", "iostream");
detect.header("objc", "objc/objc.h");

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
detect.headerfunc("stdio.h", "printf");
detect.headerfunc("objc/objc.h","objc_msgSend");

// Functions in both


// Checking for data types
detect.type("uint16_t");
detect.typeHeader("uint16_t", "stddef.h");
detect.typeHeader("uint16_t", "inttypes.h");

// Compiler flags


// Running tests


// Direct cache access


// Generate
detect.writeHeader("config.h");
print detect.footer
