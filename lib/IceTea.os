// This is the global IceTea object.
IceTea = extends _E {

    // The scheme (aka. action method) to run.
    scheme: "build",

    // The folder where output files should end up
    outputDir: __outputdir,

    // global settings.
    GlobalSettings: @{
        var st = {};
        // FIXME: We likely need to ad this at the bottom of detect.os
        var map = [
            "ASM",
            "CC", "CXX",
            "OBJC", "OBJCXX",
            "SWIFTC", "DC", "CSC",
            "GOC", "RUSTC", "JAVAC"
        ];
        for(var kind,name in map) {
            st[kind] = {};
        }
        st.$PATH = [];
    },

    // Should we traverse Win32 stuff?
    _win32: false,
    searchWin32: function(should) {
        if(__.isBool(should)) {
            @_win32 = should;
        } else {
            return @_win32;
        }
    },

    // If tasks should be ran or not.
    runTasks: true,

    // When building, we want to keep track of things.
    FileCache: @{ return Cache("Files"); },

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
    findStepForFile: function(file) {
        for(var _,step in IceTea.__steps) {
            if(step.accepts(file)) {
                step.init();
                return step;
            }
        }
        return false;
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

    // Display the steps in a container.
    dumpSteps: function(container) {
        for(var level, tasks in container) {
            print "Level: ${level}";
            for(var i,task in tasks) {
                print "  - ${task.backend.display}: ${task.in} -> ${task.out}"
            }
        }
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
    get@__tasks: function() {
        throw "IceTea.__tasks is deprecated. Use a taskContainer instead!";
    },
    addTask: function(level, task, taskContainer) {
        // Create level if it does not exist yet.
        taskContainer[level] = taskContainer[level] || [];
        taskContainer[level].push(task);
    },

    // Returns the currently highest level.
    // Add +1 to create a new level.
    getLevel: function(taskContainer) {
        taskContainer = taskContainer;
        return taskContainer.length-1;
    },

    // Get a count of the tasks.
    getTaskCount: function(taskContainer) {
        taskContainer = taskContainer;
        var count = 0;
        for(var level,tasks in taskContainer) {
            for(var _,task in tasks) {
                if(task.isHidden()) continue;
                else                count++;
            }
        }
        return count;
    },

    // We can optimize task containers,
    // by putting all IceTea.Rule objects
    // into the same level, for instance.
    // this is basically what we'll attempt to do, for now.
    // FIXME: This is rather impossible, when considering, that two rules may produce dependencies for one another.
    // Awh man... FIXME, come back here later.
    optimizeTaskContainer: function(taskContainer) {
        var newTaskContainer = [];
        var xLevel = 0;
        var ruleStack = [];
        var T = IceTea.Task.Type;
        for(var level,tasks in taskContainer) {
            var allAreRules = true;
            for(var id,task in tasks) {
                if(task.type != T.RULE) {
                    allAreRules = false;
                    break;
                }
            }
            if(!allAreRules) {
                // Tons of steps. We don't want to mess with them.
                debug "OPTIMIZE: Advancing level."
            } else {
                // Add all rules from this level
                // into the stack. That allows us
                // to have a collection of all the rules together.
                // We can then set the current tasks to NULL.
                // When we realize that the next level
                // only consists of steps again, we can
                // put all the ruels into the previous level.
            }
        }
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
    createSteps: function(target, taskContainer, level) {
        taskContainer = taskContainer;
        var finalDeps = [];
        level = level || 0;

        // Mark this target as made.
        if("__isStepped" in target) {
            // Already done with this one.
            return true;
        }
        target.__isStepped = true;

        debug "Level: ${level}"

        if(target.rule.options.noExecute) {
            debug "${target.rule.display}: noExecute found. Skipping."
            return true;
        }

        // Are we dealing with a raw rule?
        if(target.rule.options.raw) {
            debug "Assigning ${target.name} with ${target.rule.display}"
            IceTea.addTask(level, IceTea.Task({
                type: "rule",
                target: target,
                input: target.input
            }), taskContainer);
            return true;
        }

        if("needs" in target) {
            debug "Processing dependencies of ${target.name}"
            for(var _,depName in target.needs) {
                var dep = IceTea.__targets[depName];
                if(!("__isStepped" in dep)) {
                    debug "Dep: ${depName}"
                    IceTea.createSteps(dep, taskContainer, level);
                }
            }
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
                        IceTea.addTask(levelBase++, t, taskContainer);
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
                        throw "${dep}'s output is NULL! (${finalDeps})"
                    } else {
                        outs.push(dep.out);
                    }
                }
                return outs;
            }
        });
        target.__finalTask = finalTask;
        IceTea.addTask(IceTea.getLevel(taskContainer)+1, finalTask, taskContainer);

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

            // A rule that exports needs probably wants to do something with it.
            // So we copy it into the target, to sneakily have it build in the normal
            // build chain.
            if("needs" in target.rule) {
                if(__.isArray(target.rule.needs)) {
                    needs = needs + target.rule.needs;
                }
            }

            // Configure the target
            if(!target.isConfigured()) {
                var rt2 = target.configure();
                if(typeOf(rt2) == "boolean" && !rt2) {
                    return rt2;
                }
            }

            // Prepare the dependency object.
            var needs = "needs" in target ? target.needs : [];

            debug "Are we configuring dependencies? ${needs} (${target})"
            if(typeOf(needs) == "array") {
                for(var _,depName in needs) {
                    debug "Configuring dependency: ${depName} of ${targetName}"
                    var rt = configureTarget(depName, targetName);

                    // If this retuend false, return false.
                    if(typeOf(rt) == "boolean" && !rt) return rt, depConfigured;
                }
            }

            // And the rule.
            if(!target.rule.isConfigured()) {
                var rt3 = target.rule.configure(target);
                if(typeOf(rt3) == "boolean" && !rt3) {
                    return rt2;
                }
            }

            if(typeOf(dependsOn) == "string") {
                // We are being a child, so we should export ourself into parent.
                var dependant = IceTea.__targets[dependsOn];
                if(!dependant.hasMergedWith(targetName)) {
                    if(
                        "exports" in target
                        && "settings" in dependant
                    ) {
                        debug "Merging exports from ${target.name} into settings of ${dependsOn}"
                        dependant.settings = dependant.settings + target.exports;
                    }
                    dependant.setHasMergedWith(targetName);
                }
                debug "Done merging."
            }
            debug "Done configuring."
            return true;
        }

        for(_,tname in targetNames) {
            // Time for some recursion!
            var rt = configureTarget(tname);
        }
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
    Transformer: function(targetNames, taskContainer) {
        debug "Transformer: ${targetNames}"
        for(var _,targetName in targetNames) {
            var target = IceTea.__targets[targetName];
            debug "Transforming ${targetName}";
            IceTea.createSteps(target, taskContainer);
        }
    },

    Runner: function(taskContainer, buildTargets, outputPrefix, forceDetail){
        // Global or custom task-container?
        // FIXME: Syntactic shuggar, use matrix.
        taskContainer = taskContainer;

        // normalize the third, now new flag.
        forceDetail = forceDetail || false;

        // First, determine the way of reporting.
        var isCompact = @{
            if(
                forceDetail
                || cli.check("--debug")
                || cli.check("--detail-output")
                //|| Is this a TTY? FIXME: $.isTTY() to check for TTY.
            ) {
                return false;
            } else {
                return true;
            }
        };
        var isColorful = !cli.check("--no-color");
        var reportTarget = function(currentIdx, maxIdx, level, task) {
            // Shorthand:
            var _write = process.stdout.write;

            // The variables used to display things.
            var buildstr = @{
                var orig = typeOf(task.in) == "array" ? task.out : task.in;
                if("_present" in task.backend) {
                    orig = task.backend._present.call(task);
                }
                return orig;
            };
            var targetTitle = task.target.title;
            var stepDisplay = task.backend.display;

            // Clear the current line before printing into it.
            if(isCompact) {
                _write "\r"
                for(var i=0; i<$.cols-2; i++) {
                    _write " "
                }
                _write "\r"
            }

            // Print logic... fancy, eh?
            // If you zoom out and look at it from far, far away...
            // ... it looks kinda like ASM ;)
            outputPrefix = outputPrefix || ""
            if(outputPrefix != "" && typeOf(outputPrefix.find(" ")) == "null") {
                outputPrefix += " ";
            }

                            _write outputPrefix;
            if(isColorful)  $.saveDefaultColor();
            if(isColorful)  $.setColor($.Colors.CYAN);
                            _write "[${currentIdx}/${maxIdx}] ";
            if(isColorful)  $.resetColor();
                            _write targetTitle .. "(";
            if(isColorful)  $.setColor($.Colors.MAGENTA);
                            _write stepDisplay;
            if(isColorful)  $.resetColor();
                            _write "): ${buildstr}";
            if(!isCompact)  _write "\n"
        }

        // The n-th task we're running
        var currentIndex = 0;
        // The maximum of tasks.
        var maxIndex = IceTea.getTaskCount(taskContainer);
        // The maximum of parallel tasks to run.
        var maxParallel = toNumber(cli["-j"]);

        debug "Executing ${maxIndex} tasks with ${maxParallel} in parallel."

        // Step through each level individually.
        for(var level,tasks in taskContainer) { //try {
            debug "Entering level: ${level}"

            // Backgrounded tasks (Status.PENDING)
            var backgroundTasks = [];

            // In case of an error, wait for all tasks to finish and exit.
            var shouldExit = false;

            for(var id,task in tasks) {
                // Is this task hidden?
                // Hidden tasks == cached output.
                if(task.isHidden()) {
                    continue;
                }

                // Report it.
                reportTarget(++currentIndex, maxIndex, level, task);

                // Run.
                task.run();

                // Status: OK, FAIL or PENDING
                var status = task.test();
                var S = IceTea.Task.Status;
                switch(status) {
                    case S.OK:
                        // Nothing to be done here.
                        debug "Status: OK (Immediate task.)"
                        task.cache();
                        continue;
                    case S.FAIL:
                        // Signal everyone that this is failure.
                        debug "Status: FAIL (Preparing for shutdown.)"
                        shouldExit = true;
                        break;
                    case S.PENDING:
                        // We want to add this job to the background.
                        // However, we shouldn't add more jobs to the
                        // backgroundTasks array than we have specified.
                        debug "Status: PENDING (Pushing into queue.)"
                        while(#backgroundTasks >= maxParallel) {
                            debug "Waiting to push. In queue: ${#backgroundTasks} of ${maxParallel}"
                            // We need to see if one of the background
                            // tasks has been completed, and remove it.
                            var newTasks = [];
                            for(var i,bTask in backgroundTasks) {
                                var status = bTask.test();
                                switch(status) {
                                    case S.OK:
                                        // This one is good to go.
                                        debug "Waiting on empty spot; status: OK"
                                        bTask.cache();
                                        break;
                                    case S.FAIL:
                                        // It had a problem. Exit now.
                                        debug "Waiting on empty spot; status: FAIL (Preparing shutdown)"
                                        shouldExit = true;
                                        break;
                                    case S.PENDING:
                                        debug "Waiting on empty spot; status: PENDING (Re-adding)"
                                        newTasks.push(bTask);
                                        break;
                                    default:
                                        throw "Unknown status! ${task.out} -> ${status}"
                                }

                                // Should we exit?
                                if(shouldExit) break;

                                // Swap the queues.
                                backgroundTasks = newTasks;
                            }

                            // Did one of the previous tasks fail?
                            // If so, we gotta bail. (rhyme was not intended!)
                            if(shouldExit) break;
                        }
                        // Push the task.
                        if(!shouldExit) {
                            debug "Pushing new task to queue. (${task.out})"
                            backgroundTasks.push(task);
                        }
                        break;
                }

                // Short-circuit this loop. No further execution needed.
                if(shouldExit) break;
            }

            // Drain the queue.
            // Even when an error has occured, we need to be graceful.
            debug "Draining queue now (${#backgroundTasks})."
            while(#backgroundTasks > 0) {
                debug "In queue: ${#backgroundTasks}"
                var remainder = [];
                for(var i,xtask in backgroundTasks) {
                    var status = xtask.test();
                    debug "Status: ${status}";
                    switch(status) {
                        case S.FAIL:
                            // We might be exiting anyway.
                            // So we can gracefuly ignore this one.
                            // However, we still should raise the flag.
                            debug "Queue drain: Task failed. (${xtask.out})"
                            shouldExit = true;
                            break;
                        case S.OK:
                            // The task is done. Good!
                            debug "Queue drain: Task was successful. (${xtask.out})"
                            xtask.cache();
                            break;
                        case S.PENDING:
                            // Not done yet. Wait for it.
                            debug "Queue drain: Still waiting on this one. (${xtask.out})"
                            remainder.push(xtask);
                            break;
                        default:
                            throw "Unknown status! ${task.out} -> ${status}"
                    }
                }
                // Swap.
                debug "Swapping ${#backgroundTasks} with ${#remainder}"
                backgroundTasks = remainder;
            }

            // IF:      One/Many tasks exited with Status.FAIL,
            // THEN:    Exit using `return 1`. We can't commulate error codes.
            // ELSE:    Enter next level.
            if(shouldExit) {
                debug "Exiting now."
                return 1;
            } else {
                debug "Advancing to next level. ${#backgroundTasks}"
                continue;
            }
        } /*} catch(e) {
            // FIXME: Unify this, and below.
            unhandledException(e);
            if(isCompact) print "";
            return 1;
        }*/

        debug "Reached end of control. Beginning finalization...";
        for(var _,target in buildTargets) {
            if("finalize" in target) {
                detect.line "Finalizing: ${target.name}"
                target.finalize();
            }
        }
        if(isCompact) print "";
        return 0;
    },

    // Runs a target through a secondary build chain.
    SubRunner: function(target) {
        detect.info "Sub-Build: ${target.name}"
        var taskContainer = [];
        var rt;
        var funcSteps = [
            [
                "Preprocessor (Configure)",
                {|| IceTea.Preprocessor([ target.name ])}
            ], [
                "Transformer (Creating steps)",
                {|| IceTea.Transformer([target.name], taskContainer)}
            ], [
                "Runner (Executing steps)",
                {|| IceTea.Runner(taskContainer, [target], "|")}
            ]
        ];
        for(var i,set in funcSteps) {
            var desc,func = set.unpack();
            detect.line "Running:"
            detect.status desc
            rt = func();
            if(__.isNull(rt)) {
                rt = true;
            }
            if(!rt) return false;
        }
        return true, taskContainer;
    },

    Main: function() {
        $.saveDefaultColor();
        var rt;
        var targetNames = [];
        var taskContainer = [];
        IceTea.CallEvent("beforeRun");

        // Execute the actions.
        if(cli.check("--target")) {
            targetNames = cli["--target"].split(",");
        } else {
            for(var _,name in IceTea.toExecute) {
                if(!(name in IceTea.__actions)) {
                    throw "Action <${name}> does not exist!";
                }
                var action = IceTea.__actions[name];
                var list = action.execute(null);
                if(typeOf(list) == "array") {
                    // We got a list of target names.
                    targetNames += list;
                } else if(typeOf(list) == "number") {
                    // The action returned an exit code.
                    return list;
                }
            }
        }

        if(#targetNames == 0) {
            // If empty, we can assume that the action did something on it's own.
            return 0;
        }

        // Preprocess all the things.
        rt = IceTea.Preprocessor(targetNames);
        if(typeOf(rt) == "boolean" && rt == false) return 1;

        if(!cli.check("--configure")) {
            // Then transform them.
            rt = IceTea.Transformer(targetNames, taskContainer);
            if(typeOf(rt) == "boolean" && rt == false) return 1;

            // And run it too.
            $.Cursor.hide();
            try {
                rt = IceTea.Runner(taskContainer, @{
                    var ts = [];
                    for(var _,name in targetNames) {
                        ts.push(IceTea.__targets[name]);
                    }
                    return ts;
                });
            } catch(e) {
                unhandledException(e);
                rt = 1;
            }
            $.Cursor.show();
        }

        // Reset things.
        $.resetColor();
        return rt;
    },

    // Does common configuration stuff.
    Configurer: function(obj) {
        debug "Configuring..."
        var o = obj.__getStore();
        if("configure" in o && typeOf(o.configure) == "function") {
            var rt = o.configure.apply(obj, ...);
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

        // Berify things.
        var schemes = ["build", "clean"];
        for(var _,scheme in schemes) {
            if(!(scheme in o)) {
                throw IceTea.RuleException(@display,"A rule is required to define a method of name: ${scheme}");
            } else if(typeOf(o[scheme]) != "function") {
                throw IceTea.RuleException(@display,"A rule's scheme property must be a function.");
            }
        }

        // Rules can have options.
        if(!("options" in o)) {
            o.options = {};
        }
        o.options = {
            // A rule setting this to true receives the input as-is.
            raw: false,

            // If this is false, no file-exists checks are made.
            filesRequired: true,

            // With this option set, a rule is never really executed.
            // Helpful for stuff like headers etc.
            // Also, thi sallows dummy-files to be install-able.
            noExecute: false
        } + o.options;
    },
    isConfigured: function() {
        return this.__isConfigured;
    },
    configure: function() {
        return IceTea.Configurer(this, target);
    },
    modify: function(target) {
        var o = @__getStore();
        if("modify" in o) {
            o.modify.call(this, target);
        }
    },
    estimate: function(target) {
        var o = this.__getStore();
        if("estimate" in o && typeOf(o.estimate) == "function") {
            o.estimate.call(this, target);
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
        debug "Creating Target ${name} with rule: ${rule.name || rule.display}"
        @__setStore(opts);
        @name = name;
        @rule = rule;

        // Sanity checks
        var type = typeOf(opts.input);
        if(typeOf(opts.input) == "null") {
            throw "${name}'s input are of illegal type: ${type}.";
        }
        if("settings" in opts) {
            @settings = opts.settings;
        } else {
            @settings = {};
        }
        if("exports" in opts) {
            @exports = opts.exports;
        } else {
            @exports = {};
        }
        @__isConfigured = false;
        @__mergedWith = [];

        // If the rule requies files to exist, check it now.
        if(@rule.options.filesRequired) {
            if(!__.isArray(opts.input)) {
                debug "${@name}: Input is not array. Skipping file-checks."
            } else {
                for(var i,file in opts.input) {
                    if(!pfs.isPresent(file)) {
                        if(!cli.check("--no-color"))
                            $.setColor($.Colors.RED);
                        print "${@name}: File ${file} not found!"
                        $.resetColor();
                        terminate(1);
                    }
                }
            }
        }

        // Let the rule modify us.
        @rule.modify(this);
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

    isHidden: function() {
        if(!__.isArray(@in) && !__.isString(@in)) {
            // We can't estimate this.
            // It might be part of a generator, so rebuild.
            debug "isHidden: Skipping: ${@in}"
            return false;
        }
        if(!("previous" in this) && !("deps" in this)) {
            debug "isHidden: On base target (${@in} -> ${@out})"
            // This is a base target.
            var ins = __.isArray(@in) ? @in : [@in];
            for(var i,file in ins) {
                debug "isHidden: ${file}"
                var mtime = pfs.fileModified(file);
                debug "isHidden: mtime: ${mtime} (${file})"
                if(file in IceTea.FileCache) {
                    var cv = IceTea.FileCache[file];
                    var cv_n toNumber(cv);
                    debug "isHidden: ${file} -> cv: ${cv} (${#cv}) -> cv_n: ${cv_n} (${#cv_n})"
                    if(toNumber(cv_n) > mtime) {
                        // The file was modified, we are no longer hidden.
                        debug "isHidden: ${file} modified"
                        return false;
                    } else {
                        debug "isHidden: ${file} Unmodified"
                        return true;
                    }
                } else {
                    debug "isHidden: ${file} not in cache at all."
                    return false;
                }
            }
            debug "Reached end of check."
            // No modifications.
            return true;
        } else {
            // Easy. We depend on our base! :)
            if("deps" in this) {
                debug "isHidden: On (probably) top-most target"
                // If one of our deps is not hidden, we aren't either.
                for(var i,file in @in) {
                    // FIXME: Avoid CP, make a function instead.
                    debug "isHidden: ${file}"
                    var mtime = pfs.fileModified(file);
                    debug "isHidden: mtime: ${mtime} (${file})"
                    if(file in IceTea.FileCache) {
                        var cv = IceTea.FileCache[file];
                        var cv_n toNumber(cv);
                        debug "isHidden: cv: ${cv} (${#cv}) -> cv_n: ${cv_n} (${#cv_n})"
                        if(toNumber(cv_n) > mtime) {
                            // The file was modified, we are no longer hidden.
                            return false;
                        }
                    } else {
                        debug "isHidden: ${file} not in cache at all."
                        return false;
                    }
                }
                return true;
            } else {
                debug "isHidden> Somewhere in the middle, checking child. ${@in}"
                return @previous.isHidden();
            }
        }
    },

    cache: function() {
        if(!__.isArray(@in) && !__.isString(@in)) {
            return;
        }
        var input = __.isArray(@in) ? @in : [@in];
        for(var i,file in input) {
            var mtime = pfs.fileModified(file);
            IceTea.FileCache[file] = mtime;
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

    // Gets the current status _WITHOUT_ asking the backend first.
    // Mainly used internally.
    status: function() {
        return this._status;
    },

    // Run this task.
    run: function() {
        try {
            var rt = this.backend[IceTea.scheme].call(this);
            debug "Task returned: ${rt} (${typeOf(rt)})"
            return this._setStatus(rt);
        } catch(e) {
            unhandledException(e)
            this._status = @Status.FAIL;
            return true;
        }
    },

    // See if this task is still running.
    // In case of an async process being the underlying task,
    // this will cause the ::tick() method to be called.
    test: function() {
        try {
            var S = IceTea.Task.Status;
            if(@status() != S.PENDING) {
                debug "Task already returned."
                return @status();
            } else {
                var rt = this.backend.status.call(this);
                @_status = rt;
                debug "${@out}: Status is ${rt} (${typeOf(rt)})"
                return rt;
            }
        } catch(e) {
            print "CRITICAL: Error while testing ${@name}.";
            unhandledException(e);
            return @Status.FAIL;
        }
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
