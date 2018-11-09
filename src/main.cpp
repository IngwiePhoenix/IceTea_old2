/**
    @file
    @brief The main IceTea logic

    This file contains and holds upon all the functions that translate targets,
    actions, externals and rules into some logic. This is done by processing
    the input using a single thread, and executing the tasks over a multiple of
    threads (depending on the given hardware).

    Only minor additions are made to the script environment, that contain simple
    wrapper functions.
*/

#include "IceTea.h"

int main(int argc, char** argv) {
    IceTea* it = IceTea::create();
    return it
        ->setupCli(argc, (const char**)argv)
        ->run();
}
