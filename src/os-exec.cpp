#include "os-exec.h" // Contains everything.

using namespace std;
using namespace ObjectScript;
using namespace stlplus;

// This is the actual command function.
CommandResult it_cmd(const string& _cmd, vector<string>& replaces) {
    string cmd = _cmd;
    CommandResult res;

    // Now to convert it into a real command string.
    if(!replaces.empty()) {
        stringstream strm;
        vector<string>::iterator it=replaces.begin();
        for (int i=0; i < cmd.length(); i++) {
            if(cmd[i] != '?') {
                strm << cmd[i];
            } else {
                // Replace accordingly now.
                if(it != replaces.end()) {
                    strm << "\"" << *it << "\"";
                    it++;
                }
            }
        }
        cmd = strm.str();
    }

    // Doing a last fixup...
    string program;
    string args;
    if (cmd.find(' ') != string::npos) {
        program = cmd.substr(0, cmd.find(' '));
        args    = cmd.substr(cmd.find(' ')+1);
    } else {
        program = cmd;
        args = "";
    }

    // Running the actual command.
    async_subprocess runner;
    runner.spawn(program, args, false, true, true);
    string stdout;
    string stderr;
    while(runner.tick()) {
        int readOut = runner.read_stdout(stdout);
        int readErr = runner.read_stderr(stderr);
    }
    // Prepare return
    res.exit_code = runner.error_number();
    res.streams[1] = stdout;
    res.streams[2] = stderr;

    return res;
}

OS_FUNC(os_exec) {
    // Syntax: [int, array[3]] $(command [, replacements])
    string         cmd = os->toString(-params+0).toChar();
    vector<string> replaces;

    if(params >= 2) {
        // We have to extract the replaces.
        // We start at 1, because we go down the stack.
        for(int i=1; i<params; i++) {
            replaces.push_back( os->toString(-params+i).toChar() );
        }
    }

    CommandResult res = it_cmd(cmd, replaces);

    os->pushNumber(res.exit_code);
    os->newArray();
    os->pushString(""); // STDIN not supported, yet.
    os->addProperty(-2);
    os->pushString( res.streams[1].c_str() );
    os->addProperty(-2);
    os->pushString( res.streams[2].c_str() );
    os->addProperty(-2);

    return 2;
}
