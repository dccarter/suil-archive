//
// Created by dc on 13/12/18.
//
#include <suil/console.h>
#include <suil/init.h>
#include <suil/cmdl.h>

#include "parser.h"

using namespace suil;

static void generateTypes(const std::vector<String> &files, const String& outdir);

static void cmd_Generate(cmdl::Parser& parser)
{
    cmdl::Cmd gen{"gen", "Compiles given file(s) and generates associated types/services"};
    gen << cmdl::Arg{"inputs",
                     "paths to input files to generate types from (absolute paths or relative to working directory)",
                     'i', false, true};
    gen << cmdl::Arg{"outdir",
                     "the directory to output the generated files (defaults to working directory)",
                     'O', false};
    gen([&](cmdl::Cmd& cmd){
        auto in = cmd.getvalue<std::vector<String>>("inputs", {});
        if (in.empty()) {
            // fail immediately
            fprintf(stderr, "error: at least 1 input file must be specified\n");
            exit(EXIT_FAILURE);
        }

        String out = cmd["outdir"];
        generateTypes(in, out);
    });

    cmdl::Cmd repl{"repl", "Enter's an interactive shell useful for debugging"};
    repl << cmdl::Arg{"grammar",
                     "path to the grammar file to work with",
                     'g', false};
    repl([&](cmdl::Cmd& cmd) {
        // create a new parser and enter it's REPL
        String grammar = cmd["grammar"];
        scc::Parser p;
        if (!p.load(grammar.empty()? nullptr: grammar()))
            exit(EXIT_FAILURE);
        p.repl();
    });

    parser.add(std::move(gen));
    parser.add(std::move(repl));
}

int main(int argc, char *argv[])
{
    suil::init(opt(printinfo, false));
    log::setup(opt(name, APP_NAME));

    cmdl::Parser parser(APP_NAME,  APP_VERSION);
    cmd_Generate(parser);
    try {
        parser.parse(argc, argv);
        parser.handle();
    }
    catch (...)
    {
        fprintf(stderr, "error: %s\n", Exception::fromCurrent().what());
        exit(-1);
    }
    return 0;
}

void generateTypes(const std::vector<String> &files, const String& outdir)
{
    scc::Parser p;
    if (!p.load()) {
        // loading failed
        exit(EXIT_FAILURE);
    }

    for (auto& f: files) {
        // parse all files
        console::info("parsing file %s ...", f());
        auto programFile = p.parseFile(f());
        programFile.generate(f(), outdir.data());
    }
}