// This is the global IceTea object.
IceTea = extends _E {

    // The scheme (aka. action method) to run.
    scheme: "build",

    // The folder where output files should end up
    outputDir: __outputdir,

    // If tasks should be ran or not.
    runTasks: true,

    // The targets to build, clean or install.
    targetList: [],

    // Name of actions to handle.
    toExecute: [],

    // Steps
    __steps: [],
    addStep: function(inDef, outDef, stepOpts) {
        // Exists?
        for(var i=0; i<IceTea.__steps.length; i++) {
            var itStep = IceTea.__steps[i];
            if(itStep.outDef == outDef && @{
                for(var _,def in inDef) {
                    if(!(def in itStep.inDef)) {
                        return false;
                    }
                }
                return true;
            }) {
                var itName = itStep.name;
                var optName = stepOpts.name;
                throw "Two steps can not share the same input and output definitions. Small variations are allowed."
            }
        }
        // Add it.
        var step = IceTea.Step(inDef, outDef, stepOpts);
        IceTea.__steps.push(step);
    },

    // Rules
    __rules: {},
    addRule: function(name, display, ruleOpts) {
        if(name in IceTea.__rules) {
            debug "Overwriting previously defined rule ${name}";
        }
        IceTea.__rules[name] = IceTea.Rule(name, display, ruleOpts);
    },

    // Targets
    __targets: {},
    addTarget: function(name, ruleObj, targetOpts) {
        if(name in IceTea.__targets) {
            print "WARNING: Target ${name} is already defined."
        }
        IceTea.__targets[name] = IceTea.Target(name, ruleObj, targetOpts);
    },

    // Externals
    __externals: {},
    addExternal: function(name, opts) {
        if(name in IceTea.__externals) {
            print "WARNING: Overriding external ${name}."
        }
        IceTea.__externals[name] = IceTea.External(name, opts);
    },

    // Return targets by tag. Supports wildcards.
    tag: function(t) {
        if(typeOf(t) == "null") {
            return IceTea.__targets.keys;
        } else {
            var res = [];
            for(name,target in IceTea.__targets) {
                if("tag" in target && wildcard.match(target.tag, t)) {
                    res.push(name);
                } else if(wildcard.match(name, t)) {
                    res.push(name);
                }
            }
            return res;
        }
    },

    // Actions
    __actions = {},
    addAction: function(name, actionDef) {
        if(name in IceTea.__actions) {
            print "WARNING: Overwriting previously defined action \"${n}\"";
        }
        IceTea.__actions[name] = IceTea.Action(name, actionDef);
    },

    /**
     * Task queue.
     *
     * The task queue is a top-sorted structure, laid out into multiple levels.
     * Each level contains one stage of a task, untill it is completed.
     *
     * If target A has three stages to be complete, then each level will have one stage. And, one task may depend
     * on many tasks from a previous stage.
     *
     * For instance:
     *
         [
             [
                 // Level 1
                 {
                     input: "a.cpp",
                     output: "a.o",
                     built: false,
                     next: IceTea.Task,
                     previous: null
                 }
             ], [
                 // Level 2
                 {
                     input: "a.o",
                     output: "a",
                     built: false,
                     previous: { ... a.cpp ... },
                     next: { ... }
                 }
             ]
         ]
     *
     * Each time a task in a higher level is resolved, it's child/ren is/are checked,
     * to see if they were built yet. If not, they're skipped for the time being.
     */
    __tasks = [],
    addTask: function(level, task) {
        // Create level if it does not exist yet.
        IceTea.__tasks[level] = IceTea.__tasks[level] || [];
        IceTea.__tasks[level].push(task);
    },

    // Returns the currently highest level.
    // Add +1 to create a new level.
    getLevel: function() {
        return IceTea.__tasks.length-1;
    },

    // Get a count of the tasks.
    getTaskCount: function() {
        var count = 0;
        for(var level,tasks in IceTea.__tasks) {
            for(var _,task in tasks) {
                count++;
            }
        }
        return count;
    },

    /**
     * Create the steps required to build.
     * Each step is a Task. A step can be of various types.
     *
     * @see IceTea.Task
     *
     * @param {Object} target Target from/for which to make steps.
     * @param {Number} level  Starting level. Useful when post-adding tasks.
     * @return {Boolean} True on success, false on failure.
     */
    createSteps: function(target, level) {
        var finalDeps = [];
        level = level || 0;

        debug "Level: ${level}"

        // Are we dealing with a raw rule?
        if("options" in target.rule && "raw" in target.rule.options && target.rule.options.raw) {
            debug "Assigning ${target.name} with ${taret.rule.display}"
            IceTea.addTask(level, IceTea.Task({
                type: "rule",
                target: target
            }));
        }

        // Nope. So process the steps.
        for(var _,file in target.input) {
            debug "Processing: ${file}"
            // Find a step for this file.
            var currentFile = file;
            var lastTask = null;
            var levelBase = level;
            while(typeOf(currentFile) != "null") {
                var stepFound = false;
                // Match a file against the steps. If one hits, add it.
                debug "Testing steps..."
                for(var _,step in IceTea.__steps) {
                    debug "Matching: ${step.inDef} with ${currentFile}"
                    if(step.accepts(currentFile)) {
                        // Initialize the step, if it hasn't already.
                        step.init();

                        // Build a task object, assign it to the corresponding nodes in the tree.
                        debug "Creating task: ${file}"
                        var t = IceTea.Task({
                            type: "step",
                            step: step,
                            input: currentFile,
                            previous: lastTask,
                            target: target
                        });

                        // If a previous task is defined, the current one is the previous' next.
                        if(lastTask != null) {
                            lastTask.next = t;
                        }

                        // Add the task to the current level.
                        IceTea.addTask(levelBase++, t);
                        currentFile = t.out;
                        stepFound = true;
                        lastTask = t;
                        break;
                    }
                }

                if(!stepFound) {
                    if(lastTask == null || typeOf(lastTask) == "null") {
                        debug "There does not seem to be a previous task for ${currentFile}. Is a step missing?"
                    }

                    // No step found. Consider it a final dependency.
                    finalDeps.push(lastTask);

                    // There is no next step to perform. So, short-circuit the loop.
                    currentFile = null;
                }
            }
        }
        debug "Final deps: ${finalDeps}"

        // Now generate the highest-level rule task.
        debug "Finalizing on: ${target.name}"
        var finalTask = IceTea.Task({
            type: "rule",
            target: target,
            deps: finalDeps,
            input: @{
                var outs = [];
                for(var _,dep in finalDeps) {
                    if(dep.out == "null" || typeOf(dep.out) == "null") {
                        throw "${dep}'s output is NULL!"
                    } else {
                        outs.push(dep.out);
                    }
                }
                return outs;
            }
        });
        IceTea.addTask(IceTea.getLevel()+1, finalTask);

        // All the previous tasks should point to the next task now.
        for(var _,dep in finalDeps) {
            dep.next = finalTask;
        }

        // Success.
        return true;
    },

    /** @section The task processors
       There are a few of them:
            - Initializer: If it exists, call init() on each target. Ommit this, if --help is used.
                * the init() function can add entries to the help screen. Important for configuration.
                * Check if rules that targets refer to, exist.

            - Preprocessors: Run configure on each object.
                * configure() has access to the config object, command line params (parsed) and can also abort.

            - Transformers: Create the list of tasks.
                * Create a list of all the targets that are to be built.
                * Iterate over each of their input files.
                * Trace down the rules for each individual file and call prepare().
                * Create a new task with the file and the rule information.
                * Create the output variable, which means creating the base folders needed (out/,...)
                * Determine if its a scripted, or command controlled task.
                * If its a scripted task, save a Value::Method, otherwise a list<string> of commands.
                * Save info to a task list, WorkQueue.

            - Runner: Build executor
                * Create an array with the maximum size of CPU cores available - or the amount of -j.
                * Pick up an item from the bottom of the task list and decide if we can run it or not.
                * If we can not run it, put it back to the top of the stack.
                * If we can run it, either spawn an async subprocess or run the scripted function.
                    - The scripted function may use $() or shell().
                * If it is a command, check on the array regulary to
    */

    /**
        @brief Initializes all targets.
        @returns If the iteration was successful.
        @note This should actually change depending on specified actions...
    */
    Initializer: function() {
        for(name,target in IceTea.__targets) {
            if(typeOf(target.init) == "function") {
                var rt = target.init();
                // Returning false halts the initialization, thus cancels anything else.
                if(typeOf(rt) == "boolean" && !rt) return rt;
            }
        }
        // Re-parse the CLI.
        cli.parse();
    },

    /**
        @brief Configure all targets and resolve dependencies

        This function calls the targets configure method in order to make them
        ready for actual compilation/building.

        It also looks for an existing `need` property and if it exists, grabs the
        other target's configure methods and runs them first. It then combines
        the `exports` property with the local `settings` property, if it exists.

        For a configuration to fail explicitly, it must use the abort/exit methods or return false.
    */
    Preprocessor: function(targetNames) {
        // Determine how to build.
        targetNames = targetNames || IceTea.targetList;
        // Recursively configure a target and it's needs.
        var configureTarget = function(targetName, dependsOn) {
            if(!(targetName in IceTea.__targets)) {
                if(typeOf(dependsOn) != "null") {
                    throw "Target ${targetName}, depended on by ${dependsOn}, does not exist.";
                } else {
                    throw "Target ${targetName} does not exist.";
                }
            }

            var target = IceTea.__targets[targetName];
            var configured = [];

            var needs = "needs" in target ? target.needs : [];
            debug "Are we configuring dependencies? ${needs} (${target})"
            if(typeOf(needs) == "array") {
                for(var _,depName in needs) {
                    debug "Configuring dependency: ${depName} of ${targetName}"
                    var rt, depConfigured = configureTarget(depName, targetName);
                    configured += depConfigured;

                    // If this retuend false, return false.
                    if(typeOf(rt) == "boolean" && !rt) return rt, depConfigured;
                }
            }

            // Configure the target
            if(!target.isConfigured()) {
                var rt2 = target.configure();
                if(typeOf(rt2) == "boolean" && !rt2) {
                    return rt2, configured;
                } else {
                    configured.push(target);
                }
            }

            // And the rule.
            if(target.rule.isConfigured()) {
                var rt3 = target.rule.configure(target);
                if(typeOf(rt3) == "boolean" && !rt3) {
                    return rt2, configured;
                }
            }

            if(typeOf(dependsOn) == "string") {
                // We are being a child, so we should export ourself into parent.
                var dependant = IceTea.__targets[dependsOn];
                if(
                    !dependant.hasMergedWith(targetName)
                    && "exports" in target
                    && "settings" in dependant
                ) {
                    debug "Merging exports from ${target.name} into settings of ${dependsOn}"
                    debug dependant.settings
                    debug target.exports
                    dependant.settings = dependant.settings + target.exports;
                    dependant.setHasMergedWith(targetName);
                }
                debug "Done merging."
            }
            debug "Done configuring."
            return true, configured;
        }

        for(_,tname in targetNames) {
            // Time for some recursion!
            var rt, configured = configureTarget(tname);
        }
        IceTea.targetListResolv = configured;
        return true;
    },

    /**
        @brief Turn targets into a series of tasks

        This function is mainly responsible for turning targets into tasks. It
        takes note of the specified actions, or uses `all` as a default action.

        `all` __must__ exist. It can not be omitted. Only if the user always
        specifies a different task, thent his will not be required.

        @see Preprocess
        @see Initializer
    */
    Transformer: function(targetNames) {
        targetNames = targetNames || IceTea.targetList;
        debug "Transformer: ${IceTea.targetList}"
        for(var _,targetName in targetNames) {
            var target = IceTea.__targets[targetName];
            debug "Transforming ${targetName}";
            IceTea.createSteps(target);
        }
    },

    Runner: function(){
        $.Cursor.hide();
        $.saveDefaultColor();
        var longOutput = cli.check("--debug") || cli.check("--detail-output");
        // function globals.
        var maxJobs = toNumber(cli["-j"]);
        var taskCount = IceTea.getTaskCount();
        var currCount = 0;
        for(var level, tasks in IceTea.__tasks) {
            debug "Entering level: ${level}"
            var jobs = [];
            var taskId = 0;
            while(true) {
                // Ensure that we have always as many jobs as possible in the job queue.
                if(#jobs < maxJobs) {
                    // Accept new jobs, till we cap, if possible.
                    while(#jobs != maxJobs && typeOf(tasks[taskId]) != "null") {
                        debug "Accepting new tasks now. taskId: ${taskId}"
                        var task = tasks[taskId++];
                        if(typeOf(task.previous) != "null") {
                            debug "Testing if dependant target was or was not built yet."
                            if(!task.previous.built) {
                                // Skip this task and come back here later.
                                continue;
                            }
                        }

                        // If this job was built, skip it.
                        if(task.built == true) {
                            continue;
                        }

                        debug "Running job ${level}.${taskId}"
                        var buildStr = typeOf(task.in) == "array" ? task.out : task.in;
                        var progStr = "${++currCount}/${taskCount}";
                        var targetStr = task.target.title;
                        var stepStr = task.backend.display;
                        var baseStr = "[${progStr}] ${targetStr}(${stepStr}): ${buildStr}";
                        if(cli.check("--no-color")) {
                            if(!longOutput && #baseStr <= $.cols) {
                                process.stdout.write("\r");
                                // Clear the line.
                                for(var i=0; i<$.cols-2; i++) {
                                    process.stdout.write(" ");
                                }
                                process.stdout.write("\r");
                            } else if(#baseStr > $.cols) {
                                process.stdout.write("\n");
                            }
                            process.stdout.write(baseStr);
                            if(longOutput || #baseStr > $.cols) {
                                process.stdout.write("\n");
                            }
                        } else {
                            if(!longOutput && #baseStr <= $.cols) {
                                process.stdout.write("\r");
                                // Clear the line.
                                for(var i=0; i<$.cols-2; i++) {
                                    process.stdout.write(" ");
                                }
                                process.stdout.write("\r");
                            } else if(#baseStr > $.cols) {
                                process.stdout.write("\n");
                            }
                            // Colorize
                            $.setColor($.Colors.CYAN);
                            process.stdout.write("[${progStr}] ");
                            $.resetColor();
                            process.stdout.write("${targetStr}(");
                            $.setColor($.Colors.MAGENTA);
                            process.stdout.write(stepStr);
                            $.resetColor();
                            process.stdout.write("): ${buildStr}");
                            if(longOutput || #baseStr > $.cols) {
                                process.stdout.write("\n");
                            }
                        }
                        var immediate = task.run();

                        // Did the job possibly fail?
                        if(task.status() == IceTea.Task.Status.FAIL) {
                            // The task arleady failed. Print an error and break.
                            // Also print the error message.
                            // Note, that on compact output, we want to write a new line before, and after.
                            debug "Job failed immediately.";
                            $.Cursor.show(); // Reset.
                            return task.exitCode();
                        } else if(task.status() == IceTea.Task.Status.OK) {
                            debug "Job was built successfuly."
                            task.built = true;
                        }

                        if(!immediate) {
                            debug "Delayed/async job added to queue."
                            jobs.push(task);
                        } else {
                            debug "Job was immediate."
                        }
                    }

                    if(#tasks <= taskId) {
                        debug "Capped task count (${#tasks}). Resetting taskId."
                        taskId = 0;
                    } else {
                        debug "Cap not reached."
                    }
                }

                // Test for the jobs.
                var newJobs = [];
                var exitNow = false;
                for(var _,job in jobs) {
                    var status = job.test();
                    var _Status = IceTea.Task.Status;
                    switch(status) {
                        case _Status.OK:
                            // This task is done. mark it as built.
                            job.built = true;
                        break;
                        case _Status.FAIL:
                            // This task actually failed. Break out and stop.
                            var exCode = job.exitCode();
                            return exCode;
                        break;
                        case _Status.PENDING:
                            // Still pending...
                            newJobs.push(job);
                        break;
                    }
                }

                // Overwrite the new jobs with the old ones.
                jobs = newJobs;

                if(#jobs == 0) {
                    debug "Leaving level: ${level}"
                    break;
                } else {
                    debug "Going: ${#jobs}"
                }
            }
        }
        debug "Reached end of control."
        if(!longOutput) process.stdout.write("\n");
        $.Cursor.show();
    },

    Main: function() {
        var rt;
        IceTea.CallEvent("beforeRun");

        // Execute the actions.
        if(cli.check("--target")) {
            IceTea.targetList = cli["--target"].split(",");
        } else {
            for(var _,name in IceTea.toExecute) {
                if(!(name in IceTea.__actions)) {
                    throw "Action <${name}> does not exist!";
                }
                var action = IceTea.__actions[name];
                var list = action.execute(null);
                if(typeOf(list) == "array") {
                    // We got a list of target names.
                    IceTea.targetList += list;
                } else if(typeOf(list) == "number") {
                    // The action returned an exit code.
                    return list;
                }
            }
        }

        if(#IceTea.targetList == 0) {
            // If empty, we can assume that the action did something on it's own.
            return 0;
        }

        // Preprocess all the things.
        rt = IceTea.Preprocessor();
        if(typeOf(rt) == "boolean" && rt == false) return 1;

        // Then transform them.
        rt = IceTea.Transformer();
        if(typeOf(rt) == "boolean" && rt == false) return 1;

        /*
        // Example printing.
        var taskCount = IceTea.getTaskCount();
        var localCount = 1;
        for(var level,tasks in IceTea.__tasks) {
            for(var _,task in tasks) {
                var inStr;
                if(typeOf(task.in) == "array") {
                    inStr = task.in.join("; ");
                } else {
                    inStr = task.in;
                }
                var progStr = "${localCount++}/${taskCount}";
                var targetStr = task.target.name;
                var stepStr = task.backend.display;
                var outStr = task.out;
                print "[${progStr}] ${targetStr}(${stepStr}): ${inStr} -> ${outStr}"
            }
        }
        */

       rt = IceTea.Runner();
       return rt;
    },

    // Does common configuration stuff.
    Configurer: function(obj) {
        debug "Configuring..."
        var o = obj.__getStore();
        if("configure" in o && typeOf(o.configure) == "function") {
            var rt = o.configure.call(obj);
            obj.__isConfigured = true;
            return rt;
        }
        return true;
    },

    events: {
        beforeInitialize: function() {
            var fullPath = pfs.join(__sourcedir, ".IceTea");
            if(pfs.isPresent(fullPath)) {
                require.paths.push(fullPath);
            }

            var moduleFolder = "icetea_modules";
            var rootDir = pfs.join(sys.dirSep, "..");
            var current = __sourcedir;
            var rootHit = false;
            while(current != rootDir && !rootHit) {
                if(current == rootDir) {
                    // If we're processing root, make sure to check this.
                    rootHit = true;
                }
                var dir = pfs.join(current, moduleFolder);
                require.paths.push(dir);
                // Navigate one upwards
                current = pfs.join(current, "..");
            }
        },
        afterInitialize: function(){
            var fullPath = pfs.join(__sourcedir, ".IceTea");
            if(pfs.isPresent(fullPath)) {
                var incFiles = pfs.glob(fullPath, "*.it");
                for(var i,file in incFiles) {
                    require(file); // it should work on .it files...
                }
            }
        },
        postprocessCli: function() {
            // Determine run scheme
            if(cli.check("-w")) {
                @scheme = "clean";
            } else if(cli.check("-i")) {
                @scheme = "install";
            } else if(cli.check("--scheme")) {
                @scheme = cli["--scheme"];
            } else {
                @scheme = "build";
            }

            if(cli.check("--print-targets")) {
                // dump something...
                print json.encode(@__targets);
                return false;
            } else if(cli.check("--print-actions")) {
                print json.encode(@__actions);
                return false;
            } else if(cli.check("--print-rules")) {
                print json.encode(@__rules);
                return false;
            }

            var actNames = [];
            if(#cli.stray == 0) {
                // No stray args. Assume "all".
                actNames.push("all");
            } else {
                actNames = cli.stray;
            }
            @toExecute = actNames;
            return true;
        },
        beforeRun: function() {
            // Has someone made the all target, yet?
            if(!("all" in @__actions)) {
                @addAction("all",{
                    build: IceTea.tag(),
                    clean: IceTea.tag(),
                    // Might not be optimal to have this by default.
                    // install: IceTea.tag()
                });
            }
        },
        afterRun: function() {},
        onShutdown: function() {}
    },
    // Simple wrapper for C++.
    CallEvent: function(e) { IceTea.events[e].call(IceTea); }
}

