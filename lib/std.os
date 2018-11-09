// Throw an exception when something could not be found.
function __get(name) {
    throw "Trying to access unknown variable ${name}.";
}
function Object.__get(name) {
    if(name in this) {
        return this[name];
    } else throw "${this.__name} does not have property ${name}.";
}

function Object.__add(operand) {
    if(typeOf(operand) == "object") {
        for(var k,v in operand) {
            if(typeOf(v) == "object" || typeOf(v) == "array") {
                // These two can be added too.
                if(k in this) {
                    this[k] += v;
                } else {
                    this[k] = v;
                }
            } else {
                this[k] = v;
            }
        }
        return this;
    } else {
        var t = typeOf(operand);
        throw "Object.__add: Expected Object, got ${t} instead.";
    }
}

function Object.merge(with) {
    return this;
    for(var k,v in with) {
        this[k] = v;
    }
}

function Array.__add(operand) {
    if(typeOf(operand) == "array") {
        for(var _,value in operand) {
            this.push(value);
        }
    } else {
        this.push(operand);
    }
    return this;
}

function String.__add(operand) {
    var newThis = this .. operand;
    return newThis;
}

// Implement .bind:
function Function.bind(to) {
    var self = this;
    return function() {
        self.apply(to, ...);
    }
}

// Extension for $
$.__instantiable = true;
$.__newinstance = function(cmd) {
    var p = SubProcess({async: false});
    var replaces = ...;
    while(typeOf(cmd.find("?")) != "null") {
        var idx = cmd.find("?");
        var tmpStr = cmd.sub(0, idx);
        var afterStr = cmd.sub(idx+1);
        if(#replaces >= idx) {
            tmpStr = tmpStr + replaces[idx];
            tmpStr = tmpStr + afterStr;
            cmd = tmpStr;
        }
    }
    debug "$ ${cmd}"
    var spawned = p.execute(cmd, false, true, true);
    var stdout = p.stdout();
    var stderr = p.stderr();
    var exit = p.exit_code();
    return spawned, exit, [null, stdout, stderr];
}

_G.system = function(cmd, opt) {
    opt = opt || system.STDOUT;
    var spawned, exitCode, streams
        = $.apply($, cmd);
    if(spawned) {
        switch(opt) {
            case system.EXIT_CODE:
                return exitCode;
            case system.STDOUT:
                return streams[1];
            case system.STDERR:
                return streams[2];
            case system.ALLOUT:
                return streams[1], streams[2];
            case system.HIGHEROUT:
                if(#streams[1] > #streams[2]) {
                    return streams[1];
                } else {
                    return streams[2];
                }
        }
    } else {
        return false;
    }
}
system.EXIT_CODE = 0;
system.STDOUT = 1;
system.STDERR = 2;
system.ALLOUT = 3;
system.HIGHEROUT = 4;

/**
 * Extensions to the File module
 */
File.writeWhole = function(contents, file) {
    var fh = File(file, "w");
    var rt = fh.write(contents);
    fh.close();
    return rt;
}

File.readWhole = function(file) {
    var fh = File(file, "r");
    var size = fh.size;
    var rt = fh.read(size);
    fh.close();
    return rt, size;
}
