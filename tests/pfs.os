/**
    os-pfs: All the file system functions so far.
*/

var oldCwd = sys.fullCwd;
print "Starting at: ${sys.fullCwd}"
print "We are currently at: ${sys.cwd}"

// Change directory
sys.cd "..";
print "Now at: ${sys.fullCwd} --> ${sys.cwd}"

// Going back.
sys.cd oldCwd;

// Lookup function
print "Bash is installed at: ${sys.which('bash')}"

// Own lookup.
var list = pfs.lookup("main.cpp", [__DIR__ .. "/../src/"]);
print "Looked up main.cpp and got: ${list}"
