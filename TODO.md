# ToDo: To make it better...

I have set up myself to make an awesome build tool. But as each project's nature, it has a todo list. Here it goes!

- [ ] Support creation and manipulation of files by mapping the oldschool `FILE* f*()` functions into the `os-pfs` module.
- [X] Finally introduce the usage of actions instead of building all targets.
- [X] Implement sanity checks to make sure that an action/target/rule object is correctly entered.
- [-] Introduce `external(name) {... opts ...}`
- [X] Implement the detector module.
- [X] Implement an overall caching storage to make it possible for the app to store a cache for detector and alike.
- [X] Somehow introduce console colors for all platforms.
- [X] Introduce pre-inclusion (`require()`ing all files within a specified subfolder)
- [ ] Introduce addition to `require()` to automatically include a project. I.e.: `require.project "submodule/"`
- [X] Begin using the `prepare()` method for rules.
- [X] Implement the `tag()` mechanism for usage inside targets/externals and the function to do a proper lookup.
- [X] Introduce the array `+` operator. I.e.: `var a=["foo"] + ["bar"]`. `o` should now contain: `["foo","bar"]`
- [X] Proper SHA2 implementation
- [X] Properly implement the lib+func checks (libdl on linux and what on Win32?)

### Added 19th Dec. 2014, 3.40AM
- [X] Introduce dependency tracker
- [X] Rewrite `void Run(void*)` for a better update tracker. It currently will happily rerun a task **multiple** times. It needs to learn to only run a task once, and only once.
- [X] Add `+` operator for `Object`.
- [X] Better error handling if OS actually crashed. Currently `int main(int,char**)` will report that no targets were built whilst it actually has an exception written ontop of it.
- [ ] Separate debug/non-debug builds
- [X] Better Windows linker support / flag passing / warning handling.
- [ ] Implement a `build(Array)` method to allow scripted builds (i.e. from within a function, add tasks to the queue.)
- [ ] Make rules able to depend upon targets also.
- [ ] Let targets finalize themselves (i.e. copy resulting binary to root folder - or link it at least)

### Added 25th Dec. 2014, 1.27PM
- [X] Introduce a proper dependency tracking. Resolve target outputs and add them to the dependencies.
    * May make it recursive.
- [ ] Finally write `detect.write_*()` functions.
    - [X] `detect.write_header()`
    - [ ] `detect.write_json()`
- [X] Proper build flags for the `IceTea` target
- [ ] Introduce an `install` kind of thing.
- [ ] Teach `bootstrap.it` these rules:
    * ASM: Assembly
    * C#: CSharp
    * Framework: OS X Framework
    * Component: A generic module
    * Concat: Combine inputs into single output
    * Shared libraries
    * Static libraries
- [X] Implement these settings: [Rules now use the settings, so they are userland-defined.]
    * native.framework_dirs: OBJC, OBJCXX
    * OBJC/XX.GC/ARC: Settings for garbage collection or ARC
    * LIB.rpath: Search paths and install name
    * COMPONENT: target binary, other settings
    * ASM: Defines and such
- [X] Finally take `bootstrap.it` into memory within a post-build
    - Using the `incbin` utility.

### Added 20th Februrary 2015, 10.21AM
- [X] Rewrite build executor (Thanks to @martine :))
    * First generate all the tasks, then loop over them to execute them.
    * Wait on multiple tasks (commands) and check for their status.
        * If failed, let the other commands finish first, then exit.
    - Use `stlplus::async_subprocess`.
    - Create a subclass that reports `bool done()`.
- [-] Make `settings.LINK.libraries` unique (i.e.: only one `-lmylib` per array)

### Added 20th November 2015, 11:07PM
- [X] Implement a Topsort mechanism
    * Luckily, advice was retrived here: http://stackoverflow.com/a/30249477/2423150
- [ ] Possibly generate a graph.
- [ ] Maybe introduce RegEx support.

### Added 10th Janurary 2016, 6:42AM
- [ ] Re-implement a `describe/it` style testing framework as seen in:
    - https://github.com/stephenmathieson/describe.h/blob/master/describe.h

### Added 12th Janurary 2016, 5.47AM
- [ ] Make sure that files within a target actually exist.

### Added 13th Janurary 2016, 4.20PM
- [ ] Add a rule - and options - that allow it to be "not run".
    - Makes it possible for targets to depend on header-only libs.
    - Get extra settings in or trigger other configure steps.
- [ ] Build the same target for multiple rules. I.e. as shared and static library.
- [ ] Write proper process classes. `$()` is nice, but more complex classes are needed.
    - `Command.Async` : Runs a command and watches it in a separate thread.
    - `Command.Sync`  : A command that is constructed and ran immediately. The backend for normal `$()` calls.
    - `shell` will not be affected by this. It uses an entirely different routine anyway.
- [ ] Rewrite parts of `detect` in OS itself to simplify source.
- [ ] Store info about input files' last-changed date. On subsequent runs, filter these, and manipulate the queue accordingly.
