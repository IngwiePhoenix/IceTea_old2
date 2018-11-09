void __makeTasks__prev(
    string rule, string file, string target,
    TaskQueue* queue, OS* os,
    string& input, bool finalRule=false
) {
    os->pushString(rule.c_str());
    os->pushValueById(rules->valueID);
    if(!os->in()) {
        cerr << "[IceTea]: " << rule << " does not exist.";
        return;
    }
    os->pushString(rule.c_str());
    os->getProperty();
    Value::Object myRule(os, os->getAbsoluteOffs(-1));
    // Now get the two most important properties
    Value::Array* accepts = (Value::Array*)myRule["accepts"];
    Value::Object* outputs = (Value::Object*)myRule["output"];

    // Now create some strings.
    Value::String* o_myPattern = (Value::String*)(*outputs)["pattern"];
    Value::String* o_myExpected = (Value::String*)(*outputs)["expected"];
    string myPattern = (*o_myPattern);
    string myExpected = (*o_myExpected);

    // Clean up the heap
    delete outputs;
    delete o_myPattern;
    delete o_myExpected;

    // Replace stuff...
    string basename = PathToFileName(file.c_str());
    string ext = StripFileExtension(basename);
    ReplaceStringInPlace(myExpected, "%t", target);
    ReplaceStringInPlace(myExpected, "%b", basename);
    ReplaceStringInPlace(myExpected, "%e", ext);
    ReplaceStringInPlace(myExpected, "%f", file);

    //cout <<rule<< " Working: " << file << " --> " << myExpected << endl;

    // Does the file we are processing actually HAVE a rule it can be run with, at all?
    os->pushValueById(rules->valueID);
    os->getProperty(-1, "keys");
    Value::Array rkeys(os);
    bool hasARule=false;
    for(int r=0; r<rkeys.length(); r++) {
        Value::String* ruleName = (Value::String*)rkeys[r];
        Value::Object* val = (Value::Object*)(*rules)[(*ruleName)];
        Value::Array* rAccepts = (Value::Array*)(*val)["accepts"];
        for(int o=0; o<rAccepts->length(); o++) {
            Value::String* rPat = (Value::String*)(*rAccepts)[o];
            if(WildcardMatch(file.c_str(), (*rPat))) {
                hasARule=true;
                break;
            }
        }
        delete ruleName;
        delete val;
    }

    if(!hasARule) {
        cerr << "[IceTea]: It appears, that "<< file << " has no rule it can be ran with!"
             << " The file will be added verbatim. Be aware of this." << endl;
        return;
    }

    // Does the file we have, already match any pattern in the accept array?
    bool fits=false;
    for(int i=0; i<accepts->length(); i++) {
        Value::String* o_pat = (Value::String*)(*accepts)[i];
        string pat = (*o_pat);
        //cout << "Comparing with: " << pat << endl;
        if(WildcardMatch(file.c_str(), pat.c_str())) {
            fits=true;
            break;
        }
        delete o_pat;
    }
    if(!fits) {
        //cout << "Finding new processor rule..." << endl;
        // Our file is not accepted by the rule.
        // So let's one that works and process it with that.
        // Problem is, a rule may have multiple accepts...
        // so we have to go over all of them and look for a rule.
        for(int j=0; j<accepts->length(); j++) {
            Value::String* o_pat = (Value::String*)(*accepts)[j];
            string pat = (*o_pat);
            // Now we search up for the rule that HAS the pattern as its output pattern.
            // that also is why you only define an output pattern as STRING.
            os->pushValueById(rules->valueID);
            os->getProperty(-1, "keys"); // triggers the getKeys method.
            Value::Array keys(os);
            for(int e=0; e<keys.length(); e++) {
                Value::String* key = (Value::String*)keys[e];
                Value::Object* val = (Value::Object*)(*rules)[(*key)];
                Value::Object* v_output = (Value::Object*)(*val)["output"];
                Value::String* v_pat = (Value::String*)(*v_output)["pattern"];
                string sv_pat = (*v_pat);

                if( pat == sv_pat ) {
                    //cout << "Comparing " << pat << " to " << sv_pat << " as " << (*key) << endl;
                    // The current rule accepts input, from the found rule.
                    // But does the found rule actually support our file?
                    string n_rule = (*key);
                    string t_input;
                    __makeTasks(n_rule, file, target, queue, os, t_input);
                    string inputFile = (!t_input.empty() ? t_input : file);

                    // We are going to compare output pattern to accept pattern...
                    // But we also must see if our file actually fits as accepted, for the rule we want the output
                    //cout << "Testing rule: " << n_rule << " with: " << file << endl;
                    cout << n_rule << ": " << file << ", " << inputFile << ", " << myExpected << endl;

                    Value::Array* t_accepts = (Value::Array*)(*val)["accepts"];
                    bool isAccepted=false;
                    for(int p=0; p<t_accepts->length(); p++) {
                        Value::String* t_pat = (Value::String*)(*t_accepts)[p];
                        cout << "Trying: " << (*t_pat) << endl;
                        if(
                            WildcardMatch(inputFile.c_str(), (*t_pat))
                        ) { cout << "hit!" << endl;
                            isAccepted=true;
                            break;
                        }
                        delete t_pat;
                    }

                    delete t_accepts;
                    delete key;
                    delete val;
                    delete v_output;
                    delete v_pat;

                    // Determine the "output"
                    if(!finalRule) input = myExpected;
                    else           input = inputFile;

                    // But we have to add this as a rule, since it was called that way.
                    if(isAccepted) {
                        if(!finalRule) {
                            Task* t = new Task;
                            t->ruleName = rule;
                            t->targetName = target;
                            t->input.push_back(inputFile);
                            t->output = myExpected;
                            // Find out what type the rule is. A function, or an array of commands?
                            os->pushValueById(myRule.valueID);
                            os->getProperty(-1, "build");
                            if(os->getTypeStr() == "array") {
                                t->type=COMMAND;
                                for(int k=0; k<os->getLen(); k++) {
                                    os->pushNumber(k);
                                    os->getProperty();
                                    t->commands.push_back( os->popString().toChar() );
                                }
                            } else if(os->getTypeStr() == "function") {
                                t->type=SCRIPT;
                                os->pushValueById(myRule.valueID);
                                Value::Object* theRule = new Value::Object(os);
                                t->onBuild = new Value::Method(os, theRule, "build");
                            } else {
                                cerr << "[IceTea]: Rule '"<< rule << "' must have it's build property defined as function or array! "
                                     << "'" << os->getTypeStr() << "' was given instead." << endl;
                            }
                            t->target = (Value::Object*)(*targets)[target];
                            queue->add(t);
                            return; // Nothing to return here. But we end the function here.
                        }
                    }
                }
            }
        }
    } else {
        cout << rule << " Input: " << file << endl;
        // file fits with out accepted entries! So we create a task and put it in.
        Task* t = new Task;
        t->ruleName = rule;
        t->targetName = target;
        t->input.push_back(file);
        t->output = myExpected;

        // Find out what type the rule is. A function, or an array of commands?
        os->pushValueById(myRule.valueID);
        os->getProperty(-1, "build");
        if(os->getTypeStr() == "array") {
            t->type=COMMAND;
            for(int k=0; k<os->getLen(); k++) {
                os->pushNumber(k);
                os->getProperty();
                t->commands.push_back( os->popString().toChar() );
            }
        } else if(os->getTypeStr() == "function") {
            t->type=SCRIPT;
            os->pushValueById(myRule.valueID);
            Value::Object* theRule = new Value::Object(os);
            t->onBuild = new Value::Method(os, theRule, "build");
        } else {
            cerr << "[IceTea]: Rule '"<< rule << "' must have it's build property defined as function or array! "
                 << "'" << os->getTypeStr() << "' was given instead." << endl;
        }

        t->target = (Value::Object*)(*targets)[target];
        input = myExpected;
        queue->add(t);
    }
}
