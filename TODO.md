# ToDo: To make it better...

I have set up myself to make an awesome build tool. But as each project's nature, it has a todo list. Here it goes!

- [ ] Support creation and manipulation of files by mapping the oldschool `FILE* f*()` functions into the `os-pfs` module.
- [X] Finally introduce the usage of actions instead of building all targets.
- [ ] Implement sanity checks to make sure that an action/target/rule object is correctly entered.
- [ ] Introduce `external(name) {... opts ...}`
- [ ] Implement the detector module.
- [X] Implement an overall caching storage to make it possible for the app to store a cache for detector and alike.
- [ ] Somehow introduce console colors for all platforms.
- [ ] Introduce pre-inclusion (`require()`ing all files within a specified subfolder)
- [ ] Introduce addition to `require()` to automatically include a project. I.e.: `require.project "submodule/"`
- [X] Begin using the `prepare()` method for rules.
- [ ] Implement the `tag()` mechanism for usage inside targets/externals and the function to do a proper lookup.
- [X] Introduce the array `+` operator. I.e.: `var a=["foo"] + ["bar"]`. `o` should now contain: `["foo","bar"]`
- [X] Proper SHA2 implementation
- [X] Properly implement the lib+func checks (libdl on linux and what on Win32?)

### Added 19th Dec. 2014, 3.40AM
- [ ] Introduce dependency tracker
- [ ] Rewrite `void Run(void*)` for a better update tracker. It currently will happily rerun a task **multiple** times. It needs to learn to only run a task once, and only once.
- [ ] Add `+` operator for `Object`.
- [ ] Better error handling if OS actually crashed. Currently `int main(int,char**)` will report that no targets were built whilst it actually has an exception written ontop of it.
- [ ] Separate debug/non-debug builds-
- [ ] Better Windows linker support / flag passing / warning handling.
- [ ] Introduce a few OS specific settings so that tools can be optimized further.
- [ ] Implement a `build(Array)` method to allow scripted builds (i.e. from within a function, add tasks to the queue.)
- [ ] Make rules be able to depend upon targets also.
- [ ] Let targets finalize themselves (i.e. copy resulting binary to root folder - or link it at least)
