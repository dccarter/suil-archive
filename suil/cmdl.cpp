//
// Created by dc on 15/12/17.
//

#include "suil/cmdl.h"

namespace suil {
    namespace cmdl {

        Cmd::Cmd(String &&name, const char *descript, bool help)
            : name(name.dup()),
              descript(String{descript}.dup())
        {
            if (help)
                Ego << Arg{"help", "Show this command's help", 'h', true, false};
        }

        Cmd& Cmd::operator()(String &&lf, String&& help, char sf, bool opt, bool req) {
            return (Ego << Arg{lf.dup(), help.dup(), sf, opt, req});
        }

        Cmd& Cmd::operator<<(Arg &&arg) {
            if (!arg.lf) {
                throw Exception::create(
                        "command line argument missing log format (--help) option");
            }
            for (auto& a : args) {
                if (a.check(arg.sf, arg.lf)) {
                    throw Exception::create(
                            "command line '", arg.sf, "' or \"",
                            arg.lf, "\" argument duplicated");
                }
            }

            required = required||arg.required;
            /* for help display calculation
             *    -s, --help (required)*/
            size_t len = (required? arg.lf.size()+11 : arg.lf.size());
            if (longest < len)
                longest = len;
            arg.global = false;
            args.emplace_back(std::move(arg));
            return Ego;
        }

        void Cmd::showhelp(const char *app, OBuffer &help, bool ishelp) const {

            if (ishelp) {
                help << descript << "\n";
                help << "\n";
            };
            help << "Usage:\n";
            help << "  " << app << ' ' << name;
            if (!args.empty()) {
                help << " [flags]\n";
                help << "\n";
                help << "Flags:\n";

                for (auto& arg: args) {
                    if (arg.global)
                        continue;

                    size_t  remaining{longest};
                    help << "    ";
                    if (arg.sf != NOSF) {
                        help << '-' << arg.sf;
                        if (arg.lf)
                            help << ", ";
                    } else {
                        help << "    ";
                    }

                    help << "--" <<  arg.lf;
                    remaining -= arg.lf.size();
                    if (arg.required) {
                        help << " (required)";
                        remaining -= 11;
                    }
                    std::string str;
                    str.resize(remaining, ' ');
                    help << str << " " << arg.help;
                    help << "\n";
                }
            }
            else {
                // append new line
                help << '\n';
            }
        }

        Arg& Cmd::check(const String &lf, char sf) {
            Arg *arg{nullptr};
            if (check(arg, lf, sf)) {
                return *arg;
            }
            else {
                if (lf) {
                    throw Exception::create("error: command argument '--", lf, "' not recognized");
                } else {
                    throw Exception::create("error: command argument '-", sf, "' not recognized");
                }
            }
        }

        bool Cmd::check(Arg*& found, const String &lf, char sf) {
            for (auto& arg: args) {
                if (arg.check(sf, lf)) {
                    found = &arg;
                    return true;
                }
            }
            return false;
        }

        bool Cmd::parse(int argc, char **argv, bool dbg) {
            int pos{0};
            String zarg{};

            bool  ishelp{false};

            while (pos < argc) {
                char *carg = argv[pos];
                char *cval = strchr(carg, '=');
                if (cval != nullptr) {
                    // argument taking the form --arg=val
                    *cval++ = '\0';
                }

                int dashes = Cmd::isvalid(carg);
                // are we passing option (or value)
                if (!dashes) {
                    throw Exception::create("error: Unsupported argument syntax: ", carg);
                }

                if (dashes == 2) {
                    // argument passed in long format
                    Arg& arg = Ego.check(&carg[2], NOSF);
                    if (arg.sf == 'h') {
                        ishelp = true;
                        break;
                    }

                    if (passed.find(arg.lf) != passed.end()) {
                        throw Exception::create("error: command argument '",
                                                 arg.lf, "' appearing more than once");
                    }
                    String val{"1"};
                    if (!arg.option) {
                        if (cval == nullptr) {
                            pos++;
                            if (pos >= argc) {
                                throw Exception::create("error: command argument '",
                                                         arg.lf,
                                                         "' expects a value but none provided");
                            }
                            cval = argv[pos];
                        }
                        val = String{cval}.dup();
                    }
                    else if (cval != nullptr) {
                        throw Exception::create("error: command argument '",
                                                 arg.lf, "' assigned value but is an option");
                    }
                    passed.emplace(std::make_pair(arg.lf.dup(), std::move(val)));
                }
                else {
                    // options can be passed as -abcdef where each character is an
                    // option
                    size_t nopts = strlen(carg);
                    size_t opos{1};
                    while (opos < nopts) {
                        Arg& arg = Ego.check(nullptr, carg[opos++]);
                        if (arg.sf == 'h') {
                            ishelp = true;
                            break;
                        }

                        String val{"1"};
                        if (!arg.option) {
                            if (opos < nopts) {
                                throw Exception::create("error: command argument '",
                                                         arg.lf,
                                                         "' passed as an option but expects value");
                            }

                            if (cval == nullptr) {
                                pos++;
                                if (pos >= argc) {
                                    throw Exception::create("error: command argument '",
                                                             arg.lf,
                                                             "' expects a value but none provided");
                                }
                                cval = argv[pos];
                            }

                            val = String{cval}.dup();
                        }
                        else if (cval != nullptr) {
                            throw Exception::create("error: command argument '",
                                            arg.lf, "' assigned value but is an option");
                        }
                        passed.emplace(std::make_pair(arg.lf.dup(), std::move(val)));
                    }
                    if (ishelp)
                        break;
                }
                pos++;
            }

            if (!ishelp) {
                // verify required arguments
                OBuffer msg{127};
                msg << "error: missing required arguments:";
                int  missing{false};
                for (auto& arg: args) {
                    if (arg.required && (passed.find(arg.lf) == passed.end())) {
                        // required option not provided
                        if (!Ego.inter) {
                            msg << (missing ? ", '" : " '") << arg.lf << '\'';
                        } else {
                            // command in interactive, Request commands from console
                            requestvalue(arg);
                        }
                        missing = true;
                    }
                }

                if (missing) {
                    if (!Ego.inter) {
                        throw Exception::create(String(msg));
                    }
                    else {
                        printf("\n");
                    }
                }
            }

            if (dbg) {
                // dump all arguments to console
                for (auto& kvp: passed) {
                    printf("--%s = %s\n", kvp.first(),
                           (kvp.second.empty()? "nil" : kvp.second()));
                }
            }

            return ishelp;
        }

