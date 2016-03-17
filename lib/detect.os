/**
 * @file Within here, we extend the native detect class with more functionality,
 * mainly the functionality that would be incredibly hard to code in using the C++ API.
 *
 * Because, let's be honest, the C++ API is nice but hard :).
 */

// Structure for compilers.
CompilerInfo = extends Object {
    __construct: function(
        name,           // A name of the compiler.
        toolName,       // Name of the tool. I.e.: clang
        includeDir,     // The flag to include a folder.
        warning,        // Flag prefix for warnings.
        define,         // Flag to define a preprocessor directive.
        compileFlag,    // Flag to use to translate source into compilation unit.
        outputFlag,     // Flag for the output.
        forceInclude,   // Flag used to FORCE inclusion of a file.
        optimizeFlag,   // Flag prefix used to determine optimization.
        optimizeMap,    // Map of name to other half of optimize flag.
        positionIndep,  // Flag like -fPIC to produce position independent code. Shared libs.
        debugFlag       // Flag to cause debug symbols
    ) {
        @name = name;
        @_toolName = toolName;
        @_includeDir = includeDir;
        @_warning = warning;
        @_defineFlag = define;
        @_compileFlag = compileFlag;
        @_outputFlag = outputFlag;
        @_forceInclude = forceInclude;
        @_optimizeFlag = optimizeFlag;
        @_optimizeMap = optimizeMap;
        @_debugFlag = debugFlag;
    },

    // Easy. :)
    findMe: function() {
        var path = sys.which(@_toolName);
        return path != "";
    },

    /**
     * Return a string that tells the compiler which folders to include.
     * @param {String or array} list List of folders - or jsut one.
     * @return {String} Include string.
     */
    include: function(list) {
        return detect.makeFlags(@_includeDir, list);
    },

    /** @see include */
    warning: function(list) {
        return detect.makeFlags(@_warning, list);
    },

    optimize: function(val) {
        if(__.isNull(val) || __.isBool(val) && !val) {
            return "";
        }
        if(val in @_optimizeMap) {
            return @_optimizeFlag .. @_optimizeMap[val];
        } else {
            return @_optimizeFlag .. val;
        }
    },

    define: function(list) {
        return detect.makeFlags(@_defineFlag, list);
    },

    forceInclude: function(val) {
        if(typeOf(@_forceInclude) != "null") {
            return @_forceInclude .. val;
        } else {
            var msg = "WARNING: ${@name} doens't support forced inclusion.";
            if(cli.check("--no-color")) {
                print msg;
            } else {
                $.setColor($.Color.RED);
                print msg;
                $.restoreDefaultColor();
            }
        }
    },

    buildCommand: function(
        input, output,
        includes, warnings, defines,
        flags, optimize, forceInclude,
        isShared, shouldDebug
    ) {
        if(typeOf(input) != "string") {
            var t = typeOf(input);
            throw "${@name} [compiler] can only compile ONE FILE. (Input is of type: ${t})";
        }
        isShared = !__.isBool(isShared) ? false : isShared;
        shouldDebug = !__.isBool(shouldDebug) ? false : shouldDebug;
        return [
            @_toolName,
            (!__.isNull(includes)? @include(includes) : ""),
            (!__.isNull(warnings)? @warning(warnings) : ""),
            (!__.isNull(defines) ? @define(defines)   : ""),
            (!__.isNull(optimize)? @optimize(optimize): ""),
            (__.isArray(flags)   ? flags.join(" ")    : (__.isString(flags) ? flags : "")),
            (isShared           ? @_positionIndep    : ""),
            (shouldDebug        ? @_debugFlag        : ""),
            @_compileFlag .. input,
            @_outputFlag .. output
        ].join(" ");
    }
}

