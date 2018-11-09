#include <string>

#include "slre.h"
#include "os-icetea.h"
#include "InternalIceTeaPlugin.h"

using namespace std;

/**
 * @author: Jan P. B.
 * Purpose: Count parentheses in a string.
 * Used to count capture groups for SLRE - and as string practice for Jan. :)
 *
 * @param {char*} c   : Buffer
 * @param {int}   len : Buffer length
 * @return {int} Number of parenthesis, -1 on error.
 *
 * @NOTE: Might want to comment out the printf()s, or throw some kind of exception...
 */
#define COUNTBRACKETS_MORE_OP -1
#define COUNTBRACKETS_MORE_CL -2
int countbrackets(const char* c, int len) {
    char prevchar=' ';
    int braop=0;
    int bracl=0;

    for(int i=0; i<len; i++) {
        if (c[i]=='(' && prevchar!='\\') {
            braop++;
        } else if (c[i]==')' && prevchar!='\\') {
            bracl++;
        }
        prevchar = c[i];
    }

    if (braop==bracl) {
        return braop;
    } else {
        if (braop>bracl)
            //printf("es gibt mehr ( wie )");
            return COUNTBRACKETS_MORE_OP;
        else {
            //printf("es gibt mehr ) wie (");
            return COUNTBRACKETS_MORE_CL;
        }
    }
    // Silencing "Function reaching end of control"
    return -3;
}


class IceTeaSLRE: public IceTeaPlugin {
    static OS_FUNC(match) {
        // SLRE.match(RegExp, Buffer, Captures, Flags)
        // @returns Array
        if(!os->isString(-params+0) && os->isString(-params+1)) {
            os->setException("SLRE::match : Expected argument 1 and 2 to be string.");
            return 0;
        }
        const char* pattern = os->toString(-params+0).toChar();
        const char* buffer = os->toString(-params+1).toChar();
        int pat_len = os->getLen(-params+0);
        int buff_len = os->getLen(-params+1);
        int pars = countbrackets(pattern, pat_len);
        int flags = 0;

        // Optional parameters
        if(params > 2) {
            // 3 or more args given.
            // Second arg is the overriding of capture groups.
            // Might be useful for whole-string stuff?
            pars = os->toNumber(-params+2);
            if(params > 4) {
                // More than 2 params, the third is flags.
                // We export the flags as numbers, so expect only a number.
                if(os->isNumber(-params+3)) {
                    flags = os->toNumber(-params+3);
                }
            }
        }

        // Capture us some results.
        // Also, this is a workaround for MSVC C2057
        // See: http://www.fredosaurus.com/notes-cpp/newdelete/50dynamalloc.html
        typedef struct slre_cap SLRE;
        SLRE* caps = new SLRE[pars];

        int rt = slre_match(
            pattern, buffer, buff_len,
            caps, pars, flags
        );

        // Prepare result
        switch(rt) {
            case SLRE_NO_MATCH:
                os->pushNumber(rt);
                os->newArray();
                delete[] caps;
                return 2;

            case SLRE_UNEXPECTED_QUANTIFIER:
                os->setException("SLRE: Unexpected quantifier");
                delete[] caps;
                return 0;

            case SLRE_UNBALANCED_BRACKETS:
                os->setException("SLRE: Unbalanced brackets");
                delete[] caps;
                return 0;

            case SLRE_INTERNAL_ERROR:
                os->setException("SLRE: Internal error");
                delete[] caps;
                return 0;

            case SLRE_INVALID_CHARACTER_SET:
                os->setException("SLRE: Invalid character set");
                delete[] caps;
                return 0;

            case SLRE_INVALID_METACHARACTER:
                os->setException("SLRE: Invalid metacharacter");
                delete[] caps;
                return 0;

            case SLRE_CAPS_ARRAY_TOO_SMALL:
                os->setException("SLRE: (In native code:) Capture array too small");
                delete[] caps;
                return 0;

            case SLRE_TOO_MANY_BRANCHES:
                os->setException("SLRE: Too many branches");
                delete[] caps;
                return 0;

            case SLRE_TOO_MANY_BRACKETS:
                os->setException("SLRE: Too many brackets");
                delete[] caps;
                return 0;
        }

        // If we got here, then we are totally safe
        // to return the contents of caps.
        os->pushNumber(rt);
        os->newArray();
        for(int i=0; i<pars; i++) {
            os->pushString(caps[i].ptr, caps[i].len);
            os->addProperty(-2);
        }
        delete[] caps;
        return 2;
    }

    bool configure(IceTea* it) {
        it->getModule("SLRE");
        it->pushCFunction(match);
        it->setProperty(-2, "match");
        return true;
    }
    string getName() {
        return "SLRE";
    }
    string getDescription() {
        return  "Basic Regular Expression functionallity, provided via:\n"
                "- https://github.com/cesanta/slre";
    }
};
ICETEA_INTERNAL_MODULE(IceTeaSLRE);
