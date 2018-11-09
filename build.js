/**
 * Comming soon: Build IceTea via node as Emscripten module!
 *
 * Procedure:
 * - Download Emscripten Portable SDK.
 *   * Fetch latest version and build an URL. For example:
 *   * https://s3.amazonaws.com/mozilla-games/emscripten/releases/emsdk-${version}-portable-${arch}.zip (Windows)
 *   * https://s3.amazonaws.com/mozilla-games/emscripten/releases/emsdk-portable.tar.gz (Linux, OS X)
 * - Extract it, and set up ENV variables.
 * - Compile IncBin via emcc:
 *   * emcc -s INVOKE_MAIN=0 --post-js src/icetea-emscripten.js deps/incbin/incbin.c -o incbin.js
 * - Generate incbin code:
 *   * node ./incbin.js src/scripts.rc -o scripts.c
 * - Compile IceTea via emcc:
 *   * emcc -s INVOKE_MAIN=0 --post-js src/icetea-emscripten.js \
 *       -Isrc -Wno-switch \
 *       src/*.cpp scripts.c \
 *       -DICETEA_INCBIN_FORCE_EXTERNAL \
 *       -DOS_EMSCRIPTEN \
 *       -o icetea.js
 * - Build IceTea using the native toolchain using the JS version.
 *   * node --stack-size=2048 ./icetea.js
 *
 * The reason --stack-size is used is because of ObjectScript. It has a very deep-nesting call hieracy.
 * Therefore you will need a bigger stack size.
 *
 * I am considering using something like shelljs for this part. After all,
 * I will be able to utilize NPM!
 */

var path = require("path");
var fs = require("fs");

// Verbose command execution
function exec(cmd) {
    var cp_exec = require("child_process").execSync;
    console.log("$ "+cmd);
    cp_exec(cmd); // TODO
}

// Default flags to emcc
var em_opts = [
    "-s INVOKE_MAIN=0",
    "--pre-js"
];

// Function to make a download URL.
function getEmUrl() {
    // TODO
}

// Create the output folder: out
fs.mkdir("./out");

console.log("Downloading Emscripten portable SDK");
// Download the portable SDK by getting an URL via getEmUrl().

console.log("Downloading latest incbin source");
// If using GIT, use deps/incbin. When going via NPM, download a zip.
// Consider actually distributing an incbin bundle when deploying to NPM...

console.log("Building the incbin tool");
// Build incbin

console.log("Generating incbin source file");
// Generate the script container from src/scripts.rc

console.log("Building IceTea using Emscripten");
// Build IceTea by using emcc and default options.

console.log("Building native IceTea using Emscripten IceTea");
// Invoke the compiled IceTea to build a native build.
// This is gonna be slow, since it's a scripting language running
// inside a scripting language - and ontop of that, that scripting language
// was originally written in C++...
// Also take the stack size into account.
