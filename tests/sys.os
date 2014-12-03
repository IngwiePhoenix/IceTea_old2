/**
    os-sys: System functions
*/

var oldCwd = sys.fullCwd;
print "Starting at: ${sys.fullCwd}"
print "We are currently at: ${sys.cwd}"

// Change directory
print("");
sys.cd "..";
print "Now at: ${sys.fullCwd} --> ${sys.cwd}"

// Going back.
sys.cd oldCwd;

// Lookup function
print("");
print "Bash is installed at: ${sys.which('bash')}"

print("");
print "You are home at: ${sys.userDir}"

print("");
print "Your OS:"
print "    Type               : ${sys.type}"
print "    Name               : ${sys.name}"
print "    Directory separator: \"${sys.dirSep}\""
print "    Path separator     : \"${sys.pathSep}\""

print("");
print "Testing your environment..."
print "    HOME: ${sys.getenv('HOME')}"

print(""); // This currently fails.
sys.putenv("foo","bar");
print "A changed environment:"
print "    foo: ${sys.getenv('foo')}"