// Info about linkers.
// They can be used to build executables,
// and shared/dynamic libraries.
LinkerInfo = extends Object {
    __construct: function(
        name,               // Name
        toolName,           // name of the tool
        outputFlag,         // Output flag
        libraryDir,         // Flag to add folder to library search path.
        libraryFlag,        // Flag to tell the compiler to link with given library.
        sharedLibFlag,      // Flag for SHARED LIBRARY. Linux/Windows (.so/.dll)
        dynamicLibFlag,     // Flag for DYNAMIC LIBRARY. OS X
        bundleFlag,         // Flag to instruct the linker to make a bundle (-bundle on OS X, otherwise shared library)
        bundleLoaderFlag    // Flag to tell the linker who will load the bundle (-bundle_loader, OS X only.)
    ) {
        @name = name;
        @_toolName = toolName;
        @_outputFlag = outputFlag;
        @_libraryDir = libraryDir;
        @_libraryFlag = libraryFlag;
        @_sharedLibFlag = sharedLibFlag;
        @_dynamicLibFlag = dynamicLibFlag;
        @_bundleFlag = bundleFlag;
        @_bundleLoaderFlag = bundleLoaderFlag;
    },

    // Easy. :)
    findMe: function() {
        var path = sys.which(@_toolName);
        return path != "";
    },

    // Return a string of library flags.
    library: function(lib) {
        if(!__.isString(lib) && !__.isArray(lib)) {
            return "";
        }
        var lib = __.isArray(lib) ? lib : [lib];
        var out = [];
        for(var _,libName in lib) {
            if(pfs.isFile(libName)) {
                out[] = libName;
            } else {
                out[] = @_libraryFlag .. libName;
            }
        }
        return out.join(" ");
    },

    // Return a string of library searchpath extensions
    libraryPath: function(dir) {
        var lstr = detect.makeFlags(@_libraryDir, dir);
        var findIndex = lstr.find(__outputdir);
        if(typeOf(findIndex) == "null") {
            lstr = lstr .. " ${@_libraryDir}${__outputdir}"
        }
        return lstr;
    },

    // Link an object, library or executable.
    linkCommand: function(
        input, output,
        libraries, libPaths,
        flags, makeShared, isOSX
    ) {
        makeShared = !__.isBool(makeShared) ? false : makeShared;
        isOSX = !__.isBool(isOSX) ? false : isOSX;
        var libFlag = isOSX ? @_dynamicLibFlag : @_sharedLibFlag;
        return [
            @_toolName,
            (!__.isNull(libPaths) ? @libraryPath(libPaths)   : ""),
            (!__.isNull(libraries)? @library(libraries)      : ""),
            (__.isArray(flags)    ? flags.join(" ")          : (__.isString(flags) ? flags : "")),
            (makeShared          ? libFlag                  : ""),
            (__.isArray(input)    ? input.join(" ")          : input),
            @_outputFlag .. output
        ].join(" ");
    },

    linkSharedCommand: function(
        input, output,
        libraries, libpaths,
        flags, isOSX, OSXloader
    ) {
        isOSX = !__.isBool(isOSX) ? false : isOSX;
        var loaderFlag = isOSX ? [
            @_bundleFlag,
            @_bundleLoaderFlag,
            OSXloader
        ].join(" ") : @_sharedLibFlag;
        return [
            @_toolName,
            (!__.isNull(libPaths) ? @include(libPaths) : ""),
            (!__.isNull(libraries)? @include(libraries): ""),
            (__.isArray(flags)    ? flags.join(" ")    : (__.isString(flags) ? flags : "")),
            loaderFlag,
            (__.isArray(input)    ? input.join(" ")    : input),
            @_outputFlag .. output
        ].join(" ");
    }
}

// Info for making static libs.
StaticLibraryInfo = extends Object {
    __construct: function(
        name,       // Name
        toolName,   // Name of the tool
        createFlag,// Flags used to instruct creation
        outputFlag // Flag to tell how to putput things. Only needed on Windows, I think.
    ) {
        @name = name;
        @_toolName = toolName;
        @_createFlag = createFlag;
        @_outputFlag = (__.isString(outputFlag) ? outputFlag : "");
    },
    linkCommand: function(input, output, flags) {
        return [
            @_toolName,
            @_createFlag,
            (__.isArray(flags) ? flags.join(" ") : flags),
            (__.isArray(input) ? input.join(" ") : input),
            @_outputFlag .. output
        ].join(" ");
    }
}

// Ladys and gentlemen, prepare.
// COMPILERS AND LIIIIINKERS!
// kind => [CompilerInfo, ...]

// Optimize flags
var unixOptimizeMap = {
    none: "0",
    low: "1",
    medium: "2",
    high: "3",
    size: "s"
};
var win32OptimizeMap = {
    none: "",
    low: "1",
    medium: "2",
    high: "3",
    size: "s"
};

// General compilers.

// Common compilers. Some of these can do multiple languages,
// so we pre-construct them.
detect.CommonCompilers = {
    GCC: CompilerInfo(
       "GNU C Compiler",
       "gcc", "-I", "-W", "-D", "-c ", "-o", "-include", "-O",
       unixOptimizeMap,
       "-fPIC", "-g"
   ),
   ClangCC: CompilerInfo(
       "C-Language Frontend",
       "clang", "-I", "-W", "-D", "-c ", "-o", "-include", "-O",
       unixOptimizeMap,
       "-fPIC", "-g"
   ),
   GCC_XX: CompilerInfo(
       "GNU C++ Compiler",
       "gcc", "-I", "-W", "-D", "-c ", "-o", "-include", "-O",
       unixOptimizeMap,
       "-fPIC", "-g"
   ),
   ClangCXX: CompilerInfo(
       "C++-Language Frontend",
       "clang++", "-I", "-W", "-D", "-c ", "-o", "-include", "-O",
       unixOptimizeMap,
       "-fPIC", "-g"
   )
}
var _c = detect.CommonCompilers;

