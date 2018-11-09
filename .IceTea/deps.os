_E += {
    get: function(name) {
        return pfs.join(
            __FILE__,
            "..",
            "deps",
            name
        );
    }
}