IceTea.RuleException = extends Exception {
    __construct = function(prefix, msg) {
        return super(prefix+" :: "+msg);
    }
};

// This interconnects a user object and class.
IceTea.Storeable = extends Object {
    __setStore: function(store) {
        this.__store = store;
    },
    __get: function(name) {
        if(name in this.__store) {
            debug "Storeable::__get(${name}) -> this.__store"
            return this.__store[name];
        } else if(name in this) {
            debug "Storeable::__get(${name}) -> this"
            return this[name];
        } else {
            throw "${this.__name} does not have property ${name}."
        }
    },
    __getStore: function() {
        return this.__store;
    },
    __isset: function(name) {
        if(name in this.__store.keys) {
            debug "Storeable::__isset(${name}) -> this.__store"
            return true;
        } else if(name in this.keys) {
            debug "Storeable::__isset(${name}) -> this"
            return true;
        } else {
            return false;
        }
    }
}

IceTea.Step = extends IceTea.Storeable {
    __construct: function(inDef, outDef, opt) {
        this.__setStore(opt);
        if(typeOf(inDef) == "string") {
            this.inDef = [inDef];
        } else if(typeOf(inDef) == "array") {
            this.inDef = inDef;
        } else {
            throw "A step's input definition must be an array or string.";
        }
        if(typeOf(outDef) != "string") {
            throw "A step's output definition must be a string.";
        } else {
            this.outDef = outDef;
        }
        if(typeOf(opt.pattern) != "string") {
            throw "A step must define an output pattern."
        } else {
            this.pattern = opt.pattern;
        }
        this.__isConfigured = false;
    },

    // Prettyness.
    __get@display: function() {
        return @name;
    },

    // Configuration stuff.
    isConfigured: function() {
        return this.__isConfigured;
    },
    configure: function() {
        return IceTea.Configurer(this);
    },

    // determine if an input matches with the step.
    accepts: function(file) {
        for(var _,pattern in this.inDef) {
            if(wildcard.match(pattern, file)) {
                return true;
            }
        }
        return false;
    },

    estimate: function(target, file) {
        return @pattern
            .replace("%t", target.name)
            .replace("%f", file)
            .replace("%e", pfs.extname(file))
            .replace("%b", pfs.basename(file))
            .replace("%d", pfs.dirname(file))
            .replace("%o", __outputdir);
    },

    init: function() {
        if(!@isConfigured()) {
            var title = @name || @outDef;
            debug "Configuring Step: ${title} - ${@configure}";
            @configure();
            @_isConfigured = true;
        }
    }
}

