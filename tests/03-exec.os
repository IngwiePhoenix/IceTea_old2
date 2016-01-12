/**
    os-exec:
        $(command [, replaces])

    The interface to run shell commands, demo.
*/

exec = $;
function $(command) {
    print "$ ${command} | ${...}";
    var s,e,o = exec.apply(this, arguments);
    print "$? = ${e}"
    print "STDOUT:\n${o[1]}"
    print "STDERR: ${o[2]}\n"
}

print "Running a simple command...";
$ "ls";

print "Running with arguments";
$ "ls -1 ."

print "Running with argument replacements"
$("echo ?", "Hello, World!");

print "Running with quoted replace"
$("echo '?'", "Yo, amma quoted.");

print "Using the exec command now."
var r = shell("ls -aGp")
print "Result is: " .. r

print "Without capturing the output"
shell "ls -aGp"

// Swap back.
$ = exec;
