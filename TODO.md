# ToDo: To make it better...

I have set up myself to make an awesome build tool. But as each project's nature, it has a todo list. Here it goes!

- [ ] Support creation and manipulation of files by mapping the oldschool `FILE* f*()` functions into the `os-pfs` module.
- [ ] Finally introduce the usage of actions instead of building all targets.
- [ ] Implement sanity checks to make sure that an action/target/rule object is correctly entered.
- [ ] Introduce `external(name) {... opts ...}`
- [ ] Implement the detector module.
- [ ] Implement an overall caching storage to make it possible for the app to store a cache for detector and alike.
- [ ] Somehow introduce console colors for all platforms.
- [ ] Introduce pre-inclusion (`require()`ing all files within a specified subfolder)
- [ ] Introduce addition to `require()` to automatically include a project. I.e.: `require.project "submodule/"`
- [ ] Begin using the `prepare()` method for rules.
- [ ] Implement the `tag()` mechanism for usage inside targets/externals and the function to do a proper lookup.
- [ ] Introduce the array `+` operator. I.e.: `var a=["foo"] + ["bar"]`. `o` should now contain: `["foo","bar"]`
