/**
 * This is sort of like underscore, lodash, microdash or anything alike.
 *
 * But, for IceTea.
 */

_ = {
    isDefined: {|val| return typeOf(val) != "null"; },
    isFunction: {|val| return typeOf(val) == "function"; },
    isObject: {|val| return typeOf(val) == "object"; }
};
