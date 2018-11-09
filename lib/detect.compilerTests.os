// To be merged with detect.os!
// MSVC Stuff: https://msdn.microsoft.com/library/hh567368(v=vs.110).aspx

detect.__add({
    MSVCVersion: function() {
        var src = [
            "int main() {",
            "   #ifdef _MSC_VER",
            "       return (int)_MSC_VER",
            "   #else",
            "       return -1",
            "   #endif"
            "}"
        ].join("\n");
    },

    isMSVC: {|| return @MSVCVersion() != -1 },

    MSVCString: function(version) {

    }
});
