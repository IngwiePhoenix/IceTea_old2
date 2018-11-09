if(!ENVIRONMENT_IS_NODE) {
    throw new Error("Can only run on NodeJS!");
}

function debugLog(str) {
    if(process.env.DEBUG) {
        process.stdout.write('[IceTea:Emscripten]: '+str+'\n');
    }
}

if(typeof ENV != "undefined") {
    // Make the ENV available.
    for(var name in process.env) {
        ENV[name] = process.env[name];
    }
}

// Mount the paths
var path = require('path');
var fs = require("fs");
var list = fs.readdirSync(path.resolve('/'));
list.forEach(function(item){
    var p = path.join('/', item);
    var stat = fs.lstatSync(p);
    if(stat.isDirectory()) {
        debugLog('Mounting...');
        try {
            debugLog('mkdir('+p+')');
            FS.mkdir(p);
            debugLog('mount('+p+')');
            FS.mount(NODEFS, {root: p}, p);
        } catch(e) {
            debugLog("Skipped");
        }
    }
});

// Navigate to current folder
FS.chdir(process.cwd());

// Call main
Module.callMain(Module.arguments);
postRun();
