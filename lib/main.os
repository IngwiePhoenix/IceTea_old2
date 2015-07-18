function __get(name) {
    throw "Trying to access unknown variable ${name}.";
}

function main(args) {
    // Pre-check
    //if(cli->check("-u")) cli->group("Project options");
    // init
    // cli.parse
    // if -u == usage
    // else IceTea.Preprocessor
    // if --print-json, print targets as json.

    // Determine run scheme
    if(cli.check("-w")) {
        IceTea.scheme = "clean";
    } else if(cli.check("-i")) {
        IceTea.scheme = "install";
    } else if(cli.check("--scheme")) {
        IceTea.scheme = cli["--scheme"];
    }

    // Status
    var rt;

    rt = IceTea.Initializer();
    if(rt==false) return 1;
    if(cli.check("-u")) {
        // Usage?
        return cli.usage();
    } else if(cli.check("--print-targets")) {
        // dump something...
        print json.encode(IceTea.__targets);
        return 0;
    } else if(cli.check("--print-actions")) {
        print json.encode(IceTea.__actions);
        return 0;
    } else if(cli.check("--print-rules")) {
        print json.encode(IceTea.__rules);
        return 0;
    }
    var toBuild = [];
    if(cli.stray.length == 0) {
        toBuild = IceTea.__actions["all"][IceTea.scheme];
    } else {
        for(var i,aname in cli.stray) {
            toBuild = toBuild + IceTea.__actions[aname][IceTea.scheme];
        }
    }

    // Postprocess.
    if("*" in toBuild) toBuild = IceTea.tag();

    // Insert
    IceTea.toBuild = toBuild;

    rt = IceTea.Preprocessor();
    if(typeOf(rt) == "boolean" && rt == false) return 1;

    rt = IceTea.Transformer();
    if(typeOf(rt) == "boolean" && rt == false) return 1;

    for(var i,v in IceTea.__tasks) {
        print "-- ${i}";
        for(var output,task in v) {
            print "  In: ${task.input} - Out: ${output}"
        }
    }
}
