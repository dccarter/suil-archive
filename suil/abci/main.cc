//
// Created by dc on 09/12/17.
//

#include "server.hpp"
#include "client.hpp"
#include "suil/redis.hpp"
#include "suil/cmdl.hpp"

using namespace suil;

coroutine void runclient() {
    tdmabci::client<> dummy("127.0.0.1", 46658);
    msleep(utils::after(5000));

    dummy.connect();

}

void cmd_hello(cmdl::parser& parser) {
    cmdl::command hello{"hello", "Say hello"};
    hello << cmdl::argument{"append", "Append to hello message",
                            'a'};
    hello << cmdl::argument{"data", "Append to hello message",
                            cmdl::NOSF, false};

    hello([&](cmdl::command& cmd){
        zcstring  name = cmd["name"];
        zcstring  msg  = cmd["message"];
        buffer_t out{32};
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

void cmd_goodbye(cmdl::parser& parser) {
    cmdl::command hello{"goodbye", "Say goodbye"};
    hello([&](cmdl::command& cmd){
        zcstring  name = cmd["name"];
        zcstring  msg  = cmd["message"];
        buffer_t out{32};
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

    cmdl::parser parser("sabci", "0.0.1");
    parser << cmdl::argument{"name", "The name of the person to greet",
                            'n', false, true};
    parser << cmdl::argument{"message", "The message to append to the greeting",
                            'm', false};
    cmd_hello(parser);
    cmd_goodbye(parser);
    const char *arguments[] {"./sabci", "hello", "--name", "Carter", "-m", "message", "-a"};
    parser.parse(argc, argv);
    zcstring name = parser["name"];
    zcstring message = parser["message"];
    zcstring data    = parser["data"];
    sinfo("global args: %s %s %s", name("nil"), message("nil"), data("nil"));
    return 0;
}