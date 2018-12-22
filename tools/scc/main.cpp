//
// Created by dc on 13/12/18.
//
#include <suil/console.h>
#include <suil/init.h>
#include "meta_builder.h"

using namespace suil;

int main(int argc, char *argv[])
{
    suil::init();
    scc::FileCompiler compiler("parser.grammar");
    scc::MetaGenerator metaGenerator(compiler);

    while (true) {
        char *line;
        size_t len{0};
        console::info("> ");
        if (getline(&line, &len, stdin) < 0) {
            // error reading input
            console::warn("error: failed to read input: %s", errno_s);
            break;
        }

        if (strcmp("exit", line) == 0) {
            /* exit requested */
            free(line);
            break;
        }

        try {
            // compile line
            compiler.compileString(line);
        }
        catch (...) {
            // compilation failed
            console::error("%s\n", Exception::fromCurrent().what());
        }
        free(line);
    };
    return 0;
}