        void Cmd::requestvalue(Arg &arg) {
            String display{utils::catstr("Enter ", arg.lf)};
            if (Ego.interhelp) {
                write(STDOUT_FILENO, arg.help(), arg.help.size());
                printf("\n");
            }

            String val = cmdl::readparam(display, nullptr);
            passed.emplace(std::make_pair(arg.lf.peek(), std::move(val)));
        }

        String Cmd::operator[](const String &lf) {
            auto it = passed.find(lf);
            if (it != passed.end())
                return it->second.peek();
            return String{nullptr};
        }

        Parser::Parser(const char *app, const char *version, const char *descript)
            : appname(String{app}.dup()),
              appversion(String{version}.dup()),
              descript(String{descript}.dup())
        {
            // application version command
            Cmd ver("version", "Show the application version", false);
            ver([&](Cmd& cmd){
                OBuffer b{63};
                b << appname << " v" << appversion << '\n';
                if (!Ego.descript.empty()) {
                    b << Ego.descript << '\n';
                }
                write(STDOUT_FILENO, b.data(), b.size());
            });
            ver.internal = true;

            // application help command
            Cmd help("help", "Display the application help", false);
            help([&](Cmd& cmd) {
                // show application help
                Ego.showhelp();
            });
            help.internal = true;

            Ego.add(std::move(ver), std::move(help));
            // --help to global flag
            Ego << Arg{"help", "Show the help for application", 'h'};
        }

        Cmd* Parser::find(const String &name) {
            Cmd *cmd{nullptr};
            for (auto& c: commands) {
                if (c.name == name) {
                    cmd = &c;
                    break;
                }
            }

            return cmd;
        }

        Arg* Parser::findarg(const String &name, char sf) {
            for (auto& a: globals) {
                if (a.sf == sf || a.lf == name) {
                    return &a;
                }
            }
            return nullptr;
        }

        Arg Parser::shallowcopy(const Arg &arg) {
            return std::move(Arg{arg.lf.peek(), nullptr, arg.sf,
                                      arg.option, arg.required, true});
        }

        Parser& Parser::operator<<(Arg &&arg) {
            if (Ego.findarg(arg.lf) == nullptr) {
                arg.global = true;
                required = required || arg.required;
                size_t len = (required? arg.lf.size()+11 : arg.lf.size());
                if (longestflag < len)
                    longestflag = len;
                globals.emplace_back(std::move(arg));
            }
            else {
                throw Exception::create(
                        "duplicate global argument '", arg.lf,
                        " already registered");
            }
            return Ego;
        }

        void Parser::add(Cmd &&cmd) {
            if (Ego.find(cmd.name) == nullptr) {
                // add copies of global arguments
                for (auto& ga: globals) {
                    Arg* _;
                    if (!cmd.check(_, ga.lf, ga.sf)) {
                        Arg copy = Ego.shallowcopy(ga);
                        cmd.args.emplace_back(std::move(copy));
                    }
                }

                // accommodate interactive
                inter = inter || cmd.inter;
                size_t len = Ego.inter? (cmd.name.size()+14) : cmd.name.size();
                if (longestcmd < len) {
                    longestcmd = len;
                }
                // add a new command
                commands.emplace_back(std::move(cmd));
            }
            else {
                throw Exception::create(
                        "command with name '", cmd.name, " already registered");
            }
        }

