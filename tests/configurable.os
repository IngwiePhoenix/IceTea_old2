configurable("foo")(function(head, body) {
    print "Foo was called with:"
    print "    head: ${head}"
    print "    body: ${body}"
    print ""
});

print "Foo is a: ${foo}"

print "Calling foo as empty:"
foo() {};

print "Calling foo with simple args:"
foo(1) {};

print "Calling it with other things...:"
foo(0x01)(function(){});

print "Passing a variable:"
var meep = "meep.";
foo()(meep);

print "Finally, trying out a deprecation function!"
configurable("deprecated")(function(head, callback){
    _G[head[0]] = function() {
        echo "*** WARNING ***: "
        print "Function ${head[0]} is deprecated: ${head[1]}"
        callback.apply(this, arguments);
    }
});

deprecated("bar", "This is a test.")(function(a, b){
    print "${a}+${b} = "..(a+b)
});

bar(1, 2);

print "One last test."
function baz(food) {
    print "I'd like to order ${food}"
}
deprecated("baz", "You should go to McDonnalds...")(baz);

baz("a cheeseburger!");