IceTea.Rule = extends IceTea.Storeable {
    __construct: function(name, displayName, o) {
        @__setStore(o);
        @name = name;
        @display = displayName;
        @__isConfigured = false;
        if(!("pattern" in o) || typeOf(o.pattern) != "string") {
            throw "A rule must define a pattern."
        }
        @pattern = o.pattern;
        @verify();
    },
    verify: function() {
        var o = @__getStore();
        var schemes = ["build", "clean"];
        for(var _,scheme in schemes) {
            if(!(scheme in o)) {
                throw IceTea.RuleException(@display,"A rule is required to define a method of name: ${scheme}");
            } else if(typeOf(o[scheme]) != "function") {
                throw IceTea.RuleException(@display,"A rule's scheme property must be a function.");
            }
        }
    },
    isConfigured: function() {
        return this.__isConfigured;
    },
    configure: function() {
        return IceTea.Configurer(this);
    },
    estimate: function(target, input) {
        var o = this.__getStore();
        if("estimate" in o && typeOf(o.estimate) == "function") {
            o.estimate.call(this, target, file);
        } else {
            return @pattern
                .replace("%t", target.name)
                .replace("%r", @name)
                .replace("%o", __outputdir);
        }
    }
}

IceTea.Target = extends IceTea.Storeable {
    __construct: function(name, rule, opts) {
        debug "Creating Target ${name} with rule: ${rule}"
        @__setStore(opts);
        @name = name;
        @rule = rule;
        // Sanity checks
        var type = typeOf(opts.input);
        if(typeOf(opts.input) != "array" && rule.validate(opts.input)) {
            throw "${name}'s input are of illegal type: ${type}.";
        }
        if("settings" in opts) {
            @settings = opts.settings;
        } else {
            @settings = {};
        }
        @__isConfigured = false;
        @__mergedWith = [];
    },
    __get@title: function() {
        var o = @__getStore();
        if("title" in o) {
            return o.title;
        } else {
            return @name;
        }
    },
    isConfigured: function() {
        return this.__isConfigured;
    },
    configure: function() {
        return IceTea.Configurer(this);
    },
    init: function() {
        var o = this.__getStore();
        if("init" in o && typeOf(o.init) == "function") {
            o.init.apply(this);
        }
    },
    setHasMergedWith: function(name) {
        @__mergedWith.push(name);
    },
    hasMergedWith: function(name) {
        return (name in @__mergedWith);
    }
};