// We usually assume UNIX first, win32 is also checked on Win32.
detect.Compilers = {
    // FIXME: Various assemblers...
    ASM: [_c.ClangCC, _c.GCC],
    NASM: [
        // FIXME: NASM is a meta assembler.
    ],
    CC: [_c.ClangCC, _c.GCC],
    CXX: [_c.ClangCXX, _c.GCC_XX],
    OBJC: [_c.ClangCC, _c.GCC],
    OBJCXX: [_c.ClangCXX, _c.GCC_XX],
    SWIFTC: [
        CompilerInfo(
            // FIXME: SwiftCompilerInfo = extends CompilerInfo.
            // FIXME: Warning flags for swift
            "Apple Swift Compiler",
            "swiftc", "-I", "-W", null, "-emit-object", "-o", "-module-link-name", "-O",
            unixOptimizeMap,
            null, "-g"
        )
    ],
    CSC: [
        // FIXME: C# Compilers (Mono)
    ],
    DC: [
        // FIXME: D compilers (dmd)
    ],
    GOC: [
        // FIXME: Go compiler
    ],
    RUSTC: [
        // FIXME: Rust compiler
    ],
    JAVAC: [
        // FIXME: Java compiler
    ]
};

// Windows support
// cl.exe is a C and C++ compiler. So we can just use it twice.
var cl_exe = CompilerInfo(
    // FIXME: -fPIC for cl.exe.
    "Microsoft Visual C compiler",
    "cl.exe", "/I", "/w", "/D", "/c", "/Fo", "/FI", "/O",
    win32OptimizeMap,
    "", "/Zi"
);
detect.Win32Compilers = {
    ASM: [cl_exe],
    CC: [cl_exe],
    CXX: [cl_exe],
    // FIXME: Microsoft does ObjC support. Find out how to use it.
    OBJC: [_c.ClangCC, _c.GCC],
    OBJCXX: [_c.ClangCXX, _c.GCC_XX],
    CSC: [],
};

// Linker: The things that link things.
detect.Linkers = [
    LinkerInfo(
        "C-Language Frontend (as C++ Linker)",
        "clang++", "-o", "-L", "-l",
        "-shared", "-dynamiclib",
        "-bundle", "-bundle_loader"
    ),
    LinkerInfo(
        "C-Language Frontend (as C Linker)",
        "clang", "-o", "-L", "-l",
        "-shared", "-dynamiclib",
        "-bundle", "-bundle_loader"
    ),
    LinkerInfo(
        "GNU C Compiler (as C++ linker)",
        "g++", "-o", "-L", "-l",
        "-shared", "-dynamiclib",
        "-bundle", "-bundle_loader"
    ),
    LinkerInfo(
        "GNU C Compiler (as C linker)",
        "gcc", "-o", "-L", "-l",
        "-shared", "-dynamiclib",
        "-bundle", "-bundle_loader"
    )
];
detect.Win32Linkers = [
    LinkerInfo(
        "Microsoft Windows Linker",
        "link.exe", "/Fe", "/LIBPATH:", ""
    )
];

// The programs that make static libraries.
// To my knowledge, there are only two. But i'll split it into multiple, for safety.
detect.StaticLibraryBuilder = [
    StaticLibraryInfo("GNU AR", "ar", "-rcs")
];
detect.Win32StaticLibraryBuilder = [
    StaticLibraryInfo("Microsoft LIB tool", "lib.exe", "/nologo", "/out:")
];

