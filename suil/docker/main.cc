//
// Created by dc on 09/12/17.
//

#include "suil/cmdl.hpp"

using namespace suil;


void cmd_hello(cmdl::Parser& parser) {
    cmdl::Cmd hello{"hello", "Say hello"};
    hello << cmdl::Arg{"append", "Append to hello message",
                            'a', false};

    hello([&](cmdl::Cmd& cmd){
        zcstring  name = cmd["name"];
        zcstring  msg  = cmd["message"];
        zbuffer out{32};
        out << "Hello " << name << "\n";
        if (!msg.empty())
            out << ". We say " << msg << "\n";
        zcstring append = cmd["append"];
        if (!append.empty())
            out << ".\nOh how so good\n";

        write(STDOUT_FILENO, out.data(), out.size());
    });

    parser.add(std::move(hello));
}

void cmd_goodbye(cmdl::Parser& parser) {
    cmdl::Cmd hello{"goodbye", "Say goodbye"};
    hello([&](cmdl::Cmd& cmd){
        zcstring  name = cmd["name"];
        zcstring  msg  = cmd["message"];
        zbuffer out{32};
        out << "Goodbye " << name << "\n";
        if (!msg.empty())
            out << ". We say " << msg << "\n";
        write(STDOUT_FILENO, out.data(), out.size());
    });
    parser.add(std::move(hello));
}

int main(int argc, char *argv[])
{
    suil::init(opt(printinfo, false));

    cmdl::Parser parser("sabci", "0.0.1");
    parser << cmdl::Arg{"name", "The name of the person to greet",
                            'n', false, true};
    parser << cmdl::Arg{"message", "The message to append to the greeting",
                            'm', false};
    cmd_hello(parser);
    cmd_goodbye(parser);
    const char *arguments[] {"./sabci", "hello", "--name", "Carter", "-m", "message", "-a"};
    parser.parse(argc, argv);
    parser.handle();
    return 0;
}