IceTea.Task = extends Object {
    Type = extends _E {
        RULE    : "rule",
        STEP    : "step",
        EXTERN  : "external"
    },

    Status = extends _E {
        OK      : 1,
        PENDING : 2,
        FAIL    : 3
    },

    __construct: function(opts) {
        if(typeOf(opts.target) == "null") {
            throw "A step requires a reference to a target.";
        }
        if(typeOf(opts.input) == "null") {
            throw "A step requires input. Output is estimated.";
        }

        // determine backend
        switch(opts.type) {
            case @Type.RULE:
                this.backend = opts.target.rule;
            break;
            case @Type.STEP:
                this.backend = opts.step;
            break;
            case @Type.EXTERN:
                throw "Not implemented yet.";
            default:
                throw "Unsupported task type.";
        }

        this.type = opts.type;
        this.target = opts.target;
        this.in = opts.input;
        this.out = this.backend.estimate(@target, @in);
    },

    __get: function(name) {
        if(name in this.keys) {
            debug "Step::__get(${name}) -> this"
            return this[name];
        } else if(name in this.backend.keys) {
            debug "Step::__get(${name}) -> this.backend"
            return this.backend[name];
        } else if(name in this.target.keys) {
            debug "Step::__get(${name}) -> this.target"
            return this.target[name];
        } else {
            debug "Step::__get(${name}) -> Void"
            return null;
        }
    },

    // Get the status of the task.
    _setStatus: function(rt) {
        if(typeOf(rt) == "boolean") {
            // The function returned immediately.
            this._status = rt ? @Status.OK : @Status.FAIL;
            return true; // Immediate return
        } else {
            this._status = @Status.PENDING;
            return false; // Delayed return
        }
    },
    status: function() {
        return this._status;
    },

    // Run this task.
    run: function() {
        try {
            var rt = this.backend[IceTea.scheme].call(this);
            return this._setStatus(rt);
        } catch(e) {
            unhandledException(e)
            this._status = @Status.FAIL;
            return true;
        }
    },

    // If the status is pending, then this tests if things are resolved.
    // This function should return one of IceTea.Task.Type
    test: function() {
        if(this._status == @Status.PENDING) {
            try {
                var rt = this.backend.status.call(this);
                return rt;
            } catch(e) {
                print "CRITICAL: Error while testing.";
                return @Status.FAIL;
            }
        } else {
            throw "This task is not pending.";
        }
    },

    exitCode: function() {
        // FIXME: Actually implement this.
        return 0;
    }
}

