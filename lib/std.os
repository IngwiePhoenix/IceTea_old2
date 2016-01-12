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
        for(var key, value in operand) {
            if(typeOf(value) == "object" || typeOf(value) == "array") {
                if(!(key in this)) {
                    this[key] = value;
                } else {
                    this[key] += value;
                }
            } else {
                this[key] = value;
            }
        }
    } else {
        var type = typeOf(operand);
        throw "Object.__add: Expected Object, got ${type} instead.";
    }
    return this;
}

function Array.__add(operand) {
    if(typeOf(operand) == "array") {
        for(var _,value in operand) {
            this.push(value);
        }
    } else {
        var type = typeOf(operand);
        throw "Array.__add: Expected Object, got ${type} instead: ${operand}";
    }
    return this;
}

function String.__add(operand) {
    var newThis = this .. operand;
    return newThis;
}
