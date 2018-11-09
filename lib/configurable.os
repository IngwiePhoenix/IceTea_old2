function configurable(name, callback) {
    _G[name] = function() {
        var head = ...;
        return function(body) {
            return callback(head, body);
        }
    };
}
