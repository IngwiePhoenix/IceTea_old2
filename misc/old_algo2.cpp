/**
    @brief Create the actual tasks

    This function contains a LOT of logic and represents the heartpiece of the
    entire project. It looks up the rules and compares a file and other information
    against its results. The finalized task is added to the @ref TaskQueue

    This function also calls itself recursively and passes input trough referenced
    objects.

    @param rule The rule to be used in the current iteration.
    @param file The file to be used for the current iteration.
    @param target The target to iterate on.
    @param[out] input The input to be returned to the parent call
    @param[in,out] parent The @ref Task object to be passed as a parent.
    @param[out] success A bool that will be set to true if the iteration was indeed a success. False if otherwise.
    @param finalRule a boolean that is used to tell the __first__ iteration about it.
*/
void __makeTasks(
    string rule, string file, string target,
    string& input, Task*& parent, bool& success, bool finalRule=false
) {
    if(!ruleExists(rule)) {
        cerr << "[IceTea]: Rule \"" << rule << "\" does not exist." << endl;
        return;
    }

    Value::Object* targetObj = (Value::Object*)(*targets)[target];
    Value::Object* ruleObj = (Value::Object*)(*rules)[rule];

    /*
        In most cases, the input file won't match directly.
        However, sometimes it just so happens to be right.
        Example: minifying lots of .css file into .min.css
        But for native code, that wont be the case.
        Example: file=myfile.cpp and the rule produces mytarget.exe
        Therefore, we do a check if we currently are on the right target or not.
    */
    Value::Array* accepts = (Value::Array*)(*ruleObj)["accepts"];
    bool isBaseTarget=false; // Base target = the input file IS the right one. No search needed.
    for(int i=0; i<accepts->length(); i++) {
        Value::String* acceptPattern = (Value::String*)(*accepts)[i];
        if(wildcard( (*acceptPattern), file )) {
            isBaseTarget = true;
            break;
        }
        delete acceptPattern;
    }

    // In any case, we want to create the expected value. Its easy.
    string myExpected = estimateRuleOutput(rule, target, file);

    static map<string, bool> rulesChecked;
    #define RUN_INIT \
        if(rulesChecked.find(rule) == rulesChecked.end()) { \
            os->pushValueById(ruleObj->valueID); \
            os->pushString("prepare"); \
            rulesChecked[rule]=true; \
            if(os->in(-1, -2)) { \
                os->getProperty(-2); \
                if(os->isFunction()) { \
                    os->pushValueById(ruleObj->valueID); \
                    os->callFT(0,1); \
                    if(os->isType(OS_VALUE_TYPE_BOOL) && os->toBool() == false) { \
                        success = false; \
                        return; \
                    } \
                } \
            } \
        }

    if(isBaseTarget) {
        RUN_INIT
        if(finalRule) {
            // The base target is ALSO the only...all we gotta do is return accordingly.
            input = file;
        } else {
            // Do not even go much further, add the target accordingly.
            Value::String* t_rd = (Value::String*)(*ruleObj)["display"];
            string display = (*t_rd);
            delete t_rd;
            Task* t = new Task;
            t->ruleName = rule;
            t->targetName = target;
            t->ruleDisplay = display;
            t->input.push_back(file);
            t->output = myExpected;

            // Find out what type the rule is. A function, or an array of commands?
            os->pushValueById(ruleObj->valueID);
            os->pushString(runScheme.c_str());
            os->getProperty(-2);
            //tout << runScheme << "(" << display << "): " << os->getTypeStr() << endl; print();
            if(os->getTypeStr() == "array") {
                t->type=COMMAND;
                int objlen = os->getLen();
                for(int k=0; k<objlen; k++) {
                    os->pushNumber(k);
                    os->getProperty();
                    t->commands.push_back( os->toString().toChar() );
                }
            } else if(os->getTypeStr() == "function") {
                t->type=SCRIPT;
            } else {
                if(cli->check("-w")) {
                    // Whipe mode. Replace the emptyness right there with an actual thing.
                    os->pushValueById(ruleObj->valueID);
                    os->pushCFunction(os_target_clean, (void*)t);
                    os->setProperty(-2, runScheme.c_str());
                } else {
                    cerr << "[IceTea]: Rule '"<< rule << "' must have it's build property defined as function or array! "
                         << "'" << os->getTypeStr() << "' was given instead." << endl;
                }
            }

            t->target = (Value::Object*)(*targets)[target];

            // Relate it
            t->child = NULL;
            t->parent = parent;
            parent->child = t;

            tout.print();
            tasks->add(t);

            // We send the output of this task back to the parent call.
            // That is the new input.
            input = myExpected;
        }
    } else {
        /*
            This is much more complicated...
            We have the scenario, that the current file doe snot fit.
            So we have to search for the actual rule.
            But in relation to that, the rule we find must also be able to
            output the kind of file that we want. Therefore, we need:

            - The "final" rule's output pattern
            - The actual rule's output pattern
        */
        os->pushValueById(rules->valueID);
        os->getProperty(-1, "keys");
        Value::Array* ruleNames = new Value::Array(os);
        for(int r=0; r<ruleNames->length(); r++) {
            Value::String* ruleName = (Value::String*)(*ruleNames)[r];
            Value::Object* currRule = (Value::Object*)(*rules)[(*ruleName)];

            /*
                We are going to find out, if the rule we are currently onto
                is capable of delivering the file we want.

                    if( the current rule's output pattern IS within the given rule's accept list ) {
                        Then continue.
                    }
            */
            bool outputMatchesAccepted=false;
            Value::Object* currOutputObj = (Value::Object*)(*currRule)["output"];
            Value::String* currOutputPat = (Value::String*)(*currOutputObj)["pattern"];
            for(int e=0; e<accepts->length(); e++) {
                // Entry in the given rule's accept array
                Value::String* currAccepts = (Value::String*)(*accepts)[e];
                if( (*currOutputPat) == (*currAccepts) ) {
                    outputMatchesAccepted = true;
                }
                delete currAccepts;
                if(outputMatchesAccepted) break;
            }
            //delete currOutputObj;
            //delete currOutputPat;

            /*
                So our output pattern might match an input pattern.
                That means we found a "child rule". An example would be:

                    .exe
                        -> .o-stripped (Child)
                            -> .o (Base)

                Or more complicated:

                    .framework (Final)
                        -> .dylib (Child)
                            -> .o-stripped (Child)
                                -> .o (Base)

                Or even...

                    .framework (Final)
                        -> .dylib (Child)
                            -> .a (final of another target)
                                -> .o-stripped (Child)
                                    -> .o (Base)
                            -> .o-stripped (Child)
                                -> .o (Base)

                The final rule is constructed from the search process (Transformer).
                The base rule then occurs, when the input file exactly matches the input pattern,
                given in the "accepts" object property.
                A child rule is, if the file doesnt match the given rule and if its not the final rule.
                At this state, we have to recursively call ourselves.
            */

            if(outputMatchesAccepted) {
                RUN_INIT
                /*
                    Again, the output of the current rule matches our wanted input.
                    Now we have to verify if we found a base rule, or a child rule.
                    That is done by comparing the current file to the accepted pattern
                    of the current rule. If it matches, it is a base rule.
                    In this case, we call ourself with the original file and found rule.
                    Otherwise, we must peer one level deeper and estimate an input.
                    Example: If we were given the executable rule (.exe as output)
                    and have a .cpp file as input, then we would find, that
                    the executable rule wants '.o-stripped' as its input.
                    The strip rule provides the actual output for that, but requires
                    a '.o' file for its input. So we search for a rule that outputs '.o'
                    and recall ourselves with that:

                        File: myfile.cpp
                        Rule: strip

                    But, strip requires '.o''s! What will happen is, that we first
                    re-execute ourself to find a rule that outputs '.o'. In this case,
                    we will actually find a base-rule, and the very above, first
                    check will pass, and we just create a task.
                    That will send back a .o file that we can then add as input
                    to the strip rule.

                    When the strip rule has added it's stuff, it gives back a '.o-stripped'
                    file, and since .exe doesnt need to make its ownt ask, we return that
                    to the main search function.
                */

                // Let's start by finding out if what we found is a base rule.
                bool currentIsBase=false;
                Value::Array* currAccepts = (Value::Array*)(*currRule)["accepts"];
                for(int n=0; n<currAccepts->length(); n++) {
                    Value::String* accPattern = (Value::String*)(*currAccepts)[n];
                    if(wildcard((*accPattern), file)) {
                        currentIsBase = true;
                    }
                    delete accPattern;
                    if(currentIsBase) break;
                }

                string theExpected = estimateRuleOutput((*ruleName), target, file);
                Value::String* t_rd = (Value::String*)(*currRule)["display"];
                string display = (*t_rd);
                string _ruleName = (*ruleName);
                delete t_rd;

                if(currentIsBase) {
                    // The file we have, and the rule we found, make up for a base.
                    // In this case, recall ourself. The above, first check will pass
                    // and it will execute a base rule.
                    string newInput;
                    __makeTasks((*ruleName), file, target, newInput, parent, success);
                    //if(newInput.empty()) continue;
                    input = newInput;
                } else {
                    /*
                        The current rule provides an output that we can use, but we dont have
                        a suitable input for it. I.e.:

                            strip accepts: *.o
                                  outputs: *.o-stripped
                            file         : myfile.cpp

                        So what do we do?

                        We re-run ourself with the current file, then create
                        a new task with the "returned" output.
                        Beyond that, we return the expected output, as well.
                    */
                    Task* t = new Task;
                    string newInput;
                    __makeTasks((*ruleName), file, target, newInput, t, success);

                    // If the newInput we got is empty, then we probably should skip this rule.
                    if(newInput.empty()) {
                        continue;
                    }

                    t->ruleName = _ruleName;
                    t->targetName = target;
                    t->ruleDisplay = display;
                    t->output = theExpected;

                    os->pushValueById(currRule->valueID);
                    os->pushString(runScheme.c_str());
                    os->getProperty(-2);
                    //tout << runScheme << "(" << display << "): " << os->getTypeStr() << endl; print();
                    if(os->getTypeStr() == "array") {
                        t->type=COMMAND;
                        int objlen = os->getLen();
                        for(int k=0; k<objlen; k++) {
                            os->pushNumber(k);
                            os->getProperty();
                            t->commands.push_back( os->toString().toChar() );
                        }
                    } else if(os->getTypeStr() == "function") {
                        t->type=SCRIPT;
                    } else {
                        if(cli->check("-w")) {
                            // Whipe mode. Replace the emptyness right there with an actual thing.
                            os->pushValueById(currRule->valueID);
                            os->pushCFunction(os_target_clean, (void*)t);
                            os->setProperty(-2, runScheme.c_str());
                        } else {
                            cerr << "[IceTea]: Rule '"<< rule << "' must have it's '" << runScheme << "' property defined as function or array! "
                                      << "'" << os->getTypeStr() << "' was given instead." << endl;
                        }
                    }

                    t->target = (Value::Object*)(*targets)[target];
                    parent->child = t;
                    t->parent = parent;
                    t->input.push_back(newInput);
                    tasks->add(t);
                    input = theExpected;
                    return; // Done.
                }
            } else {
                // No matching rule was found.
                //cerr << file << " against " << rule << " resulted in nothing." << endl;
            }

            delete currRule;
            delete ruleName;
        }
        // If we have reached here, NO RULE DID MATCH.
        //cout << file << " seriously didnt match ANYTHING." << endl;
    }
    delete targetObj;
    delete ruleObj;
    delete accepts;
}

/**
    @brief The actual wrapper around @ref __makeTasks

    This function simply returns what the actual @ref __makeTasks was giving back.
    This is also called the 0th iteration. It receives its output as the last instance.
*/
string makeTasks(string rule, string file, string target, Task*& parent, bool& successor) {
    string tmp;
    // The first call ALWAYS results in the actual thing.
    __makeTasks(rule, file, target, tmp, parent, successor, true);
    return tmp;
}
