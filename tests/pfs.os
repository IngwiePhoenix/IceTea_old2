/**
    os-pfs: All the file system functions so far.
*/

// Own lookup.
print("");
var list = pfs.lookup("main.cpp", [__DIR__ .. "/../src/"]);
print "Looked up main.cpp and got: ${list}"

// File and folder functions
print("");
print "Are we a file for sure? ..." .. (pfs.isFile(__FILE__) ? "Yep." : "Nope.")
print "The folder we are in is " .. (pfs.isDir(__DIR__) ? "totally" : "not") .. " a folder."
if(pfs.isPresent(__FILE__)) {
    print "We are present. Let's do further testing."
}

print("");
print "Our folder contains:"
print "  -- Files"
for(var i,v in pfs.getFileList(__DIR__ .. "/..")) {
    print "   | ${v}"
}
print "  -- Folders"
for(var i,v in pfs.getDirList(__DIR__ .. "/..")) {
    print "   | ${v}"
}

print("");
print "Our source contains those .hpp files (Should all be by STLPlus!)"
for(var i,v in pfs.glob(__DIR__ .. "/../src", "*.hpp")) {
    print "  -- ${v}"
}

print("");
print "Let's disassemble my name:"
print "    __FILE__ => ${__FILE__}"
print "    __DIR__  => ${__DIR__}"
print "    Basename => ${pfs.basename(__FILE__)}"
print "    Extname  => ${pfs.extname(__FILE__)}"
print "    Filename => ${pfs.filename(__FILE__)}"
print "    Dirname  => ${pfs.dirname(__FILE__)} (__FILE__)"
print "    Dirname  => ${pfs.dirname(__DIR__)} (__DIR__)"
print "    Created  => ${pfs.fileCreated(__FILE__)}"
print "    Accessed => ${pfs.fileAccessed(__FILE__)}"
print "    Modified => ${pfs.fileModified(__FILE__)}"

print("");
