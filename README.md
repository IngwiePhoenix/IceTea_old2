# IceTea - couldn't be chillyer.

Are you also a developer that wishes to work cross-platform with as less of an efford as possible? Then you most probably have ran into CMake, premake or even GYP. But were you satisfied with them? Most of those that I met, weren't. Then there are also the traditional tools - autotools (autoconf, autoheader, auto...) for UNIX and MSBuild for Windows - that can be very frustrating, too.

But to top it all, what if you wanted to add a project into your own, i.e. a library?

I think you can relate to at least one of those issues. And if you can't, then there are still even more reasons why you should check out IceTea.

## It's as easy as opening a bottle of ice tea!
```
target("myProg", "exe") {
    input: [“src/*.cpp”],
    CXX: {
        include_dirs: ["src/"]
    }
}
```

This will compile all files in `src/` into an executable called `myProg`.

But what if you wish to run a bit of configuration?

```
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

In this case, before any target is built, the configure() method is called upon the target. If it fails, the entire process fails.

Now we take it a step further. Imagine you wish to have a conditional configuration.

```
target("mySpecialLib", "lib") {
    input: ["static_lib/*.c"],
    init: function() {
        // We can add options to the help screen here!
        // When we do, its prefixed with our target's name, too.
        cli.insert {
            longopt: "--curl-path",
            arg: "dir",
            desc: "Path to your libCURL source"
        }
        // But wait. We can also do...
        config.enable("foo", "Enable functionality foo", "false"); // name, description, default
        config.with("libmeep", "Use libmeep for a functionality", "false");
    }
    configure: function() {
        if(config.enabled("foo")) {
            // Do something because foo was enabled.
        }
        if(fs.isDir(config.withValue("libmeep"))) {
            // Do something, if the value of --XYZ-with-libmeep is a dir.
        }
    }
}
```

You dont even need to specify a wildcard for your paths. Imagine you have some test cases and want to build them all!

```
for(testfile in fs.glob("test/*.cpp")) {
    var testname = fs.stripExtension( fs.basename(testfile) );
    target(testname, "exe") {
        input: [testfile],
        // Get a dependency and add it's exports to this target.
        needs: ["myMainProject"]
    }
}
```

You have multiple ways of getting a dependency: Via the full name, or multiple via a tag:

```
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
    needs: tag("foo-ext") + ["somethingElse"] // -> foo-ext-a, foo-ext-b
}
```

Use the plus (`+`) operator to combine arrays, since `tag()` returns all targets matching a tag - or even, tag pattern.

This is all I am going to show for now... See for a bit of backstory below!

## Inspiration
Okay... So I was working a lot with script selections and had to use either `phpize`, `node-gyp` or something else. If I wanted to port a library to multiple languages, I was quickly caught up rewriting and typign redundant information. So I wanted to be able to do something as simple as:

```
target("libttvfs", "lib") {...}
target("php-ttvfs", "php-ext") {...}
target("nodejs-ttvfs", "nodejs-ext") {...}
target("TTVFS", "osx_framework") {...}
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

Therefore I created IceTea - well, still AM creating it. It runs off [ObjectScript](http://github.com/unitpoint/objectscript) for the most part, uses [ttvfs](https://github.com/fgenesis/ttvfs) for its filesystem management, a modified variant of [picosha2](https://github.com/okdshin/PicoSHA2) for hashing files in a cache and `APConsolelib` for colorful and formatted output. There is also my own `cli.h` that supports grouped arguments, whose idea was taken from another argument parser called `NBCL`.

IceTea is ment to be compact (currently 900kb when built with `-O3`) and simple. I am not directly aiming for speed, but since you can see `TinyThreads++` in there, you probably guessed already that I want to run build jobs on multiple threads to speed up the build time. I think it's called parallel building...

So far, so good. If youw ant to look into the code, go ahead. To build a test binary, just run:

    g++ -Isrc -Wno-switch src/*.cpp -o icetea

and you are good to go.
