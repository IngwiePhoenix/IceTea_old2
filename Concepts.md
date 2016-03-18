# Concepts

Those are possible designs for feature implementations.
But, you may like to read them anyway! :)

Just keep in mind, my TODO list isn't short...

## Projects
```javascript
project({
    name: "Foo",
    version: "0.0.1",
    by: "Ingwie Phoenix"
}) {||
    target("foo", "exe") {
        input: ["foo.c"]
    }
}
```

## Toolchain
Allow the user to define custom toolchains. This allows a project to completely focus on building for either the Web with Emscripten/Cheerp/WASM or even use a custom compiler install like HHVM does!

This allows great flexibility.

Features:
- Inherit the steps of the parent
- Mark compatible to other toolchains. For instance, a project compiled with your custom clang compiler may actually still be compatible to the native compiler.

```javascript
toolchain("native") {||
    step(...)
    step(...)
    rule(...)
}
toolchain("emscripten") {||
    step(...)
    rule(...)
}
toolchain("cheerp", "native") {|super|
    // super is the scope of the parent.
    step(...)
    step(...)
    rule(...)
}
toolchain("custom-clang") {||
    // ...
    this.markCompatible("native");
}

target("foo", "exe") {
    toolchain: "cheerp",
    // ...
}

target("l1", "lib") {
    input: [...]
}
target("l2", "lib") {
    toolchain: "cheerp",
    input: [...]
}
target("bar", "exe") {
    needs: [
        "l1",
        "l2"    // <- Error: Target bar can not depend on l2: Toolchains dont match!
                // Make sure that either they inherit, marked compatible, or are the same!
    ]
}
```

## BDD generator
IceTea needs some way to test things. So test generators should be a thing.

```javascript
// build.it
target("foo", "exe") {...}
test("Basic", "foo") {
    scheme: "./test/basic_test.it",
    tests: pfs.glob("./test/basic/", "*_test.c")
}
```
```c
// basic_test.it
#include <testing_framework.h>
#include "<%= pfs.filename(@testcase.filename) %>.h" // i.e.: foo_test.h
int main(int argc, char** argv) {
    <%= @testcase.source %>
}
```
```c
// test/basic/foo_test.h
#include <assert.h>
// test/basic/foo_test.c
int life = 42;
assert(life == 42);
```

But maybe IceTea should have it's own BDD style stuff too, so it can spin up separate processes just dedicated to testing. In fact, it could be used as a general-purpose abstraction tool.
```javascript
describe("SubProcess", {||
    it("can run a sync process", {||
        // ...
        expect(stream[0]).toBeEmpty();
        expect(#stream[1] > 0).toBeTrue();
    });
});
```

## Rule/Step profiles
```javascript
step("*.c", "*.o") {
    pattern: "...",
    configure: function(){},
    // Same applies for rules.
    profiles: {
        build: function(){},
        clean: function(){},
        install: function(){}
    },
    status: function(){}
}
```

## Project relationships
- Refference nested targets with `::`.
    - In this example, we'd have these targets:
        * `A::a`
        * `B::b`
```javascript
project({
    name: "A",
    version: "1"
}) {||
    target("a", "lib") {
        // ...
    }
}
project({
    name: "B",
    version: "1"
}) {||
    target("b", "exe") {
        needs: ["A::a"]
    }
}
```

- Projects should at best consist of a single file. Maybe the `build.it`, or subsequent `.IceTea` scripts.
```javascript
// Much like require, but:
project("./deps/minihttp");
```
- A project called via `project(path)`:
    - has it's own, locally scoped `.IceTea` folder.
    - can export variables by attaching it to the `this` object.

## Versioned projects
```javascript
project({
    name: "X",
    version: "1.0.2"
}) {||
    target("a","exe") {
        needs: ["OtherProject@^0.1.0::foo"]
    }
}
```

## External packages (ITPM, IceTea Package Manager!!)
```javascript
target("example", "exe") {
    packages: {
        // Installs packages via the package manager.
        // The package manager to be "emulated" goes before the name
        // with a colon.
        // For instance:
        "clib:jwerle/fs.c": "0.1.1",
        "conan:Poco": "1.6.1@lasote/stable"
    },
    input: pfs.glob("./src", "*.c") + pfs.glob("./src", "*.cxx")
}
```

In this case, the actual target will change slightly. The packages will be downloaded before the configure step. If they contain a `build.it` file, that one will be run after all the packages are downloaded. Once that's done, we resolve the package names into target names. A target should tag itself with the package name that it was installed as.

```javascript
target("fs_c", "lib") {
    tag: "jwerle/fs.c",
    input: [...]
}
```

The tags are then added to the `needs` array.

```javascript
// It could become this:
var target = IceTea.__targets.example;
target.needs = target.needs || [];
target.needs += IceTea.tag("jwerle/fs.c");
target.needs += IceTea.tag("Poco");
```

However, the package is __not__ required to export it's target via tagging. Instead, it can also use the project-target architecture, and have a `package.it` file that could look like this:

```javascript
package("jwerle/fs.c") {
    provides: ["jwerle::fs.c"],
    dependencies: {
        // A package can depend on others too.
        // If this is found, then we re-run the process as often as needed
        // untill we have all packages.
        // HOWEVER: Due to limitations in C and C++, we can only have one
        // version of a package installed!
        // So if two packages want two incompatible dependencies, that wont work.
    }
}
```

