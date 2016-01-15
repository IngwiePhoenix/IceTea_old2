<img src="http://ingwie.me/img/icetea.png" alt="IceTea logo" height=200>
[![Gitter](https://badges.gitter.im/Join%20Chat.svg)](https://gitter.im/Deskshell-Core/PhoenixEngine?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge)˘
# IceTea - couldn't be chillier.

## Quick summary:
[![Build Status](https://travis-ci.org/IngwiePhoenix/IceTea.svg?branch=master)](https://travis-ci.org/IngwiePhoenix/IceTea)

Logo by [Jacob Redit](http://andnowtheworldismine.blogspot.com/) ([Email](reidt.jacob@gmail.com))


## Disclaimer
Are you also a developer that wishes to work cross-platform with as less of an efford as possible? Then you most probably have ran into CMake, premake or even GYP. But were you satisfied with them? Most of those that I met, weren't. Then there are also the traditional tools - autotools (autoconf, autoheader, auto...) for UNIX and MSBuild for Windows - that can be very frustrating, too.

But to top it all, what if you wanted to add a project into your own, i.e. a library?

I think you can relate to at least one of those issues. And if you can't, then there are still even more reasons why you should check out IceTea.

## Features
Here is a short rundown of IceTea's features

- __Turn-complete scripting language.__ Uses common and extended syntax, allowing vor a very nice amoutn of syntactic shugar while maintaining a consistency across every single line of code.
- __Configuring.__ IceTea can configure your project (aka. your good old friend `config.h`). It can also export this configuration as JSON, so you can use it for more than just IceTea.
- __Building.__ IceTea is a build tool by itself. It works via a Rule-Task basis, similar to Ninja.
- __Integration.__ You can integrate IceTea as a stand-alone script interpreter into your existing build system, adding either configuration or build abilities to it - or ismply execute neat scripts without worrying for an underlying shell.
- __Customizable.__ You can use `bootstrap.it` to override the default rules and tell IceTea how you want things to be handled.
- __Dynamic, yet static.__ You can use statically listed or danymically generated input lists, allowing you to choose your prefered method.
- __Dependencies.__ You can have many targets that can depend on one another.
- __Extensible.__ By creating a local `.IceTea` folder and dropping files with the `.it` extension into it, they will become automatically loaded when IceTea starts, allowing any 3rd-party integration to be included. Because these fiels are sorted, you can use numbers to decide in which orders these extensions are loaded!
- __Projects.__ Keep your build organized and your files sorted properly. Projects allow you to sort targets into groups and effectively make your build cleaner.
- __Multi-threaded.__ IceTea runs on multiple cores to provide as much speed as it possibly can.

## Building
IceTea is ultra, ultra tiny. Therefore there is just one command on UNIX based system and 3 on Windows. Or, `build.sh` on linux and `build.bat`on Windows. The reason is that an ASM macro, `.incbin`, is used to include script files directly into the binary. That does not work so well on Windows.

Unix:
    $ g++ -Wno-switch src/*.cpp -o icetea

Windows:
    > cl deps/incbin/incbin.c /Feincbin.exe
    > incbin src/scripts.rc -o scripts.cpp
    > cl /EHsc src/*.cpp scripts.cpp /Feicetea

### Note!
In order for the files to resolve correctly, make sure you navigated into the IceTea folder first. If you are using another program to run this, make sure to set the CWD of that of the IceTea root. For instance, you can pull down and start building like so:

    $ git clone --recursive https://github.com/IngwiePhoenix/IceTea.git
    $ cd IceTea
    ...

## Read more in the wiki!
Read further to get an overview. But when you're ready for the big walls of text, head to [the wiki](wiki).

## It's as easy as opening a bottle of ice tea!
```javascript
target("myProg", "exe") {
    input: pfs.glob("src", "*.cpp"),
    settings: {
        native: {
            includeDirs: ["src/"]
        }
    }
}
```

This will compile all files in `src/` into an executable called `myProg`. It will also create an output folder and build all objects with the prefix `myProg-` - and then link them into the top-level.

But what if you wish to run a bit of configuration?

```javascript
target("myLib", "shlib") {
    input: pfs.glob("lib", "*.cxx"),
    configure: function() {
        detect.header("stdio.h");
        detect.header("sys/dirent.h");
        detect.stdc_headers(); // Checks for all standart C headers.
        detect.set("MY_NAME", "Value"); // @MY_NAME@ -> Value
        detect.lib("z");
        if(detect.lib("curl")) {
            @settings.LINK.libraries[]="curl";
        }
        detect.func("popen"); // Check if we have popen() by default.

        // *.in -> *. Basically, config.h.in becomes config.h
        detect.transform("lib/config.h.in");
        // We can use the following call to create an "actual" header.
        detect.write_header("config.h");
        // If you are using another tool to post-process your config, you can...
        detect.write_json("config-h.json");
    },
    settings: {
        native: {
            includeDirs: ["lib/"],
            defines: ["HAVE_CONFIG_H"]
        }
    }
}
```

In this case, before any target is built, the `configure()` method is called upon the target. If it fails, the entire process fails.

Now we take it a step further. Imagine you wish to have a conditional configuration.

```javascript
target("mySpecialLib", "lib") {
    input: pfs.glob("static_lib", "*.c"),
    init: function() {
        // We can add options to the help screen here!
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
            if(pfs.isDir(config.withValue("libmeep"))) {
                // Do something, if the value of --XYZ-with-libmeep is a dir.
            }
        }
    }
}
```

In the above example, we use the `init()` method to prepare the rest of the target that can not statically be written in. Within `configure()` we check if `--with-libmeep` was specified, and get it's "value". If it has one, it returns it. Otherwise it returns false. But since we gave it a default, "no" is returned. `config.with()` only returns wether it was supplied or not.

You don't even need to specify a wildcard for your paths. Imagine you have some test cases and want to build them all!

```javascript
for(var _,testfile in pfs.glob("test/", "*.cpp")) {
    var testname = pfs.basename( testfile );
    target(testname, "exe") {
        input: [testfile],
        title: "MyApp Test: ${testname}",
        // Get a dependency and add it's exports to this target.
        needs: ["myMainProject"]
    }
}
```

You have multiple ways of getting a dependency: Via the full name, or multiple via a tag:

```javascript
target("foo-ext-a", "lib") {
    // ...
    tag: "foo-ext"
}
target("foo-ext-b", "lib") {
    // ...
    tag: "foo-ext"
}
target("bar", "exe") {
    // ...
    needs: tag("foo-ext") + ["somethingElse"] // -> foo-ext-a, foo-ext-b, somethingElse
}
```

Use the plus (`+`) operator to combine arrays, since `tag()` returns all targets matching a tag.

This is all I am going to show for now... See the wiki for more.

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

Ninja does not go into the above list, simply because its a pure build tool and its only purpose is to read in generated Ninja files and act upon what they read.

Therefore I created IceTea - well, still AM creating it. It runs off [ObjectScript](http://github.com/unitpoint/objectscript) for the most part, uses [STLPlus](https://stlplus.sourceforge.net) for its filesystem and subprocess management, a modified variant of [picosha2](https://github.com/okdshin/PicoSHA2) for hashing files in a cache. There is also my own `cli.h` that supports grouped arguments, whose idea was taken from another argument parser called `NBCL`.

IceTea is meant to be compact (currently 900kb when built with `-O3`) and simple. I am not directly aiming for speed, but since you can see `TinyThreads++` in there, you probably guessed already that I want to run build jobs on multiple threads to speed up the build time. I think it's called parallel building...

## Good resources
- http://stackoverflow.com/questions/28263062/topological-sorting-of-c-data-structures/30249477#30249477
