function printHead(num) {
    for(i=0; i<num; i++) {
        echo "*";
    }
    print();
}
var tests = [];
var stars = (sys.getenv("COLUMNS")!="" ? sys.getenv("COLUMNS") : 90);

for(var _,f in pfs.getFileList(__DIR__)) {
    if(pfs.filename(f) != pfs.filename(__FILE__)) {
        tests[] = f;
    }
}
tests = tests.sort();


printHead(stars);
print "    IceTea tests"
print ""
print "      Available tests:"
for(var _,fn in tests) {
    print "        - ${pfs.basename(fn)} (${fn})"
}
printHead(stars);

echo "\n\n"
for(var _,tf in tests) {
    print "\n\n--- Running: ${tf}"
    require(tf);
}