In this case, we simply look up the package, see what it provides and add that into the target that wanted it!

But even if that is not provided, the user is the poor one that has to specify it himself... :). However, that might also be useful. Imagine you are having a package that provides two versions: One with backwards compatibility and one that breaks the ABI. Take for example FLTK. By defining the `FL_ABI_VERSION` macro, you can basically kill BC. So what you could do:

```javascript
target("app", "exe") {
    packages: {
        "git:fltk/fltk": "#master"
    },
    needs: [
        "FLTK@abi-1.4::FLTK"
    ]
}
```
Boom. Just like that, we will compile with an abi-breaking FLTK!

But what we might also could do is to allow a package to specify a callback to determine what it wants to add. Taking the example from above, imagine a setting:
```javascript
target("app", "exe") {
    packages: {
        "git:fltk/fltk": "#master"
    },
    settings: {
        FLTK: {
            useFutureABI: true
        }
    }
}
```
```javascript
// FLTK/package.it
package("fltk/fltk") {
    inject: function(target) {
        if("FLTK" in target.settings) {
            if(target.settings.FLTK.useFutureABI) {
                target.needs += [
                    "FLTK@abi-1.4::FLTK"
                ]
            }
        }
    }
}
```

And, yes, you can also specify multiple packages in one `package.it` file, if you published it under various names :).

## Compatibility to package.json
Simply add an `icetea` key to it, and attach the same settings as above. For instance:

```json
{
    "name": "fltk/fltk",
    "version": "1.3.3",
    "icetea": {
        "fltk/fltk": {
            "provides": [
                "FLTK::FLTK"
            ],
            "dependencies": {}
        }
    }
}
```

## Implementing a package manager
Now that I have talked about how they could look like, this is how I could imagine them being implemented. Once IceTea has built itself, it has mbedTLS and miniHTTP inside, allowing it to call Github APIs and other neat things.

So, take for instance you have a little self-hosted package manager...
```javascript
var MyPM = extends AbstractPackageManager {
    __construct: function() {
        // The first time it is found, its constructed.
    },
    install: function(name, version) {
        // Install the package. Feel free to re-call this method for sub-instals.
        // For instance, for installing dependencies of the dependency.
    },
    info: function(name, version) {
        return {
            // A path to a package.it file
            package_it: "...",
            // Information deserialized from a JSON
            json: {/*...*/},
            // ... to be extended ...
        }
    },
    isEverythingCompatible: function(deps) {
        // Return true, if it is safe to install all of these.
        // VERY. IMPORTANT. NOTE.
        // If you take NPM, it will have nested dependencies, since JavaScript is not so strict.
        // ObjectScript also won't mind.
        // Therefore, each package manager should decide individually if nesting is OK or not.
        // For instance, there will be two package managers:
        // - One for plain ObjectScript based stuff in icetea_modules
        // - Native dependencies in deps/.
        // Native dependencies cant be mixed - but scripted ones, can.
    }
}
PackageManager.add {
    name: "mypm",
    description: "My awesome package manager.",
    version: "0.0.1-dev"
    class: MyPM,
}
```

You can obtain additional package managers from existing ones. Once a package was downloaded, we immediately run it's `package.it`. That means, that we won't even know that the next one in the list might not have existed before!

```javascript
package("IngwiePhoenix/demo") {
    dependencies: {
        "git:icetea/npm.it": "#master",
        "npm:node-gyp": "..."
    }
}
```

## Package scripts
A package most commonly has scripts before, after or during its installing. So...
```javascript
package("IngwiePhoenix/demo") {
    provides: ["demo"],
    scripts: {
        // Before the installation...
        preInstall: function(){},
        // Replace the installation step. By default there is none. So add one if you want.
        install: function() {
            print "I was installed!"
        },
        // After installation
        postInstall: function(){},

        // You can also add custom scripts.
        // I.e.: icetea --pm deploy
        // Instead of running your usual building,
        // switch IceTea to PackageManager mode so it can
        // run your scripts.
        // Also, feel free to use the cli object!
        deploy: function() {
            // ...
        }
    },
    // Executed straight after the package file was loaded.
    load: function(){
        _G.demo = true;
    }
}
```

## Install stuff!
All the cool kids have it, but we currently don't. So! An install step.
- Install arbitary files
- Install headers, libraries, executables into a prefix

```javascript
target("mylib", "shlib") {
    input: [/* ... */],
    settings: {
        native: {
            reallocateable: true // -fPIC
        },
        install: {
            // Which headers?
            headers: pfs.glob("src/", "*.h"),
            // Or drop the headers all into a relative path.
            headers: {
                "mylib": pfs.glob("src/","*.h")
            },

            // Since this is a dynamic library, it'll be copied into .../lib
            // automatically. So we can set some stuff for that

            // Library name for OSX for instance
            libraryName: "mylib",
            // OS X: Install name
            installName: "@rpath/libmylib.dylib",
            // OS X: Version to be stanced into the file
            dylibVersion: "0.0.0"

            // More options might come.
            // The rule specified must have an install profile.
            // It is responsible for installing the library, at that point.
        }
    }
}
```
