// This is the global IceTea object.
IceTea = extends _E {
    // The scheme (aka. action method) to run.
    scheme: "build",
    // The folder where output files should end up
    outputDir: __outputdir,
    // If tasks should be ran or not.
    runTasks: true,
    // The targets to build
    toBuild: [],
    toBuildResolv: [],

    // Global collectors

    // Rules
    __rules: {},
    addRule: function(n,r) {
        if(n in IceTea.__rules && cli.check("-W")) {
            print "WARNING: Overwriting previously defined rule \""+r+"\"";
        }
        IceTea.__rules[n]=r;
    },
    estimateRuleOutput: function(rule, target, file) {
        file = file || "";
        if(!(rule is IceTea.Rule)) {
            throw "Rule must be an instance of IceTea.Rule.";
        } else if(!(target is IceTea.Target)) {
            throw "Target must be an instance of IceTea.Target.";
        } else {
            var expected = rule.output.expected;
            var filename = pfs.filename(file);
            var basename = pfs.basename(file);
            var ext = pfs.extname(file);
            var out = expected
                .replace('%t', target.name)
                .replace('%b', basename)
                .replace('%e', ext)
                .replace('%f', filename)
                .replace('%o', IceTea.outputDir);

            return out;
        }
    },

    // Targets
    __targets: {},
    addTarget: function(n,t) {
        if(n in IceTea.__targets) {
            print "WARNING: ${n} is already defined."
        } else {
            IceTea.__targets[n]=t;
        }
    },
    // Return targets by tag. Supports wildcards.
    tag: function(t) {
        if(typeOf(t) == "null") {
            return IceTea.__targets.keys;
        } else {
            var res = [];
            for(name,target in IceTea.__targets) {
                if(typeOf(target.info.tag) != "null" && wildcard.match(t,target.info.tag)) {
                    // Target has a tag AND it fits. Add.
                    res.push(name);
                }
            }
            return res;
        }
    },

    // Actions
    __actions = {},
    addAction: function(n, a) {
        if(n in IceTea.__actions && cli.check("-W")) {
            print "WARNING: Overwriting previously defined action \"${n}\"";
        }
        IceTea.__actions[n]=a;
    },

    // Tasks. They are topologicaly sorted - only the master nodes should go in.
    __tasks = {},
    /**
        @brief Create the actual tasks

        This function contains a LOT of logic and represents the heartpiece of the
        entire project. It looks up the rules and compares a file and other information
        against its results. The finalized task is added to the @ref TaskQueue

        This function also calls itself recursively and passes input trough referenced
        objects.

        @param rule The rule to be used in the current iteration.
        @param file The file to be used for the current iteration.
        @param target The target to iterate on.
        @param[out] input The input to be returned to the parent call
        @param[in,out] parent The @ref Task object to be passed as a parent.
        @param[out] success A bool that will be set to true if the iteration was indeed a success. False if otherwise.
        @param finalRule a boolean that is used to tell the __first__ iteration about it.
    */
    createTasks: function(targets) {
        if(typeOf(targets) == "string") {
            targets = [targets];
        }

        var __makeTasks = function(target, rule, file, recursive) {
            // Determine if we called ourself.
            recursive = recursive || false;

            // See if the supplied file is accepted by this rule or not.
            var accepted = false;
            for(var i,acc in rule.accepts) {
                if(wildcard.match(acc, file)) {
                    accepted = true;
                    break;
                }
            }
            if(accepted) {
                // i.e.: myfile.x matches *.x
                // This matches! This is the rule we want.
                var t = IceTea.Task(target, rule, file);
                if(rule.displayName != target.rule.displayName) {
                    // Only add if we have not met the top target.
                    IceTea.__tasks[target.name][t.output] = t;
                }
                return {
                    rule: rule,
                    estimated: t.output,
                    input: t.input;
                };
            } else {
                // It does not match. So we have to look for a rule that outputs what this rule wants.
                var res = false;
                for(_,$rule in IceTea.__rules) {
                    if($rule.output.pattern in rule.accepts) {
                        // This matches. So let's pass on
                        res = __makeTasks(target, $rule, file, true);
                        if(typeOf(res) == "object") break;
                    }
                }
                if(typeOf(res) == "object") {
                    if(res.rule.displayName != rule.displayName) {
                        // We are still not at the top.
                        return __makeTasks(target, rule, res.estimated);
                    }
                } else {
                    //throw "There is appearently no rule to process file ${file}.";
                }
            }
        };

        // Grab the targets off the collection
        var toBuild = [];
        for(i,n in targets) {
            if(n in IceTea.__targets)
                toBuild.push( IceTea.__targets[n] );
            else
                throw "Target ${n} does not exist!";
        }

        for(i,target in toBuild) {
            // Prepare the task entry
            IceTea.__tasks[target.name] = {};
            var finalInput = [];
            var finalOut = IceTea.estimateRuleOutput(target.rule, target);
            for(i,file in target.info.input) {
                // Trigger the back-tracking.
                var rt = __makeTasks(target, target.rule, file);
                finalInput.push(rt.input);
            }
            IceTea.__tasks[target.name][finalOut] = IceTea.Task(target, target.rule, finalInput, true);
        }
    },

    /** @section The task processors
       There are a few of them:
            - Initializer: If it exists, call init() on each target. Ommit this, if --help is used.
                * the init() function can add entries to the help screen. Important for configuration.
                * Check if rules that targets reffer to, exist.
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
            var init = target.info.init;
            if(typeOf(init) == "function") {
                var rt = init.call(target.info);
                // Returning false halts the initialization, thus cancels anything else.
                if(typeOf(rt) == "boolean" && !rt) return rt;
            }
        }
    },

    /**
        @brief Configure all targets and resolve dependencies

        This function calls the targets configure method in order to make them
        ready for actual compilation.

        It also looks for an existing `need` property and if it exists, grabs the
        other target's configure methods and runs them first. It then combines
        the `exports` property with the local `settings` property, if it exists.

        For a configuration to fail explicitly, it must use the abort/exit methods.
    */
    Preprocessor: function() {
        var configured = [];
        for(_,tname in IceTea.toBuild) {
            var target = IceTea.__targets[tname];
            if(typeOf(target) == "null") {
                throw "Target ${tname} does not exist.";
            }

            if(typeOf(target.needs) == "array") {
                for(_,tn in target.needs) {
                    var t = IceTea.__targets[tn];
                    if(typeOf(target) == "null") {
                        throw "Target ${tn} (dependent of ${tname}) does not exist.";
                    }
                    if(t == target) continue; // Skip this one, itll be configured in a bit.
                    configured.push(tn);
                    var configure = t.info.configure;
                    if(typeOf(configure) == "function") {
                        var rt = configure.call(t);
                        // If false, stop.
                        if(typeOf(rt) == "boolean" && rt == false) return rt;
                    }
                    // Update settings
                    target.settings = target.settings + t.exports;
                }
            }

            // Configure main target
            var mainConfigure = target.info.configure;
            if(typeOf(mainConfigure) == "function") {
                var rt = mainConfigure.call(target);
                // Returning false halts the initialization, thus cancels anything else.
                if(typeOf(rt) == "boolean" && !rt) return rt;
            }

            configured.push(tname);
        }
        IceTea.toBuildResolv = configured;
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
    Transformer: function() {
        IceTea.createTasks(IceTea.toBuildResolv);
    }
}

IceTea.RuleException = extends Exception {
    __construct = function(prefix, msg) {
        return super(prefix+" :: "+msg);
    }
};

IceTea.Rule = extends Object {
    __construct: function(displayName, o) {
        this.intern = o;
        this.displayName = displayName;
    },
    __get: function(key) { return this.intern[key]; },
    __get@display: function() { return this.display; },
    verify: function() {
        var o = this.intern;
        if(typeOf(o.accepts) != "string")
            throw IceTea.RuleException(this.display,"Rule.accepts must be a string.");

        if(typeOf(o.output) != "object" || (
            typeOf(o.output.pattern) != "string"
            && typeOf(o.output.expected) != "string"
        )) throw IceTea.RuleException(this.display, "Rule.output is misstructured.");

        if(typeOf(o.build) != "function")
            throw IceTea.RuleException(this.display,"Rule.build must be specified and be a function.");
    }
}

IceTea.Target = extends Object {
    __construct: function(name, r, obj) {
        this.master = null;
        this.built = false;
        this.children = [];
        this.parent = null;
        this.info = obj;
        this.name = name;
        this.rule = r;
    }
};

IceTea.Task = extends Object {
    type = extends _E {
        SCRIPT: 1,
        COMMANDS: 2
    },
    __construct: function(target, rule, file, master) {
        // Used to pick up when topo-sorting.
        this.top = master || false;
        this.target = target;
        this.rule = rule;
        this.input = file;
        if(typeOf(file) == "array") {
            // This is very likely a full final. So make file empty.
            this.output = IceTea.estimateRuleOutput(rule, target);
        } else {
            this.output = IceTea.estimateRuleOutput(rule, target, file);
        }
    }
}
// Configurables
configurable("rule", function(head, body){
    var name = head[0];
    var display = head[1];
    IceTea.addRule(name, IceTea.Rule(display, body));
});
configurable("target", function(head, body){
    var name = head[0];
    var rule = head[1];
    var r = IceTea.__rules[rule];
    if(typeOf(r) == "null") {
        throw "Rule ${head[1]} (used on target ${name}) is unknown.";
    }
    IceTea.addTarget(name, IceTea.Target(name, r, body));
});
configurable("action", function(head, body){
    IceTea.addAction(head[0], body);
});
