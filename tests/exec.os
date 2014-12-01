/**
    os-exec:
        $(command [, replaces])

    The interface to run shell commands, demo.
*/

exec = $;
function $(command) {
    print "$ ${command} | ${...}";
    var e,o = exec.apply(this, arguments);
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
