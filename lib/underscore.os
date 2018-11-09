/**
 * This is sort of like underscore, lodash, microdash or anything alike.
 *
 * But, for IceTea.
 */

underscore = extends _E {
    isDefined:  {|val| return typeOf(val) != "null"; },
    isNull:     {|val| return typeOf(val) == "null"; },
    isBool:     {|val| return typeOf(val) == "boolean"; },
    isNumber:   {|val| return typeOf(val) == "number"; },
    isString:   {|val| return typeOf(val) == "string"; },
    isFunction: {|val| return typeOf(val) == "function"; },
    isArray:    {|val| return typeOf(val) == "array"; },
    isObject:   {|val| return typeOf(val) == "object"; },
    isPointer:  {|val| return typeOf(val) == "userdata"; },

    each: function(target, cb) {
        if(@isObject(target) || @isArray(target)) {
            for(var key,val in target) {
                cb(val, key, target);
            }
        } else {
            var t = typeOf(target);
            throw "_.each can only iterate over objects or arrays. Got: ${t}";
        }
    },

    arrify: function(stringOrArray) {
        if(typeOf(stringOrArray) == "string") {
            stringOrArray = [stringOrArray];
        } else if(typeOf(stringOrArray) != "array") {
            var t = typeOf(stringOrArray);
            throw "Array or string expected. Got: ${t}";
        }
        return stringOrArray;
    }
};

_ = underscore;
__ = underscore;
U = underscore;
