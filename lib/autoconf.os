// FIXME: I need to port some casual Autoconf macros to detect.
// This also includes stuff like largefile support.
// And ontop of that, I need to de-unix them. Oh joy.

// FIXME: Maybe I can teach Preprocessor to define a global,
// so that the autotools-like functions dont perform,
// but instead insert their CLI values?

// FIXME: detect.tool() should return: <bool, string>.

// FIXME: Many of these chscks use -D. But windows uses /D. Need to become flag-uniform.

autoconf = extends _E {
    // Largefile (LFS) Support
    // Source: http://www.opensource.apple.com/source/gnutar/gnutar-410.1/tar/m4/largefile.m4
    AC_SYS_LARGEFILE_INIT: function() {
        cli.group "Autoconf-like";
        detect.enable("largefile", "Enable largefile support.");
    },
    AC_SYS_LARGEFILE_FLAGS: function($1) {
        var varName = "ac_cv_sys_largefile_${$1}";
        if(detect.truthyCache(varName)) return true;
        detect.line "Checking for ${$1} value to request large file support"
        var found, getconf = detect.tool("getconf");
        var spawned, exitCode, streams
            = $("(${getconf} LFS_${$1}) 2>/dev/null");
        if(spawned) {
            if(exitCode != 0) {
                this[varName] = streams[1].trim();
                detect.success this[varName];
                return true;
            } else if($1 == "CFLAGS") {
                this[varName] = false;
                // (Via Apple Open Source:) IRIX 6.2 and later require cc -n32
                var host_os = detect.os();
                if(
                    wildcard.match("irix6.[2-9]*", host_os)
                    || wildcard.match("irix6.1[0-9]*", host_os)
                    || wildcard.match("irix[7-9].*", host_os)
                    || wildcard.match("irix[1-9][0-9]*", host_os)
                ) {
                    if(!detect.tool("gcc")) {
                        @ac_cv_sys_largefile_CFLAGS = "-n32";
                        IceTea.GlobalSettings.native.flags[]
                            = @ac_cv_sys_largefile_CFLAGS;
                        detect.success "(Assuming) ${@ac_cv_sys_largefile_CFLAGS}"
                        return true;
                    } else {
                        @ac_cv_sys_largefile_CFLAGS = false;
                        detect.fail "No."
                        return false;
                    }
                }
            } else {
                detect.fail "No."
                this[varName] = false;
                return false;
            }
        } else {
            detect.fail "Unable to run test!"
            exit(1);
        }
    },
    AC_SYS_LARGEFILE_SPACE_APPEND: function(
        $1, // Flag name (LDFLAGS, CFLAGS, ...)
        $2  // Existing flags - variable name
    ) {
        var varName = "lfs_${$1}_called";
        if(varName in this) return;
        else this[varName] = true;
        // Convert the flag name into something IceTea knows.
        var settingsKey, settingsName;
        switch($1) {
            case "LDFLAGS":
                settingsKey = "LINK";
                settingsName = "flags";
            break;
            case "CFLAGS":
            case "CPPFLAGS": // FIXME: This is prune to cause issues on Windows! I need a fix.
                settingsKey = "native";
                settingsName = "flags";
            break;
        }
        if(wildcard.match("?*", $2)) {
            if(__.isNull(this[$1]) /*The original checks empty string, we check NULL*/) {
                IceTea.GlobalSettings[settingsKey][settingsName][]
                    = $2;
            } else {
                IceTea.GlobalSettings[settingsKey][settingsName][]
                    = [this[$1], $2].join(" ");
            }
        }
    },
    AC_SYS_LARGEFILE_MACRO_VALUE: function(
        $1, // What are we checking for? Macro
        $2, // Variable name. In our case, @-variable
        //$3, // Originally a description for the defined macro. We dont have that yet.
        $4  // Callback
    ) {
        detect.line "Checking for ${$1}";
        if(detect.truthyCache($2)) {
            detect.success "${detect.cache[$2]} (Cached)";
            return true;
        }
        this[$2] = false;
        $4.call(this);
        for(var n,ac_flag in ac_cv_sys_largefile_CFLAGS.split("")) {
            if(wildcard.match("-D${$1}", ac_flag)) {
                this[$2] = 1;
            } else if(wildcard.match("-D${$1}=*", ac_flag)) {
                // With a simple regex, I could've shortened this...
                var spawned, exitCode, streams
                    = $("expr \" ${ac_flag}\" : '[^=]*=\(.*\)'");
                if(spawned && exitCode == 0) {
                    this[$2] = streams[1].trim();
                } else {
                    this[$2] = false;
                }
            }
        }
        if(this[$2] != false) {
            detect.defineUnquoted($1, this[$2]);
            detect.success "${this[$2]}"
            detect.cache($2, this[$2])
            return true;
        } else {
            detect.fail "No."
            return false;
        }
    },
    AC_SYS_LARGEFILE: function() {
        if(detect.enabled("largefile")) {
            var found, getconf = detect.tool("getconf");
            found && detect.define("GETCONF", getconf);
            for(var n,str in ["CFLAGS","LDFLAGS","LIBS"]) {
                @AC_SYS_LARGEFILE_FLAGS(str);
            }
            for(var n,ac_flag in @ac_cv_sys_largefile_CFLAGS.split(" ")) {
                if(
                    wildcard.match("-D_FILE_OFFSET_BITS=*", ac_flag)
                    || wildcard.match("-D_LARGEFILE_SOURCE", ac_flag)
                    || wildcard.match("-D_LARGE_FILES", ac_flag)
                    || wildcard.match("-D_LARGE_FILES=*", ac_flag)
                ) {
                    // noop.
                } else if(
                    wildcard.match("-D?*", ac_flag)
                    || wildcard.match("-I?*", ac_flag)
                ) {
                    // In this case, CPP stands for C Pre Processor.
                    // In IceTea: settings.native.defines
                    @AC_SYS_LARGEFILE_SPACE_APPEND("CPPFLAGS", ac_flag);
                } else {
                    @AC_SYS_LARGEFILE_SPACE_APPEND("CFLAGS", ac_flag);
                }
            }
            @AC_SYS_LARGEFILE_SPACE_APPEND("LDFLAGS", @ac_cv_sys_largefile_LDFLAGS);
            @AC_SYS_LARGEFILE_SPACE_APPEND("LIBS", @ac_cv_sys_largefile_LIBS);
            @AC_SYS_LARGEFILE_MACRO_VALUE(
                "_FILE_OFFSET_BITS",
                "ac_cv_sys_file_offset_bits",
                //"Number of bits in a file offset, on hosts where this is settable.",
                function() {
                    var host_os = detect.os();
                    if( // HP-UX 10.20 and later
                        wildcard.match("hpux10.[2-9][0-9]*", host_os)
                        || wildcard.match("hpux1[1-9]*", host_os)
                        || wildcard.match("hpux[2-9][0-9]*", host_os)
                    ) {
                        @ac_cv_sys_file_offset_bits = 64;
                    }
                }
            );
            @AC_SYS_LARGEFILE_MACRO_VALUE(
                "_LARGEFILE_SOURCE",
                "ac_cv_sys_largefile_source",
                //"Define to make fseeko etc. visible, on some hosts.",
                function() {
                    var host_os = detect.os();
                    if( // HP-UX 10.20 and later
                        wildcard.match("hpux10.[2-9][0-9]*", host_os)
                        || wildcard.match("hpux1[1-9]*", host_os)
                        || wildcard.match("hpux[2-9][0-9]*", host_os)
                    ) {
                        @ac_cv_sys_largefile_source = 1;
                    }
                }
            );
            @AC_SYS_LARGEFILE_MACRO_VALUE(
                "_LARGE_FILES",
                "ac_cv_sys_large_files",
                // "Define for large files, on AIX-style hosts.",
                function() {
                    var host_os = detect.os();
                    if( // AIX 4.2 and later
                        wildcard.match("aix4.[2-9]*", host_os)
                        || wildcard.match("aix4.1[0-9]*", host_os)
                        || wildcard.match("aix[5-9].*", host_os)
                        || wildcard.match("aix[1-9][0-9]*", host_os)
                    ) {
                        @ac_cv_sys_large_files = 1;
                    }
                }
            );
        }
    },

    AC_C_INLINE: function() {
        // ... Check for C-inlining?
        // Uh, later. FIXME.
    },


}