detect = detect + {
    // For test commands, build commands and the like.
    makeFlags: function(flag, list, equalize) {
        equalize = equalize || false;
        list = __.arrify(list);
        var outStr = [];
        var equal = equalize? "=" : "";
        for(var _,str in list) {
            outStr[] = flag .. equal .. str;
        }
        return outStr.join(" ");
    },

    makeCompilerArgs: function(args) {
        var keys = [
            "includeDirs",
            "libraryDirs",
            "libraries",
            "defines",
            "warnings",
            "compileFlags",
            "linkerFlags"
        ];
        for(var i,key in keys) {
            if(!(key in args)) {
                args[key] = [];
            }
        }
        return args;
    },

    cache: @{ return Cache("detect"); },
    toolCache: @{ return Cache("detect.tools"); },

    truthyCache: function(key) {
        var val = @cache[key];
        return toBoolean(val); // 0=false, 1=true, "abc"=true, ...
    },

    kindMap = extends _E {
        ASM: ["asm"],
        CC: ["c", "cc"],
        CXX: ["c++", "cxx", "cpp"],
        OBJC: ["objc", "obj-c", "objective-c"],
        OBJCXX: [
            "objcxx", "objc++", "objcpp",
            "obj-cxx", "obj-c++", "obj-cpp",
            "objective-cxx", "objective-c++", "objective-cpp"
        ],
        SWIFTC: ["swift","swiftc"],
        DC: ["d","dmd","dc"],
        CSC: ["c#", "csharp", "cs"],
        GOC: ["go","golang","goc"],
        RUSTC: ["rust","rustc","rst"],
        JAVAC: ["java","jvm","javac"]
    },

    extMap = extends _E {
        ASM:    ".S",
        CC:     ".c",
        CXX:    ".cpp",
        OBJC:   ".m",
        OBJCXX: ".mm",
        SWIFTC: ".swift",
        DC:     ".d",
        CSC:    ".cs",
        GOC:    ".go",
        RUSTC:  ".rust",
        JAVAC:  ".java"
    },

    nameMap = extends _E {
        ASM:    "Assembly",
        CC:     "C",
        CXX:    "C++",
        OBJC:   "Objective C",
        OBJCXX: "Objective C++",
        SWIFTC: "Swift",
        DC:     "D",
        CSC:    "C#",
        GOC:    "Go",
        RUSTC:  "Rust",
        JAVAC:  "Java"
    },

    // This stores the currently active compiler.
    activeCompilerMap = extends _E {},

    kind2ext: function(kind) {
        return @extMap[kind];
    },

    kind2name: function(kind) {
        return @nameMap[kind];
    },

    name2kind: function(name) {
        name = name.lower();
        for(var kind,names in @kindMap) {
            if(name in names) {
                return kind;
            }
        }
        throw "${name} is not valid!"
    },

    findToolViaEnv: function(key, list){
        var env = sys.getenv(key);
        if(env != "") {
            var envBase = pfs.basename(env);
            for(var _,entry in list) {
                var entryBase = pfs.basename(entry._toolName);
                if(envBase == entryBase) {
                    if(!(key in @_hasReported)) @success "${entry.name}: ${env} ($${key})"
                    var x = entry.clone();
                    x._toolName = env;
                    return true, x;
                }
            }
        } else return false;
    },

    findToolViaCache: function(kind, list) {
        // Look into the cache!
        if(kind in @toolCache) {
            var cached = @toolCache[kind];
            for(var _,entry in list) {
                if(entry._toolName == cached) {
                    if(!(kind in @_hasReported)) @success "${entry.name}: ${cached} (Cache)"
                    return true, entry;
                }
            }
        } else return false;
    },

    findToolViaList: function(kind, list) {
        debug "Searching ${kind} in ${list}..."
        for(var _,entry in list) {
            if(entry.findMe()) {
                if(!(kind in @_hasReported)) @success "${entry.name}: ${entry._toolName} OK!"
                @toolCache[kind] = entry._toolName;
                return true, entry;
            }
        }
        return false;
    },

    _hasReported: {},
    findCompiler: function(kind) {
        if(!(kind in @_hasReported)) @line "Looking for a ${@nameMap[kind]} compiler...";
        // Make a list...
        var compilerCollection = detect.Compilers;
        if(IceTea.searchWin32()) {
            compilerCollection = compilerCollection + detect.Win32Compilers;
        }
        var compilerList = compilerCollection[kind];

        // Look into the cache!
        var cached,cachedEntry = @findToolViaCache(kind, compilerList);
        if(cached) {
            @activeCompilerMap[kind] = cachedEntry;
            @_hasReported[kind]=true;
            return true;
        }

        // Do we have a compiler in the environment?
        // This might actually be a compiler.
        // Then again, we can only accept compilers,
        // if they are in our map. (flags, etc.)
        // Usually, the name of the env variable
        // is the name of a program.
        var inEnv,envEntry = @findToolViaEnv(kind, compilerList);
        if(inEnv) {
            @activeCompilerMap[kind] = envEntry;
            @_hasReported[kind]=true;
            return true;
        }

        var found, compiler = @findToolViaList(kind, compilerList);
        if(found) {
            @activeCompilerMap[kind] = compiler;
            @_hasReported[kind]=true;
            return true;
        }

        // No luck...
        if(!(kind in @_hasReported)) @fail "Unable to find a compiler!";
        terminate(1);
        return false;
    },

    // Store the active linker
    activeLinker: null,

    findLinker: function() {
        var kind = "LINKER"; // Special kind!
        if(!(kind in @_hasReported)) @line "Looking for a linker...";

        var linkerList = detect.Linkers;
        if(IceTea.searchWin32()) {
            linkerList = linkerList + detect.Win32Linkers;
        }

        // Look into the cache!
        var inCache, cacheEntry = @findToolViaCache(kind, linkerList);
        if(inCache) {
            @activeLinker = cacheEntry;
            @_hasReported[kind]=true;
            return true;
        }

        // ENV?
        var inEnv, envEntry = @findToolViaEnv(kind, linkerList)
        if(inEnv) {
            @activeLinker = envEntry;
            @_hasReported[kind]=true;
            return true;
        }

        var found, linker = @findToolViaList(kind, linkerList);
        if(found) {
            @activeLinker = linker;
            @_hasReported[kind]=true;
            return true;
        }

        if(!(kind in @_hasReported)) @fail "Unable find a linker!";
        terminate(1);
        return false;
    },

    activeStaticLibtool: null,

    findStaticLibraryTool: function() {
        var kind = "lIBRARY_TOOL";
        if(!(kind in @_hasReported)) @ine "Looking for a static library tool...";

        var linkerList = detect.StaticLibraryBuilder;
        if(IceTea.searchWin32()) {
            linkerList = linkerList + detect.Win32StaticLibraryBuilder;
        }

        // Look into the cache!
        var inCache, cacheEntry = @findToolViaCache(kind, linkerList);
        if(inCache) {
            @activeStaticLibtool = cacheEntry
            @activeCompilerMap[kind] = compiler;
            return true;
        }

        // ENV?
        var inEnv, envEntry = @findToolViaEnv(kind, linkerList);
        if(inEnv) {
            @activeStaticLibtool = envEntry;
            @activeCompilerMap[kind] = compiler;
            return true;
        }

        var found, tool = @findToolViaList(kind, linkerList);
        if(found) {
            @activeStaticLibtool = tool;
            @activeCompilerMap[kind] = compiler;
            return true;
        }

        if(!(kind in @_hasReported)) @fail "Unable find a linker!";
        terminate(1);
        return false;
    },

    // Checks. key is always a compiler key!
    header: function(name, header, runArgs){
        // This function can actually hceck mroe than just headers.
        // It could be used to check for packages/imports etc for other langs.
        // For now, I'll only implement C-style checks.
        name = name || "c";
        var kind = @name2kind(name)
        @line "Checking: ${header} (${@kind2name(kind)})..."

        var cacheKey = @haveHeader(header);
        if(cacheKey in @cache) {
            if(@truthyCache(cacheKey)) {
                @success "Found. (Cached)"
            } else {
                @fail "Not found. (Cached)"
            }
            return true;
        }

        var src_c = [
            "#include <${header}>",
            "int main(int argc, char** argv) {",
            "   return 0;",
            "}"
        ].join("\n");
        // Try to build. On success, true.
        if(@tryCompile(src_c, kind, runArgs)) {
            @success "Found.";
            @cache[cacheKey] = "1";
            return true;
        } else {
            @fail "Not found.";
            @cache[cacheKey] = "0";
            return false;
        }
    },
    lib: function(lib, name, runArgs){
        // FIXME: We could traverse the default library path.
        // For now, we just rely on the compiler.
        name = name || "c";
        var kind = @name2kind(name)
        var cacheKey = @haveLib(lib);
        @findCompiler(kind);
        @findLinker();
        @line "Checking for ${@kind2name(kind)} library ${lib}..."
        var src = "int main(int argc, char** argv) { return 0; }";
        var opts = { libraries: [lib] };
        if(__.isObject(runArgs)) {
            runArgs += opts;
        } else {
            runArgs = opts;
        }
        if(cacheKey in @cache) {
            if(@truthyCache(cacheKey)) {
                @success "Available. (Cached)"
            } else {
                @fail "Not available. (Cached)"
            }
            return true;
        } else if(@tryBuild(src, kind, runArgs)) {
            @success "Available.";
            @cache[cacheKey] = 1;
            return true;
        } else {
            @fail "Not available.";
            @cache[cacheKey] = 0;
            return false;
        }
    },
    func: function(name, func, args, runArgs){
        name = name || "c";
        var kind = @name2kind(name)
        var argStr = args && __.isArray(args) ? args.join(", ") : "";
        var cacheKey = @haveFunc(func);
        @findCompiler(kind);
        @line "Checking: ${func}(${argStr})...";
        var src = [
            "int main(int argc, char** argv){",
            "   ${func}(${argStr});",
            "   return 0;",
            "}"
        ].join("\n");
        if(cacheKey in @cache) {
            if(@truthyCache(cacheKey)) {
                @success "Found. (Cached)"
            } else {
                @fail "Not found. (Cached)"
            }
            return true;
        } else if(@tryCompile(src, kind, runArgs)) {
            @success "Found."
            @cache[cacheKey] = 1;
            return true;
        } else {
            @fail "Not found."
            @cache[cacheKey] = 1;
            return false;
        }
    },
    libfunc: function(lib, func, name, runArgs){
        name = name || "c";
        var kind = @name2kind(name)
        var cacheKey = @haveLibFunc(lib, func);
        var prefixedCacheKey = @haveLibFunc(lib, "_" .. func);
        @findCompiler(kind);
        @findLinker();
        var src = [
            "// We first need to #def us a means of making a library name.",
            "#ifdef _WIN32",
            "   #include <windows.h>",
            "   #define LIBNAME(l) TEXT(\"lib\" l \".dll\")",
            "#else",
            "   #include <unistd.h>",
            "   #include <dlfcn.h>",
            "   #ifdef __APPLE__",
            "       #define SHLIBEXT \".dylib\"",
            "   #else",
            "       #define SHLIBEXT \".so\"",
            "   #endif",
            "   #define LIBNAME(l) \"lib\" l SHLIBEXT",
            "#endif",
            "// This is just a helper macro for stringing.",
            "#define S(str) #str",
            "int main(int argc, char** argv) {",
            "   #ifndef _WIN32",
            "       // Process tracker. It's value determines the result.",
            "       int rt = 0;",
            "       void* dh = dlopen(LIBNAME(S(${lib})), RTLD_LAZY);",
            "       if(dh == NULL) {",
            "           // 1: The library could not be resolved.",
            "           rt = 1;",
            "           goto end;",
            "       }",
            "       void* symbol = dlsym(dh, \"${func}\");",
            "       if(symbol == NULL) {",
            "           // 2: No such symbol in this library. Trying again with prefix.",
            "           rt = 2;",
            "       } else {",
            "           // 3: Symbol found.",
            "           rt = 3;",
            "           goto end;",
            "       }",
            "       symbol = dlsym(dh, S(_) S(${func}));",
            "       if(symbol == NULL) {",
            "           // 4: Symbol not found, for real.",
            "           rt = 4;",
            "           goto end;",
            "       } else {",
            "           // 5: Symbol found with prefix.",
            "           rt = 5;",
            "           goto end;",
            "       }",
            "       end:",
            "           if(dh != NULL) { dlclose(dh); }",
            "           return rt;",
            "   #else // !defined(_WIN32)",
            "       FARPROC* fh = GetProcAddress(",
            "           GetModuleHandle(S(${lib}), S(${func}))",
            "       );",
            "       if(fh == NULL) {",
            "           // Windows: Library&Function not loadable.",
            "           return 6;",
            "       }",
            "       // 7: Loadable.",
            "       return 7;",
            "   #endif",
            "}"
        ].join("\n");

        @line "Checking: ${func}(...) in library ${lib}...";

        if(cacheKey in @cache || prefixedCacheKey in @cache) {
            if(@truthyCache(cacheKey) || @truthyCache(prefixedCacheKey)) {
                @success "Available. (Cached)"
            } else {
                @fail "Not available. (Cached)"
            }
            return true;
        } else {
            var compiled, spawned, rt, output
                = @tryRun(src, kind, runArgs);
            if(!compiled) {
                @fail "Unable to compile."
                return false;
            }
            switch(rt) {
                case 1:
                    @fail "Library not loadable.";
                    //@cache[libCacheKey] = 0;
                    return false;
                case 2:
                    // Never occurs.
                case 3:
                    // Without prefix.
                    @success "Available."
                    @cache[cacheKey] = 1;
                    @cache[prefixedCacheKey] = 0;
                    return true;
                case 4:
                    @fail "Function not found but library was.";
                    @cache[cacheKey] = 0;
                    @cache[prefixedCacheKey] = 0;
                    return false;
                case 5:
                    @success "Available (but with \"_\" prefix!)";
                    @cache[cacheKey] = 0;
                    @cache[prefixedCacheKey] = 1;
                    return true;
                case 6:
                    @fail "Not available.";
                    @cache[cacheKey] = 0;
                    @cache[prefixedCacheKey] = 0;
                    return false;
                case 7:
                    @success "Available.";
                    @cache[cacheKey] = 1;
                    @cache[prefixedCacheKey] = 0;
                    return true;
                default:
                    @fail "Unknown (${rt})"
                    return false;
            }
        }
    },
    headerfunc: function(header, func, name, runArgs){
        name = name || "c";
        var kind = @name2kind(name);
        var headerCacheKey = @haveHeader(header);
        var funcCacheKey = @haveFunc(func);
        @findCompiler(kind);
        var src = [
            "#include <${header}>",
            "int main(int argc, char** argv){",
            "   void* fp = (void*)${func};",
            "   return 0",
            "}",
        ].join("\n");
        @line "Checking: ${func}(...) in ${header}...";
        if(headerCacheKey in @cache && funcCacheKey in @cache) {
            if(@truthyCache(headerCacheKey) && @truthyCache(funcCacheKey)) {
                @success "Available. (Cached)"
            } else {
                @fail "Not available. (Cached)"
            }
            return true;
        } else if(@tryCompile(src, kind, runArgs)) {
            @success "OK!"
            @cache[headerCacheKey] = 1;
            @cache[funcCacheKey] = 1;
            return true;
        } else {
            @fail "Not available."
            @cache[headerCacheKey] = 0;
            @cache[funcCacheKey] = 0;
            return false;
        }
    },
    type: function(typeName, name, args, runArgs){
        name = name || "C";
        var kind = @name2kind(name);
        @findCompiler(kind);
        var cacheKey = @haveType(typeName);
        var argStr = __.isArray(args)
            ? "(" .. args.join(", ") .. ")"
            : __.isString(args) ? args : "";
        var src = [
            "int main(int argc, char** argv) {",
            "   ${typeName} x" .. argStr .. ";",
            "   return 0;",
            "}"
        ].join("\n");

        @line "Checking for type ${typeName}...";

        if(cacheKey in @cache) {
            if(@truthyCache(cacheKey)) {
                @success "Available. (Cached)"
            } else {
                @fail "Not available. (Cached)"
            }
            return true;
        } else if(@tryCompile(src, kind, runArgs)) {
            @success "OK!"
            @cache[cacheKey] = 1;
            return true;
        } else {
            @fail "Not available."
            @cache[cacheKey] = 0;
            return false;
        }
    },
    typeHeader: function(typeName, header, name, args){
        name = name || "CC";
        var kind = @name2kind(name);
        @findCompiler(kind);
        var cacheKey = @haveTypeInHeader(typeName, header);
        var argStr = __.isArray(args)
            ? "(" .. args.join(", ") .. ")"
            : __.isString(args) ? args : "";
        var src = [
            "#include <${header}>",
            "int main(int argc, char** argv) {",
            "   ${typeName} x" .. argStr .. ";",
            "   return 0;",
            "}"
        ].join("\n");

        @line "Checking for type ${typeName} in ${header}..."

        if(cacheKey in @cache) {
            if(@truthyCache(cacheKey)) {
                @success "Available. (Cached)"
            } else {
                @fail "Not available. (Cached)"
            }
            return true;
        } else if(@tryCompile(src, kind, args)) {
            @success "OK!"
            @cache[cacheKey] = 1;
            return true;
        } else {
            @fail "Not available."
            @cache[cacheKey] = 0;
            return false;
        }
    },

    // Tool tests.
    // We always look in cache, $PATH, IceTea.$PATH
    tool: function(name){
        var res = sys.which(name);
        @line "Checking for tool ${name}..."
        if(name in @toolCache) {
            @success "OK! ${@toolCache[name]} (Cache)"
            return true;
        } else if(res != "") {
            @success "OK! ${res}"
            @toolCache[name] = res;
            return true;
        } else {
            @fail "Not found."
            return false;
        }
    },
    toolTest: function(name, args){
        var p = SubProcess({async: false});
        // FIXME: Implement Arg_Vector, and make it possible to pass it's pointer across.
        var argStr = __.isArray(args) ? args.join(" ") : args;
        if(!@tool(name)) {
            return false;
        }
        @line "Checking if ${name} accepts ${argStr}...";
        var spawned = p.execute("${name} ${argStr}");
        var exitCode = p.exit_code();
        if(p.error()) {
            @fail "Error: ${p.error_text()}"
            return false;
        } else if(spawned) {
            if(exitCode == 0) {
                @success "OK!"
                return true;
            } else {
                @fail "Failed: ${exitCode}"
                return false;
            }
        } else {
            @fail "Failed to create process."
            return false;
        }
    },

    // Test compiler flags.
    tryCompilerFlag: function(args, kind){
        if(!__.isString(args) && !__.isArray(args)) {
            var t = typeOf(args);
            throw "Can't test compiler flag(s). Args aren't array or string, but '${t}' instead."
        }
        kind = kind || "CC";
        if(!(kind in @activeCompilerMap)) {
            var rt = @findCompiler(kind);
            if(!rt) return rt;
        }
        var compiler = @activeCompilerMap[kind];
        var argStr = __.isArray(args) ? args.join(" ") : args;
        @line "Checking if ${@nameMap[kind]} compiler supports '${argStr}'"
        var p = SubProcess({async: false});
        var src = "int main(int argc, char** argv){ return 0; }";
        var ext = @kind2ext(kind);
        if(pfs.mkdir(pfs.join(__outputdir, "tests"))) {
            var fnameBase = pfs.join(__outputdir, "tests", "simple_test");
            var fname = "${fnameBase}${ext}";
            var fout = "${fnameBase}.x"; // FIXME: Look for a step, that provides an output extension.
            var rt = File.writeWhole(src, fname);
            debug "File.writeWhole: ${rt}"
            var cmd = compiler.buildCommand(
                // FIXME: I...need a simpler interface. xD
                fname, fout,
                [], [], [],
                args, "none", [],
                false, false
            );
            debug "test$ ${cmd}"
            if(p.execute(cmd)) {
                if(p.exit_code() == 0) {
                    @success "Yes."
                    return true;
                } else {
                    @fail "No."
                    return false;
                }
            } else {
                @fail "No."
                return false;
            }
        } else {
            @fail "Could not make test folder."
            return false;
        }
    },
    tryCompile: function(source, kind, args){
        if(!(kind in @activeCompilerMap)) {
            var rt = @findCompiler(kind);
            if(!rt) return rt;
        }
        var compiler = @activeCompilerMap[kind];
        args = @makeCompilerArgs(args);
        var p = SubProcess({async: false});
        var ext = @kind2ext(kind);
        if(pfs.mkdir(pfs.join(__outputdir, "tests"))) {
            var name = sha2.string(source);
            var fnameBase = pfs.join(__outputdir, "tests", name);
            var fname = "${fnameBase}${ext}";
            var fout = "${fnameBase}.x";
            var rt = File.writeWhole(source, fname);
            debug "File.writeWhole: ${rt}"
            var cmd = compiler.buildCommand(
                fname, fout,
                args.includeDirs, [], [],
                args.compileFlags, "none", [],
                false, false
            );
            debug "tryCompile> ${cmd}"
            if(p.execute(cmd)) {
                if(p.exit_code() == 0) {
                    return true, fout;
                } else {
                    debug "Failed to build: ${p.exit_code()}"
                    if(#p.stderr() > 0) debug p.stderr();
                    if(#p.stdout() > 0) debug p.stdout();
                    return false;
                }
            } else {
                debug "Unable to run compiler (${p.exit_code()}, [${#p.stderr()} ${#p.stdout()}])."
                if(#p.stderr() > 0) debug p.stderr();
                if(#p.stdout() > 0) debug p.stdout();
                return false;
            }
        } else {
            debug "tryCompile: Could not make test folder."
            return false;
        }
    },
    tryBuild: function(source, key, args){
        var compiled,fout = @tryCompile(source, key, args);
        args = @makeCompilerArgs(args);
        if(compiled) {
            if(__.isNull(@activeLinker)) {
                @findLinker();
            }
            var linker = @activeLinker;
            var p = SubProcess({async: false});
            // FIXME: Same as with compile, use steps/rules to find extension.
            var binout = "${fout}.bin";
            var cmd = linker.linkCommand(
                fout, binout,
                args.libraries, args.libraryDirs,
                args.linkerFlags, false, false
            );
            debug "tryBuild> ${cmd}"
            if(p.execute(cmd)) {
                if(p.exit_code() == 0) {
                    return true, binout;
                } else {
                    debug "tryBuild> exit: ${p.exit_status()}"
                    #p.stderr() > 0 && debug p.stderr();
                    #p.stdout() > 0 && debug p.stdout();
                    return false;
                }
            } else {
                return false;
            }
        } else {
            debug "tryBuild> It did not compile."
            return false;
        }
    },
    tryRun: function(source, key, args){
        var compiled,binary = @tryBuild(source, key, args);
        if(compiled) {
            debug "tryRun> Running: ${binary}";
            var p = SubProcess({async: false})
            var spawned = p.execute(binary);
            debug "tryRun> Exit: ${p.exit_code()} Spawned: " .. (spawned ? "yes" : "no");
            return
                true,
                spawned,
                p.exit_code(),
                [null, p.stdout(), p.stderr()];
        } else {
            return false;
        }
    },

    define: function(key, value) {
        @cache[key] = value;
    },

    // Use the C/C++/ObjC/ObjC++ preprocessor.
    transform: function(inFile, outFile){
        outFile = outFile || pfs.basename(inFile); // strips .in...in an evil way.
        @info "Transforming: ${inFile} -> ${outFile}"
        var c = compileFile(inFile);
        var output = Buffer();
        c.callEnv({
            echo: {|str| output.append(str); }
        }, _G);
        return File.writeWhole(toString(output), outFile);
    },
    transformInline: function(str, env){
        env = env || {};
        var c = compileString(str);
        var output = Buffer();
        c.callEnv(env + {
            echo: {|str| output.append(str); }
        }, _G);
        return toString(output);
    },

    getDefines: function(){
        var data = {};
        var cacheKeys = @cache.keys;
        for(var _,key in cacheKeys) {
            var val = @cache[key];
            if(toBoolean(val)) {
                data[key] = val;
            }
        }
        return data;
    },
    writeHeader: function(where) {
        @info "Writing header: ${where}"
        var defines = @getDefines();
        var src = [];
        for(var k,v in defines) {
            if(__.isString(v)) {
                // FIXME: Better checks.
                // Or, just put falsy stuff in a different container...
                if(v == "0") {
                    continue;
                }
            }
            src[] = "#define ${k} ${v}"
        }
        var source = src.join("\n");
        var hash = sha2.string(source);
        var id = "ICETEA__${hash}__CONFIG_H";
        var head = "#ifndef ${id}\n#define ${id}\n";
        var foot = "\n#endif // ${id}"
        var outSrc = head .. source .. foot;
        return File.writeWhole(outSrc, where);
    },
    writeJson: function(where){
        @info "Writing JSON header: ${where}"
        var defines = @getDefines();
        return File.writeWhole(json.encode(defines), where);
    },

    // CLI shortcuts
    // TODO: Get parent target's name,
    // or just the currently configured one?
    with: function(name, desc, arg) {
        cli.insert {
            longopt: "--with-${name}",
            desc: __.isNull(desc) ? "With: ${name}" : desc,
            arg: __.isNull(arg) ? "<...>" : arg
        }
    },
    enable: function(name, desc) {
        cli.insert {
            longopt: "--enable-${name}",
            desc: __.isNull(desc) ? "Enable: ${name}" : desc,
            arg: "<Bool>"
        }
    },
    disable: function(name, desc) {
        cli.insert {
            longopt: "--disable-${name}",
            desc: __.isNull(desc) ? "Disable: ${name}" : desc,
            arg: "<Bool>"
        }
    },
    enabled: function(name) {
        return cli.check("--enable-${name}");
    },
    disabled: function(name) {
        return cli.check("--disable-${name}");
    },
    isWith: function(name) {
        return
            cli.check("--with-${name}"),
            cli["--with-${name}"];
    }
};