IceTea.Action = extends Object {
    __construct: function(name, body) {
        this.name = name;
        if(typeOf(body) == "function") {
            this._run = body;
        } else if(typeOf(body) == "object") {
            for(var key, value in body) {
                this[key] = value;
            }
        } else {
            throw "Body must be either a function, or object.";
        }
    },
    execute: function(args) {
        if("_run" in this && typeOf(this._run) == "function") {
            var toBuild = [];
            var toClean = [];
            var toInstall = [];
            var to = {
                build: [],
                clean: [],
                install: []
            }
            var env = extends _E {
                build: function(list) {
                    to.build += list;
                },
                clean: function(list) {
                    to.clean += list;
                },
                install: function(list) {
                    to.install += list;
                },
                custom: function(scheme, list) {
                    to[scheme] = to[scheme] || [];
                    to[scheme] += list;
                }
            }
            this._run.applyEnv(env, this, args);
            return to[IceTea.scheme];
        } else {
            // Get the target list:
            var list = this[IceTea.scheme];
            // Let IceTea handle it.
            return list;
        }
    }
}

// Configurables
configurable("step", function(head, body){
    var inDef, outDef = head.unpack();
    IceTea.addStep(inDef, outDef, body);
});
configurable("rule", function(head, body){
    var name,display = head.unpack();
    IceTea.addRule(name, display, body);
});
configurable("target", function(head, body){
    var name, ruleName = head.unpack();
    if(!(ruleName in IceTea.__rules)) {
        throw "Rule <${ruleName}> (requesed by target <${name}>) is unknown.";
    }
    var rule = IceTea.__rules[ruleName];
    IceTea.addTarget(name, rule, body);
});
configurable("external", function(head, body){
    var name = head[0];
    IceTea.addExternal(name, body);
});
configurable("action", function(head, body){
    var name = head[0];
    IceTea.addAction(name, body);
});
