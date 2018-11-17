//
// Created by dc on 01/10/18.
//

#include <suil/cmdl.h>
#include <suil/init.h>
#include "sapp.hpp"

using namespace suil;

void cmd_GenProject(cmdl::Parser& parser) {
    cmdl::Cmd init{"init", "Generate project template in current directory"};
    init << cmdl::Arg{"name",
                      "the name to be used for the current project",
                      cmdl::NOSF, false, false};
    init << cmdl::Arg{"base",
                      "the base directory on which to lookup suil library",
                      'b', false, false};

    init([&](cmdl::Cmd& cmd) {
        // get the name of the command
        String name = cmd["name"];
        String base = cmd["base"];
        tools::suil_InitProjectTemplate(name, base);
    });

    parser.add(std::move(init));
}

int main(int argc, char *argv[])
{
    suil::init(opt(printinfo, false));
    log::setup(opt(name, APP_NAME));

    cmdl::Parser parser(APP_NAME, APP_VERSION);
    cmd_GenProject(parser);
    try {
        parser.parse(argc, argv);
        parser.handle();
    }
    catch (...)
    {
        fprintf(stderr, "error: %s\n", Exception::fromCurrent().what());
    }
    return 0;
}