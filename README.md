# IceTea - couldn't be chillier.

Are you also a developer that wishes to work cross-platform with as less of an efford as possible? Then you most probably have ran into CMake, premake or even GYP. But were you satisfied with them? Most of those that I met, weren't. Then there are also the traditional tools - autotools (autoconf, autoheader, auto...) for UNIX and MSBuild for Windows - that can be very frustrating, too.

But to top it all, what if you wanted to add a project into your own, i.e. a library?

I think you can relate to at least one of those issues. And if you can't, then there are still even more reasons why you should check out IceTea.

## It's as easy as opening a bottle of ice tea!
```javascript
target("myProg", "exe") {
    input: ["src/*.cpp"],
    CXX: {
        include_dirs: ["src/"]
    }
}

```

This will compile all files in `src/` into an executable called `myProg`. It will also create an output folder and build all objects with the prefix `myProg-` - and then link them into the top-level.

But what if you wish to run a bit of configuration?

```javascript
target("myLib", "shlib") {
    input: ["lib/*.cxx"],
    configure: function() {
        // *.in -> *. Basically, config.h.in becomes config.h
        // To apply our own configuration into this file, just add @ICETEA_CONFIG@! :)
        config.transform("lib/config.h.in");
        config.sys_header("stdio.h");
        config.sys_header("sys/dirent.h");
        config.stdc_headers(); // Checks for all standart C headers.
        config.define("MY_NAME","Value"); // @MY_NAME@ -> Value
        config.sys_library("z");
        // We can use the following to automatically extend our target.
        // In this case, we'd add: libraries: ["curl"]
        config.add_sys_library("curl");
        config.function("popen"); // Check if we have popen() by default.

        // When we're done, we finalize it.
        config.finalize();
    },
    CXX: { include_dirs: ["lib/"] }
}
```

In this case, before any target is built, the `configure()` method is called upon the target. If it fails, the entire process fails.

Now we take it a step further. Imagine you wish to have a conditional configuration.

```javascript
target("mySpecialLib", "lib") {
    input: ["static_lib/*.c"],
    prepare: function() {
        // We can add options to the help screen here!
        // When we do, its prefixed with our target's name, too.
        cli.insert {
            longopt: "--curl-path",
            arg: "dir",
            desc: "Path to your libCURL source"
        }
        // But wait. We can also do...
        // If enabled, it'll be true. Otherwise, false.
        config.enable("foo", "Enable functionality foo");
        // name, description, default
        config.with("libmeep", "Use libmeep for a functionality", "no");
    }
    configure: function() {
        if(config.enabled("foo")) {
            // Do something because foo was enabled.
        }
        if(config.with("libmeep")) {
            if(fs.isDir(config.withValue("libmeep"))) {
                // Do something, if the value of --XYZ-with-libmeep is a dir.
            }
        }
    }
}
```

In the above example, we use the `prepare()` method to prepare the rest of the target that can not statically be written in. Within `configure()` we check if `--with-libmeep` was specified, and get it's "value". If it has one, it returns it. Otherwise it returns false. But since we gave it a default, "no" is returned. `config.with()` only returns wether it was supplied or not.

You dont even need to specify a wildcard for your paths. Imagine you have some test cases and want to build them all!

```javascript
for(var _,testfile in fs.glob("test/*.cpp")) {
    var testname = fs.stripExtension( fs.basename(testfile) );
    target(testname, "exe") {
        input: [testfile],
        // Get a dependency and add it's exports to this target.
        needs: ["myMainProject"]
    }
}
```

You have multiple ways of getting a dependency: Via the full name, or multiple via a tag:

```javascript
target("foo-ext-a", "lib") {
    ...
    tag: "foo-ext"
}
target("foo-ext-b", "lib") {
    ...
    tag: "foo-ext"
}
target("bar", "exe") {
    ...
    needs: tag("foo-ext") + ["somethingElse"] // -> foo-ext-a, foo-ext-b, somethingElse
}
```

Use the plus (`+`) operator to combine arrays, since `tag()` returns all targets matching a tag.

