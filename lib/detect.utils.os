// Prefixes
detect = detect + {
    P_HEADER:   "HAVE_",
    P_TYPE:     "HAVE_",
    P_SIZEOF:   "SIZEOF_",
    P_MACRO:    "IS_DEFINED_",
    P_FUNC:     "HAVE_",
    P_LIB:      "HAVE_LIB"
};

detect = detect + {
    haveHeader: function(name) {
        return detect.P_HEADER .. name
            .replace("/","_")
            .replace("\\","_")
            .replace(".","_")
            .replace("-","_")
            .upper();
    },

    haveType: function(type) {
        return detect.P_TYPE .. type
            .replace(" ","_")
            .upper();
    },

    haveSizeof: function(type) {
        return detect.P_SIZEOF .. type
            .replace(" ","")    // Thou shalt not use spaces!
            .replace("::","__") // C++ types
            .replace("*","")    // Pointers. Should I use P instead?
            .upper();
    },

    haveMacro: function(macro) {
        return detect.P_MACRO .. macro;
    },

    haveTypeInHeader: function(type, header) {
        return detect.P_TYPE .. type
            .. "_IN_"
            .. @haveHeader(header).sub(#detect.P_HEADER)
            .upper();
    },

    haveLib: function(lib) {
        return detect.P_LIB .. lib
            .upper();
    },

    haveFunc: function(func) {
        return detect.P_FUNC .. func
            .upper()
    },

    haveLibFunc: function(lib, func) {
        return detect.P_FUNC
            .. func
            .. "_IN_"
            .. "LIB" .. lib
            .upper();
    },

    haveTool: function(tool) {
        return tool
            .replace("+","_")
            .replace("-","_")
            .upper();
    },

    // Printing related functions...
    start: function() {
        print detect.header;
    },
    end: function() {
        print detect.footer;
    },
    info: function(str) {
        print detect.out .. str;
    },
    line: function(str) {
        echo detect.out .. str;
    },
    coloredStatus: function(c, str) {
        if(cli.check("--no-color")) {
            print " " .. str;
        } else {
            $.saveDefaultColor();
            process.stdout.write(" ");
            $.setColor(c);
            process.stdout.write(str);
            $.resetColor();
            process.stdout.write("\n");
        }
    },
    fail: function(str) {
        @coloredStatus($.Colors.RED, str);
    },
    success: function(str) {
        @coloredStatus($.Colors.GREEN, str);
    },
    status: function(str) {
        @coloredStatus($.Colors.MAGENTA, str);
    },
    write: function(str) {
        echo " ${str}"
    }
};
