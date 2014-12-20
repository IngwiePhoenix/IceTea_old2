#include "os-exec.h" // Contains everything.
#include <stdlib.h>

using namespace std;
using namespace ObjectScript;
using namespace stlplus;

bool IceTeaProcess::callback() {
    while(read_stdout(stdout) != -1) continue;
    while(read_stderr(stderr) != -1) continue;
    if(error()) {
        return false;
    } else {
        return true;
    }
}

// This is the actual command function.
CommandResult it_cmd(const string _cmd, vector<string> replaces) {
    CommandResult res;
    string cmd(_cmd);

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

    // Running the actual command.
    IceTeaProcess* runner = new IceTeaProcess();
    res.spawned = runner->spawn(cmd, false, true, true);
    // Prepare return
    res.exit_code = runner->exit_status();
    res.streams[1] = runner->stdout;
    res.streams[2] = runner->stderr;
    res.p = runner;

    return res;
}

OS_FUNC(os_exec) {
    // Syntax: [int, array[3]] $(command [, replacements])
    string         cmd = os->toString(-params+0).toChar();
    vector<string> replaces;

    if(params >= 2) {
        cout << "Args: " << params << endl;
        // We have to extract the replaces.
        // We start at 1, because we go down the stack.
        for(int i=1; i<params; i++) {
            replaces.push_back( os->toString(-params+i).toChar() );
        }
    }

    CommandResult res = it_cmd(cmd, replaces);

    os->pushBool(res.spawned);
    os->pushNumber(res.exit_code);
    os->newArray();
    os->pushString(""); // STDIN not supported, yet.
    os->addProperty(-2);
    os->pushString( res.streams[1].c_str() );
    os->addProperty(-2);
    os->pushString( res.streams[2].c_str() );
    os->addProperty(-2);

    return 3;
}

OS_FUNC(os_system) {
    // Flat wrapper around the system call.
    if(params < 1 || params > 1) {
        os->setException("System only expects one parameter, and one only.");
        return 0;
    }
    int res = system( os->toString(-params+0).toChar() );
    os->pushNumber(res);
    return 1;
}