This is all I am going to show for now... See for a bit of backstory below!

## Inspiration
Okay... So I was working a lot with script extensions and had to use either `phpize`, `node-gyp` or something else. If I wanted to port a library to multiple languages, I was quickly caught up rewriting and typing redundant information. So I wanted to be able to do something as simple as:

```javascript
target("ttvfs", "lib") {...}
target("php-ttvfs", "php-ext") {...}
target("nodejs-ttvfs", "nodejs-ext") {...}
target("TTVFS", "osx-framework") {...}
```

This would build `libttvfs` and then use some extra sources or other settings to build either `TTVFS.framework` for Mac OS X, `ttvfs.so` for PHP and `ttvfs.node` for NodeJS.

But that was likely impossible. Some people wrote shims over existing build systems - I have seen some for CMake - but I have a reason why I do not like those other build systems:

- CMake:
    * Weird syntax.
    * Poor documentation.
    * Its usually bigger than any project you build with it.
- GYP
    * Requires Python
    * Has zero configuration methods. I used [cDetect](https://github.com/IngwiePhoenix/cDetect) to get around that.
- premake
    * It looks good, but its syntax doesnt seem very readable to me. Although LUA is very powerful.
- Scons
    * Again, Python.
    * Slow, and hard to learn.
    * It's not very clear what files are ment for what and how it has to be maintained.
- Tup
    * Per-Folder files
    * Very compact syntax doe snot give away what is ment for what.

Ninja does not go into the above list, simply because its a pure build tool an dits only purpose is to read in generated Ninja files and act upon what they read.

Therefore I created IceTea - well, still AM creating it. It runs off [ObjectScript](http://github.com/unitpoint/objectscript) for the most part, uses [ttvfs](https://github.com/fgenesis/ttvfs) for its filesystem management, a modified variant of [picosha2](https://github.com/okdshin/PicoSHA2) for hashing files in a cache. There is also my own `cli.h` that supports grouped arguments, whose idea was taken from another argument parser called `NBCL`.

IceTea is ment to be compact (currently 900kb when built with `-O3`) and simple. I am not directly aiming for speed, but since you can see `TinyThreads++` in there, you probably guessed already that I want to run build jobs on multiple threads to speed up the build time. I think it's called parallel building...

So far, so good. If you want to look into the code, go ahead. To build a test binary, just run:

```
g++ -Isrc -Wno-switch src/*.cpp -o icetea
```

and you are good to go.


## API
Within all and every IceTea (aka. ObjectScript) files, you have a set of functions and objects available. I will explain to you how these functions and objects look like.

### cli: command line interface
| Method                   | Description                                     |
|--------------------------|-------------------------------------------------|
| .group(*string* Name)    | Add a new group to the command line output.     |
| .insert(*object* Option) | Insert a new option into the current group.     |
| .check(*string* Name)    | Check if __shortopt__ or __longopt__ was given. |
| .value(*string* Name)    | Similar to .check() but returns the value.      |


#### object: Option
This object must contain:

* **shortopt**: Short option. (-h)
* **longopt**: Long option name. (--help)
* **desc**: Description.
* **arg**: Argument description. (I.e.: "--dir **dir**")
* **optional**: If true, this option must not be given.
* **value**: Default value that should be given.

**Option** is expected to have either **longopt** OR **shortopt** set - or both.

### detect: Configuration utility
This object is being used to check for various things on the host.

| Method                   | Description                                     |
|--------------------------|-------------------------------------------------|
| .header(name)            | Check for a system header ala `#include <...>`  |
| .lib(name)               | Check for a system library ala `gcc ... -lname` |
| .function(name)          | Check if a function is available by default.    |
| .libfnc(lib, name)       | Check if a function is in a library.            |
| .headerfunc(hdr, name)   | Check if a function is in a specific header.    |
| .checkfnc(lib, hdr, n)   | Check for function in header AND library.       |
| .with(name, desc, arg)   | Add a --with-NAME flag.                         |
| .hasWith(name)           | Check if --with-NAME was given.                 |
| .withValue(name)         | Get the --with-NAME value.                      |
| .enable(name)            | Add an --enable-NAME flag.                      |
| .enabled(name)           | Check if --enable-NAME was given.               |
| .tryCompile(kind, src)   | Try to compile source of kind (name of a rule)  |
| .tryRun(kind, src)       | Try to run the source. Returns bool AND output. |
| .preprocess(kind, src)   | Run src thru kind's preprocessor, if given.     |
| .tool(name)              | Check if tool is available on the host.         |
| .transform(name)         | Transform `name.in` to `name`                   |
| .transform(from, to)     | Like above but target and destination is given. |
| .set(name, value)        | Define a configuration value.                   |
| .get(name)               | Get a configuration value.                      |
| .detector_dir(name)      | Set working directory for the detector.         |

#### Info to .finalize(...)
Once called, it will DIRECTLY begin to transform. Set this at the very bottom!

#### Info to .set(...)
When you `.transform()` a file, you can get values with `@NAME@`. The only special @-Variable is `@ICETEA_CONFIGURE@` which will print all macros (`#define ...`) that it gathered. Use that in soemthing like `config.h`.

#### Information to the kind value.
When you supply the Kind attribute, the engine will place a temporary source file into the current folder and run it thru the casual rules. (The produced objects and alike are deleted afterwards.)

### fs: File System (ttvfs)
The Filesystem. Very important.

| Method                   | Description                                     |
|--------------------------|-------------------------------------------------|
| .mkdir(name)             | Create a folder.                                |
| .mkdirp(name)            | Create all folders in the path if not existing. |
| .delete(name)            | Delete a file.                                  |
| .deleteDir(name)         | Delete a folder.                                |
| .open(name, mode)        | Returns a File object. Mode: r / w              |
| File.read(bytes)         | Read `bytes` and return the result.             |
| File.tell()              | Tell the current position.                      |
| File.write(str)          | Write `str` to current position and go to end.  |
| File.size                | Filesize                                        |
| .getFileList(dir)        | Returns all FILES in a folder.                  |
| .getDirList(dir)         | Returns all FOLDERS within a folder.            |
| .glob(path)              | Return a list of files matching the pattern. *  |
| .move(from, to)          | Move a file or folder.                          |

#### Info about .glob()
Use only simple paths as only the file-part will be globbed.
- `tests/*.cpp` Will work.
- `tests/*/*.cpp` Will not work.
- `tests/**/*.cpp` Won't work either.

### sha2: Hashing
This is used internally and is also being exported.

| Method                   | Description                                     |
|--------------------------|-------------------------------------------------|
| .file(file)              | Return the SHA2 of a file.                      |
| .string(str)             | Return the SHA2 of a string.                    |

### IceTea's "configurable" functions

| Configurable             | Description                                     |
|--------------------------|-------------------------------------------------|
| target(name, rule) {...} | Define a target and associate to rule.          |
| rule(name, title) {...}  | Define a rule. name -> targets. title -> screen |
| action(name) {...}       | An action.                                      |
| external(name) function  | Define an external target. More below.          |
| project(name)            | Define the current file to be this project.     |
| IceTea(object)           | Configure IceTea. The call in `build.it` counts |
| configurable(name) func  | Create a configurable yourself.                 |
| $(...)                   | Run an external program.                        |

#### Actions
These have the following properties:

* __clean__: Clean these targets - array.
* __build__: Build those targets - array.
* __install__: Install those targets - array.
* __uninstall__: Uninstall those targets - array.

However, if the action is not an object but a function instead, then this will be ran. You can use `build()`, `clean()`, `install()` and `uninstall()` just like in the object above, but can put things under conditions. All these functions take arrays as arguments. So you can call them as:

```javascript
action("dist") function() {
    if(detect.tool("tar")) {
        $("tar cvfz mydist.tgz ${__DIR__}/*.txt ${__DIR__}/out ${__DIR__}/include");
    }
}
```

#### Rules
Rules are what makes IceTea a nice tool. You can define them and base them on one another. This is a basic example:

```javascript
rule("myrule", "My custom rule") {
    accepts: ["*.my"],
    output: {
        pattern: "*.omy",
        expected: "%t.omy"
    },
    build: ["omytool ${INPUT} -o ${OUTPUT}"]
}
```

A rule needs to have the following fields:

* `accepts`: This pattern, using a wildcard, defines which files this rule can accept.
* `output`: Is an object that has two members:
    - `pattern`: The pattern of the output file.
    - `expected`: What the output is expected to look like. The %-parameters are:
        - *%t*: Target name
        - *%b*: Basename of the input file without extension.
        - *%e*: File extension
        - *%f*: The actual input file
* `build`: This can either ben array OR a function. The definitions are:
    - *Array*: Each shell command is run alone but one after another. You have these variables:
        - `INPUT`: String version of your input. If it was an array, it got turned into a space-separated string.
        - `OUTPUT`: The expected output.
        - `TARGET`: The target's name.
    - *Function*: The function will be ran within the queue, but only one at a time due to OS not being multi-threaded. Your function receives these arguments:
        - `input`: The input. Always an array.
        - `output`: The expected output.
        - `targetName`: The target's name.
        - `target`: The actual target object. Use this to grab settings etc.
* `clean` and `finalize` are the same as `build`. The difference is that...
    - `clean` is called when a target is not built, but cleaned,
    - `finalize` is called after a target is built,
    - `install` is invoked when a target is installed
    - and `uninstall` when it should be removed.

It should be noted, that only if one of the latter exists, they will be used. If no `clean` function exists, then the engine will just delete the output. But maybe you produced dependency files or response files and want to clean them as well. Use these functions for that.

Additionally, you can also provide a `prepare()` function that will be called as soon as a target needs this rule. For instance, a C++ rule will check if a C++ compiler is available. If not, ti'll fail. It must return false to fail.

The first argument to the `rule()` command is the name that targets can reffer to. The latter is the display name displayed during the actual build process.

#### Targets
A target is what is needed at the very foundation to build a project - its the input to the rules. A target usually looks like this:

```javascript
target("foo", "lib") {
    input: ["src/*.c"],
    settings: {
        native: {
            include_dirs: ["src"]
        }
    }
}
```

The above has a `native` setting. This one is used independent of the current compiler. All compilers (C, C++, ObjC, ObjC++) will use flags and settings from this object.

A target has to define the following options:

* `input`: The input to the rule, always an array.

These are optional settings that can be given:

* `settings`: Rules pull information from here to form build commands, such as include directories, warning flags, or other useful things. The usual groups are:
    - native
    - C
    - CXX
    - ObjC
    - ObjCXX
    - SHARED
    - INSTALL
    - Some platform specific ones:
        * Mac OS X
            - App: Plist, Resources
            - Framework: Plist, Resources, Name
            - Bundle: Extension, Plist, Resources, Name
            * **NOTE**: Name is target's name by default.
        * Windows
            - Resources
        * Linux: Has none so far.
* `init`: A function called on target initialization.
* `configure`: A function to configure the project. Gets no arguments and needs to return false on failure.
* `finalize`: A function that can finalize a target after build.
* `clean`: A function that can clean the target away.
* `install`: A custom install method, not needed in most cases.
* `uninstall`: A custom uninstall method.
* `exports`: An object of `settings` that willb e merged into any target that depends on this one.
* `requires`: An array of targets (hint: use the `tag()` method to grab multiple targets by tag) that are required for this one. It will first process the other targets before processign this one.
* `tag`: A string that is a tag. Useful when many targets should respond to the same name.

#### External
A target can rely on an external target that has no IceTea file. For instance, if you wish to use LLVM but this one uses CMake and Autoconf, then you could do this:

```javascript
external("llvm") {
    build: function(){
        if(detect.tool("cmake")) {
            fs.mkdir(fs.join(IceTea.outDir, "llvm"));
            fs.chdir(IceTea.outDir);
            fs.chdir("llvm");
            $ "cmake ${SRC_DIR}/llvm"
            $ "make"
        }
    },
    clean: function() {
        fs.chdir(IceTea.outDir);
        fs.chdir("llvm");
        $ "make clean"
    },
    exports: {
        LINK: {
            libraries: ["llvm-core", "llvm-codegen", /*...*/]
        },
        native: {
            include_dirs: [fs.join(SRC_DIR, "llvm/include"), /*...*/]
        }
    }
}
```

Now you can have a target rely on LLVM. LLVM will occupy a thread to configure and compile, and once that is done, your target will begin to compile and at last link against LLVM. This will save you a lot of troubble when dealing with a variety of build tools. you can even make this one more step useful if you let your code decide if it should rather create an external target or not - if it should download a source tarball if it doesn't exist yet and re-run IceTea and then use an actual target for it, or not! There are many ways you will enjoy `external()`.

#### Running shell commands.
As you saw above, you can use the `$()` function to run commands. And since OS does not require parantheses if oly one argument is given, it will look a bit like an actual line within a terminal. But there is way more to it. For instance, it supports placeholders:

```javascript
$("cd ? && rm ?", SRC_DIR, "foobar");
```

But more useful would be to know, that whenever you use this, the actual questionmark is replaced with a quoted, escaped string. For instance:

```javascript
$("echo '?'", "This is'sa really \"healthy\" command.")
// -> echo 'This\'sa really \"healthy\” command'
```

Furthermore, you can specify a shell as well. For instance, you could even do something like this:

```javascript
$("#!/usr/bin/php -e ?", "print_r($_SERVER)");
// -> /usr/bin/php -e "print_r($_SERVER)"
// -> Array(...)
```

This is useful if you want to embed scripts from another language into your process. You simply need to have the typical hashbang (`#!`) infront of your command, and this function will automatically use this as a shell instead.

Further, this function returns two things - an integer and an array. You can obtain both by doing:

```javascript
var exitCode, output = $(...)
```

The `exitCode` holds the exit code, whilst `output` holds an array with 1 to 2 whilst 1 is STDOUT and 2 is STDERR, just like you would expect in a normal terminal. This is useful if you are operating on command outputs. ObjectScript allows you to seach find and parse strings. For instance:

```javascript
var exitCode, output = $ "clang -dumpmachine";
var tripple = output[1].split("-");
var ARCH, OEM, KERNEL = tripple[0], tripple[1], tripple[2];
```

Useful, if you are cross-compiling.

#### Project
If you are working with multiple projects and wish to keep them organized in a way, you can do so by specifying a project. The current file will be treatened to be of this project. Any output will have a new subfolder with the project's name. For instance:

If no project is specified, and target "foo" is compiled, then:

    out/.foo/myfile.o
    out/.foo/foomyotherfile.o
    out/foo

But if it is the member of project Bar...

    out/Bar/foo/myfile.o
    out/Bar/foo/myotherfile.o
    out/Bar/foo

If this is the case, you can also specify a target's main output to be put into the global folder. For that, do this:

```javascript
settings: {
    icetea: {
        to_global_dir: true
    }
}
```

`icetea` is a special settings segment and must not be exported.

#### configurable(name)
You like how `target()` and `rule()` look and feel with coding? You can make your own. If you are extending IceTea script-wise, you can do so by writing this:

```javascript
configurable("bar") function(head, body) {
    // ...
}

bar("My Bar") { string: "is so cool." }
```

As you can see, `bar()` was created using a callback. The `head` section contains the arguments passed to the function itself, whilst `body` contains the single parameter.

This is useful if you want to extend IceTea with a similar design as itself. Another example to create useful configurables would be:

```javascript
function static_lib(name) { return target(name, "lib"); }
static_lib("foo") { /*...*/ }
```

#### IceTea
Only within `build.it`, you can use this function. It acts as a setup function that allows you to tell IceTea how to display things, where to put output files and alike.