        void Parser::showhelp(const char *prefix) {
            OBuffer out{254};
            if (prefix != nullptr) {
                out << prefix << '\n';
            }
            else {
                if (descript) {
                    // show description
                    out << descript << '\n';
                }
                else {
                    // make up description
                    out << appname;
                    if (appversion) {
                        out << " v" << appversion;
                    }
                    out << '\n';
                }
            }
            out << '\n';

            out << "Usage:"
                << "    " << appname << " [command]\n"
                << '\n';

            // append commands help
            if (!commands.empty()) {
                out << "Available Commands:\n";
                for (auto& cmd: commands) {
                    size_t remaining{longestcmd-cmd.name.size()};
                    out << "  " << cmd.name << ' ';
                    if (cmd.inter) {
                        out << "(interactive) ";
                        remaining -= 14;
                    }
                    if (remaining > 0) {
                        std::string str;
                        str.resize(remaining, ' ');
                        out << str;
                    }
                    out << cmd.descript << '\n';
                }
            }

            // append global arguments
            if (!globals.empty()) {
                out << "Flags:\n";
                for (auto& ga: globals) {
                    size_t remaining{longestflag-ga.lf.size()};
                    out << "    ";
                    if (ga.sf != NOSF) {
                        out << '-' << ga.sf;
                        if (ga.lf)
                            out << ", ";
                    } else {
                        out << "    ";
                    }

                    out << "--" <<  ga.lf;
                    if (ga.required) {
                        out << " (required)";
                        remaining -= 11;
                    }
                    std::string str;
                    str.resize(remaining, ' ');
                    out << str << " " << ga.help;
                    out << "\n";
                }
            }
            out << '\n'
                << "Use \"" << appname
                << "\" [command] --help for more information about a command"
                << "\n";
            String str(out);
            write(STDOUT_FILENO, str.data(), str.size());
        }

        void Parser::showcmdhelp(OBuffer &out, Cmd &cmd, bool ishelp) {
            // help was requested or an error occurred
            if (!out.empty()) out << "\n";
            cmd.showhelp(appname.data(), out, ishelp);
            // append global arguments
            if (!globals.empty()) {
                out << "\nGlobal Flags:\n";
                for (auto& ga: globals) {
                    size_t remaining{longestflag-ga.lf.size()};
                    out << "    ";
                    if (ga.sf != NOSF) {
                        out << '-' << ga.sf;
                        if (ga.lf)
                            out << ", ";
                    } else {
                        out << "    ";
                    }

                    out << "--" <<  ga.lf;
                    if (ga.required) {
                        out << " (required)";
                        remaining -= 11;
                    }
                    std::string str;
                    str.resize(remaining, ' ');
                    out << str << " " << ga.help;
                    out << "\n";
                }
            }
            write(STDOUT_FILENO, out.data(), out.size());
        }

        void Parser::parse(int argc, char **argv) {
            if (argc <= 1) {
                // show application help
                Ego.showhelp();
                exit(EXIT_FAILURE);
            }

            if (argv[1][0] == '-') {
                int tr = Cmd::isvalid(argv[1]);
                if (!tr) {
                    fprintf(stderr, "error: bad flag syntax: %s\n", argv[1]);
                    exit(EXIT_FAILURE);
                }
                fprintf(stderr, "error: free floating flags are not supported\n");
                exit(EXIT_FAILURE);
            }

            String  cmdstr{argv[1]};
            Cmd *cmd = Ego.find(cmdstr);
            if (cmd == nullptr) {
                fprintf(stderr, "error: unknown command \"%s\" for \"%s\"\n",
                            argv[1], appname());
                exit(EXIT_FAILURE);
            }

            bool showhelp[2] ={false, true};
            OBuffer errbuf{126};
            try {
                // parse command line (appname command)
                int nargs{argc-2};
                char  **args = nargs? &argv[2] : &argv[1];
                showhelp[0] = cmd->parse(argc-2, args);
            }
            catch (Exception& ser) {
                showhelp[0] = true;
                showhelp[1] = false;
                errbuf << ser.what() << "\n";
            }

            if (showhelp[0]) {
                //
                Ego.showcmdhelp(errbuf, *cmd, showhelp[1]);
                exit(showhelp[1]? EXIT_SUCCESS:EXIT_FAILURE);
            }

            // save passed command
            parsed = cmd;

            if (cmd->internal) {
                // execute internal commands and exit
                Ego.handle();
                exit(EXIT_SUCCESS);
            }
        }

        void Parser::handle() {
            if (parsed && parsed->handler) {
                parsed->handler(*parsed);
                return;
            }
            throw Exception::create("parser::parse should be "
                                             "invoked before invoking handle");
        }

        String Parser::getvalue(const String &lf, Arg *arg) {
            arg = arg? arg: Ego.findarg(lf);
            if (arg == nullptr) {
                return String{nullptr};
            }
            // find the parameter value
            if (parsed) {
                return (*parsed)[lf];
            }
            return String{nullptr};
        }

        String readparam(const String& display, const char *def) {
            char line[512];
            write(STDOUT_FILENO, display.data(), display.size());
            printf(": ");
            if (fgets(line, sizeof(line), stdin)) {
                // copy data to String, trimming of \n
                return String{line, strlen(line)-1, false}.dup();
            }
            return String{def};
        }

        String readpasswd(const String& display) {
            char *pass = getpass(display());
            return String{pass}.dup();
        }
    }
}