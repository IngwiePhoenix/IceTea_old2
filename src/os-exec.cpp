#include <string>
#include <utility>
#include <iostream>
#include "IceTea.h"
#include "os-exec.h"
#include "subprocesses.hpp"
#include "InternalIceTeaPlugin.h"

using namespace std;
using namespace ObjectScript;
using namespace stlplus;

#define GET_UNDERLYING_PROCESS()                    \
    int _this = os->getAbsoluteOffs(-params-1);     \
    os->getProperty(_this, "ptr");                  \
    ProcessInstance* proc =                         \
        reinterpret_cast<ProcessInstance*>(         \
            os->toUserdata(0)                       \
        );                                          \
    os->pop();                                      \
    if(proc->isSync())                              \
        SyncProcess* p = proc->getSyncProcess();    \
    else                                            \
        AsyncProcess* p = proc->getAsyncProcess();

// Variable, Method, Args
#define CALL_P_VMA(var, method, ...) \
    if(proc->isSync()) {                            \
        SyncProcess* p = proc->getSyncProcess();    \
        var = p->method( __VA_ARGS__ );             \
    } else {                                        \
        AsyncProcess* p = proc->getAsyncProcess();  \
        var = p->method( __VA_ARGS__ );             \
    }

// Variable, Method
#define CALL_P_VM(var, method) \
    if(proc->isSync()) {                            \
        var = proc->getSyncProcess()->method();     \
    } else {                                        \
        var = proc->getAsyncProcess()->method();    \
    }

// Method
#define CALL_P_M(method) \
    if(proc->isSync()) {                            \
        proc->getSyncProcess()->method();           \
    } else {                                        \
        proc->getAsyncProcess()->method();          \
    }

// Method, Args
#define CALL_P_MA(method, ...) \
    if(proc->isSync()) {                            \
        SyncProcess* p = proc->getSyncProcess();    \
        p->method( __VA_ARGS__ );                   \
    } else {                                        \
        AsyncProcess* p = proc->getAsyncProcess();  \
        p->method( __VA_ARGS__ );                   \
    }


// And here begins a lot of Copy-Pastery..
bool SyncProcess::callback() {
    for(;;) {
        int outRead = this->read_stdout(this->stdout);
        int errRead = this->read_stderr(this->stderr);
        if(outRead == -1 && errRead == -1) {
            break;
        }
    }
    return !error();
}
string SyncProcess::getStdout() { return stdout; }
string SyncProcess::getStderr() { return stderr; }

bool AsyncProcess::callback() {
    int outRead = this->read_stdout(this->stdout);
    int errRead = this->read_stderr(this->stderr);
    if(outRead != -1 && errRead != -1) {
        // Eat the output streams first.
        return true;
    } else if(error()) {
        // THEN check for errors.
        return error();
    } else {
        return false;
    }
}
string AsyncProcess::getStdout() { return stdout; }
string AsyncProcess::getStderr() { return stderr; }


class ProcessInstance {
private:
    void* inst;
    bool _isSync;
public:
    ProcessInstance(SyncProcess* p) {
        this->_isSync = true;
        this->inst = (void*)p;
    }
    ProcessInstance(AsyncProcess* p) {
        this->_isSync = false;
        this->inst = (void*)p;
    }
    ~ProcessInstance() {
        if(isSync()) {
            SyncProcess* p = getSyncProcess();
            delete p;
        } else {
            AsyncProcess* p = getAsyncProcess();
            delete p;
        }
    }
    bool isSync() {
        return this->_isSync;
    }
    SyncProcess* getSyncProcess() {
        return reinterpret_cast<SyncProcess*>(this->inst);
    }
    AsyncProcess* getAsyncProcess() {
        return reinterpret_cast<AsyncProcess*>(this->inst);
    }
};

