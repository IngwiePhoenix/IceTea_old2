print "----- Array.__add"
var myArr = ["foo", "baz"];
print "myArr original: " .. myArr;
myArr + ["bar"];
print "myArr changed: " .. myArr;

print "\n----- Array and +="
var list=["the"];
list += ["meeper"];
print "List: "..list;

print "\n----- Object.__add"
var test   = {};
test       + {name: "Mario"};
test       + {name: " Bros."};
test.list  = ["one"];
test       + {list: ["two"]};
test.acron = {mfg: "Mit freundlichen Grüßen"};
test       + {acron: {ttyl: "Talk to you later"}};
test.num   = 9;
test       + {num:3};
print test;

print "\n----- Complex Object.__add"
var person = {
    name: "Ingwie",
    age: 20,
    hobbies: ["dj", "coding"],
    coding: {
        languages: ["c++", "php"]
    }
};
print person;
person + {
    name: " Phoenix",
    hobbies: ["yugioh"],
    coding: {
        tools: ["llvm"]
    }
};
print person;

print "\n----- Exiting using die()"
print "I will be seen!"
abort("ded\n")
print "I won't."
