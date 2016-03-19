/**
    @file
    @brief The main IceTea logic

    This file contains and holds upon all the functions that translate targets,
    actions, externals and rules into some logic. This is done by processing
    the input using a single thread, and executing the tasks over a multiple of
    threads (depending on the given hardware).

    Only minor additions are made to the script environment, that contain simple
    wrapper functions.
*/

#include "IceTea.h"
#include "file_system.hpp"
#include <stdio.h>
#ifdef EMSCRIPTEN
#include <emscripten.h>
#endif

int main(int argc, char** argv) {
    #ifdef EMSCRIPTEN
    EM_ASM(
        function debugLog(str) {
            if(process.env.DEBUG) {
                process.stdout.write('[IceTea:Emscripten]: '+str+'\n');
            }
        }
        // Mount CWD
        /*
        debugLog("Mounting ("+process.cwd()+") into /CWD");
        FS.mkdir('/CWD');
        FS.mount(
            NODEFS,
            { root: '.' },
            '/CWD'
        );
        */

        // Make the ENV available.
        for(var name in process.env) {
            ENV[name] = process.env[name];
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
    );

    // Navigate to the virtual cwd...
    stlplus::folder_set_current(
        emscripten_run_script_string("process.cwd();")
    );
    #endif

    IceTea* it = IceTea::create();
    return it
        ->setupCli(argc, (const char**)argv)
        ->run();
}