struct OSSubProcess {
    static OS_FUNC(__construct) {
        int _this = os->getAbsoluteOffs(-params-1);
        if(os->getTypeStr(-params+0) == "object") {
            // SubProcess({
            //     async: true or false
            // })
            os->getProperty(-params+0, "async");
            bool isSync = !os->toBool(-params+0);
            ProcessInstance* pi;
            if(isSync) {
                pi = new ProcessInstance(new SyncProcess);
            } else {
                pi = new ProcessInstance(new AsyncProcess);
            }
            os->pushUserPointer((void*)pi);
            os->setProperty(_this, "ptr");
            return 0;
        } else {
            os->setException("SubProcess: Expected parameter 1 to be an object.");
        }
        return 0;
    }
    static OS_FUNC(__destruct) {
        GET_UNDERLYING_PROCESS()
        delete proc;
        return 0;
    }
    static OS_FUNC(set_env) {
        GET_UNDERLYING_PROCESS()
        string typeStr1 = os->getTypeStr(-params+0).toChar();
        string typeStr2 = os->getTypeStr(-params+1).toChar();
        if(typeStr1 == "string" && typeStr2 == "string") {
            // setEnv(String, String)
            string key = os->toString(-params+0).toChar();
            string val = os->toString(-params+1).toChar();
            CALL_P_MA(add_variable, key, val)
            return 0;
        } else {
            os->setException("SubProcess.set_env: Expected two strings.");
        }
        return 0;
    }
    static OS_FUNC(del_env) {
        GET_UNDERLYING_PROCESS()
        if(os->isString(-params+0)) {
            string name = os->toString(-params+0).toChar();
            bool rt;
            CALL_P_VMA(rt, remove_variable, name)
            os->pushBool(rt);
            return 1;
        } else {
            os->setException("SubProcess.del_env: Expected string.");
        }
        return 0;
    }
    static OS_FUNC(get_env) {
        GET_UNDERLYING_PROCESS()
        env_vector env;
        CALL_P_VM(env, get_variables)
        int size = env.size();
        os->newObject();
        for(int i=0; i<size; i++) {
            pair<string,string> entry = env[i];
            os->pushString(entry.first.c_str());
            os->setProperty(-2, entry.second.c_str());
        }
        os->pushNumber(size);
        return 2;
    }
    static OS_FUNC(write_stdin) {
        GET_UNDERLYING_PROCESS()
        if(os->isString(-params+0)) {
            string buf = os->toString(-params+0).toChar();
            int lenWritten;
            CALL_P_VMA(lenWritten, write_stdin, buf)
            os->pushString(buf.c_str());
            os->pushNumber(lenWritten);
            return 2;
        } else {
            os->setException("SubProcess.write_stdin: Parameter 1 expected to be string.");
        }
        return 0;
    }
    static OS_FUNC(close_stdin) {
        GET_UNDERLYING_PROCESS()
        CALL_P_M(close_stdin)
        return 0;
    }
    static OS_FUNC(close_stdout) {
        GET_UNDERLYING_PROCESS()
        CALL_P_M(close_stdout)
        return 0;
    }
    static OS_FUNC(close_stderr) {
        GET_UNDERLYING_PROCESS()
        CALL_P_M(close_stderr)
        return 0;
    }
    static OS_FUNC(stdout) {
        GET_UNDERLYING_PROCESS()
        string out;
        CALL_P_VM(out, getStdout)
        os->pushString(out.c_str());
        return 1;
    }
    static OS_FUNC(stderr) {
        GET_UNDERLYING_PROCESS()
        string out;
        CALL_P_VM(out, getStderr)
        os->pushString(out.c_str());
        return 1;
    }
    static OS_FUNC(error) {
        GET_UNDERLYING_PROCESS()
        bool rt;
        CALL_P_VM(rt, error)
        os->pushBool(rt);
        return 1;
    }
    static OS_FUNC(error_number) {
        GET_UNDERLYING_PROCESS()
        int rt;
        CALL_P_VM(rt, error_number)
        os->pushNumber(rt);
        return 1;
    }
    static OS_FUNC(error_text) {
        GET_UNDERLYING_PROCESS()
        string txt;
        CALL_P_VM(txt, error_text)
        os->pushString(txt.c_str());
        return 1;
    }
    static OS_FUNC(exit_code) {
        GET_UNDERLYING_PROCESS()
        int rt;
        CALL_P_VM(rt, exit_status)
        os->pushNumber(rt);
        return 1;
    }
    static OS_FUNC(kill) {
        GET_UNDERLYING_PROCESS()
        bool rt;
        CALL_P_VM(rt, kill)
        os->pushBool(rt);
        return 1;
    }
    static OS_FUNC(execute) {
        IceTea* it = (IceTea*)os;
        GET_UNDERLYING_PROCESS()
        if(os->isString(-params+0)) {
            string cmd = os->toString(-params+0).toChar();
            bool use_stdin, use_stdout, use_stderr;
            use_stdin = it->isBool(-params+1) ? os->toBool(-params+1) : false;
            use_stdout = it->isBool(-params+2) ? os->toBool(-params+2) : true;
            use_stderr = it->isBool(-params+3) ? os->toBool(-params+3) : true;
            bool spawned;
            CALL_P_VMA(spawned, spawn, cmd, use_stdin, use_stdout, use_stderr)
            os->pushBool(spawned);
            return 1;
        } else {
            string msg = "SubProcess.execute: Parameter 1 is expected to be a string.";
            msg.append(" Got: ");
            msg.append(os->getTypeStr(-params+0).toChar());
            os->setException(msg.c_str());
        }
        return 0;
    }
    static OS_FUNC(tick) {
        GET_UNDERLYING_PROCESS()
        if(proc->isSync()) {
            os->pushBool(false);
        } else {
            bool rt;
            rt = proc->getAsyncProcess()->tick();
            os->pushBool(rt);
        }
        return 1;
    }
};

class IceTeaSubProcess: public IceTeaPlugin {
public:
    bool configure(IceTea* it) {
        #define _M(name) {OS_TEXT(#name), OSSubProcess::name}
        OS::FuncDef methods[] = {
            _M(__construct),
            _M(__destruct),
            _M(set_env),
            _M(del_env),
            _M(get_env),
            _M(write_stdin),
            _M(close_stdin),
            _M(close_stdout),
            _M(close_stderr),
            _M(stdout),
            _M(stderr),
            _M(error),
            _M(error_number),
            _M(error_text),
            _M(exit_code),
            _M(kill),
            _M(execute),
            _M(tick),
            //{OS_TEXT("__callinstance"), OSSubProcess::execute},
            {}
        };

        it->getGlobalObject("SubProcess");
        it->setFuncs(methods);

        return true;
    }
    string getName() {
        return "SubProcess";
    }
    string getDescription() {
        return "Powered by STLPlus' SubProcess flavors, this plugin let's you run commands, sync or async.";
    }
};
ICETEA_INTERNAL_MODULE(IceTeaSubProcess);
