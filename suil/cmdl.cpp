//
// Created by dc on 15/12/17.
//

#include "suil/cmdl.hpp"

namespace suil {
    namespace cmdl {

        command::command(zcstring &&name, const char *descript, bool help)
            : name(name.dup()),
              descript(zcstring{descript}.dup())
        {
            if (help)
                Ego << argument{"help", "Show this command's help", 'h', true, false};
        }

        command& command::operator()(zcstring &&lf, zcstring&& help, char sf, bool opt, bool req) {
            return (Ego << argument{lf.dup(), help.dup(), sf, opt, req});
        }

        command& command::operator<<(argument &&arg) {
            if (!arg.lf) {
                throw suil_error::create(
                        "command line argument missing log format (--help) option");
            }
            for (auto& a : args) {
                if (a.check(arg.sf, arg.lf)) {
                    throw suil_error::create(
                            "command line '", arg.sf, "' or \"",
                            arg.lf, "\" argument duplicated");
                }
            }

            required = required||arg.required;
            /* for help display calculation
             *    -s, --help (required)*/
            size_t len = (required? arg.lf.len+11 : arg.lf.len);
            if (longest < len)
                longest = len;
            arg.global = false;
            args.emplace_back(std::move(arg));
            return Ego;
        }

        void command::showhelp(const char *app, buffer_t &help, bool ishelp) const {

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
                    remaining -= arg.lf.len;
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

        argument& command::check(const zcstring &lf, char sf) {
            argument *arg{nullptr};
            if (check(arg, lf, sf)) {
                return *arg;
            }
            else {
                if (lf) {
                    throw suil_error::create("error: command argument '--", lf, "' not recognized");
                } else {
                    throw suil_error::create("error: command argument '-", sf, "' not recognized");
                }
            }
        }

        bool command::check(argument*& found, const zcstring &lf, char sf) {
            for (auto& arg: args) {
                if (arg.check(sf, lf)) {
                    found = &arg;
                    return true;
                }
            }
            return false;
        }

        bool command::parse(int argc, char **argv, bool dbg) {
            int pos{0};
            zcstring zarg{};

            bool  ishelp{false};

            while (pos < argc) {
                char *carg = argv[pos];
                char *cval = strchr(carg, '=');
                if (cval != nullptr) {
                    // argument taking the form --arg=val
                    *cval++ = '\0';
                }

                int dashes = command::isvalid(carg);
                // are we passing option (or value)
                if (!dashes) {
                    throw suil_error::create("error: Unsupported argument syntax: ", carg);
                }

                if (dashes == 2) {
                    // argument passed in long format
                    argument& arg = Ego.check(&carg[2], NOSF);
                    if (arg.sf == 'h') {
                        ishelp = true;
                        break;
                    }

                    if (passed.find(arg.lf) != passed.end()) {
                        throw suil_error::create("error: command argument '",
                                                 arg.lf, "' appearing more than once");
                    }
                    zcstring val{"1"};
                    if (!arg.option) {
                        if (cval == nullptr) {
                            pos++;
                            if (pos >= argc) {
                                throw suil_error::create("error: command argument '",
                                                         arg.lf,
                                                         "' expects a value but none provided");
                            }
                            cval = argv[pos];
                        }
                        val = zcstring{cval}.dup();
                    }
                    else if (cval != nullptr) {
                        throw suil_error::create("error: command argument '",
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
                        argument& arg = Ego.check(nullptr, carg[opos++]);
                        if (arg.sf == 'h') {
                            ishelp = true;
                            break;
                        }

                        zcstring val{"1"};
                        if (!arg.option) {
                            if (opos < nopts) {
                                throw suil_error::create("error: command argument '",
                                                         arg.lf,
                                                         "' passed as an option but expects value");
                            }

                            if (cval == nullptr) {
                                pos++;
                                if (pos >= argc) {
                                    throw suil_error::create("error: command argument '",
                                                             arg.lf,
                                                             "' expects a value but none provided");
                                }
                                cval = argv[pos];
                            }

                            val = zcstring{cval}.dup();
                        }
                        else if (cval != nullptr) {
                            throw suil_error::create("error: command argument '",
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
                buffer_t msg{127};
                msg << "error: missing required arguments:";
                int  missing{false};
                for (auto& arg: args) {
                    if (arg.required && (passed.find(arg.lf) == passed.end())) {
                        // required option not provided
                        if (!Ego.inter) {
                            msg << (missing ? ", '" : " '") << arg.lf << '\'';
                        } else {
                            // command in interactive, request commands from console
                            requestvalue(arg);
                        }
                        missing = true;
                    }
                }

                if (missing) {
                    if (!Ego.inter) {
                        throw suil_error::create(zcstring(msg));
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

        void command::requestvalue(argument &arg) {
            zcstring display{utils::catstr("Enter ", arg.lf)};
            if (Ego.interhelp) {
                write(STDOUT_FILENO, arg.help(), arg.help.len);
                printf("\n");
            }

            zcstring val = cmdl::readparam(display, nullptr);
            passed.emplace(std::make_pair(arg.lf.peek(), std::move(val)));
        }

        zcstring command::operator[](const zcstring &lf) {
            auto it = passed.find(lf);
            if (it != passed.end())
                return it->second.peek();
            return zcstring{nullptr};
        }

        parser::parser(const char *app, const char *version, const char *descript)
            : appname(zcstring{app}.dup()),
              appversion(zcstring{version}.dup()),
              descript(zcstring{descript}.dup())
        {
            // application version command
            command ver("version", "Show the application version", false);
            ver([&](command& cmd){
                buffer_t b{63};
                b << appname << " v-" << appversion << '\n';
                if (!Ego.descript.empty()) {
                    b << Ego.descript << '\n';
                }
                write(STDOUT_FILENO, b.data(), b.size());
            });
            ver.internal = true;

            // application help command
            command help("help", "Display the application help", false);
            help([&](command& cmd) {
                // show application help
                Ego.showhelp();
            });
            help.internal = true;

            Ego.add(std::move(ver), std::move(help));
            // --help to global flag
            Ego << argument{"help", "Show the help for application", 'h'};
        }

        command* parser::find(const zcstring &name) {
            command *cmd{nullptr};
            for (auto& c: commands) {
                if (c.name == name) {
                    cmd = &c;
                    break;
                }
            }

            return cmd;
        }

        argument* parser::findarg(const zcstring &name, char sf) {
            for (auto& a: globals) {
                if (a.sf == sf || a.lf == name) {
                    return &a;
                }
            }
            return nullptr;
        }

        argument parser::shallowcopy(const argument &arg) {
            return std::move(argument{arg.lf.peek(), nullptr, arg.sf,
                                      arg.option, arg.required, true});
        }

        parser& parser::operator<<(argument &&arg) {
            if (Ego.findarg(arg.lf) == nullptr) {
                arg.global = true;
                required = required || arg.required;
                size_t len = (required? arg.lf.len+11 : arg.lf.len);
                if (longestflag < len)
                    longestflag = len;
                globals.emplace_back(std::move(arg));
            }
            else {
                throw suil_error::create(
                        "duplicate global argument '", arg.lf,
                        " already registered");
            }
        }

        void parser::add(command &&cmd) {
            if (Ego.find(cmd.name) == nullptr) {
                // add copies of global arguments
                for (auto& ga: globals) {
                    argument* _;
                    if (!cmd.check(_, ga.lf, ga.sf)) {
                        argument copy = Ego.shallowcopy(ga);
                        cmd.args.emplace_back(std::move(copy));
                    }
                }

                // accommodate interactive
                inter = inter || cmd.inter;
                size_t len = Ego.inter? (cmd.name.len+14) : cmd.name.len;
                if (longestcmd < len) {
                    longestcmd = len;
                }
                // add a new command
                commands.emplace_back(std::move(cmd));
            }
            else {
                throw suil_error::create(
                        "command with name '", cmd.name, " already registered");
            }
        }

        void parser::showhelp(const char *prefix) {
            buffer_t out{254};
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
                        out << " v-" << appversion;
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
                    size_t remaining{longestcmd-cmd.name.len};
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
                    size_t remaining{longestflag-ga.lf.len};
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
            zcstring str(out);
            write(STDOUT_FILENO, str.cstr, str.len);
        }

        void parser::showcmdhelp(buffer_t &out, command &cmd, bool ishelp) {
            // help was requested or an error occurred
            if (!out.empty()) out << "\n";
            cmd.showhelp(appname.cstr, out, ishelp);
            // append global arguments
            if (!globals.empty()) {
                out << "\nGlobal Flags:\n";
                for (auto& ga: globals) {
                    size_t remaining{longestflag-ga.lf.len};
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

        void parser::parse(int argc, char **argv) {
            if (argc <= 1) {
                // show application help
                Ego.showhelp();
                exit(EXIT_FAILURE);
            }

            if (argv[1][0] == '-') {
                int tr = command::isvalid(argv[1]);
                if (!tr) {
                    fprintf(stderr, "error: bad flag syntax: %s\n", argv[1]);
                    exit(EXIT_FAILURE);
                }
                fprintf(stderr, "error: free floating flags are not supported\n");
                exit(EXIT_FAILURE);
            }

            zcstring  cmdstr{argv[1]};
            command *cmd = Ego.find(cmdstr);
            if (cmd == nullptr) {
                fprintf(stderr, "error: unknown command \"%s\" for \"%s\"\n",
                            argv[1], appname());
                exit(EXIT_FAILURE);
            }

            bool showhelp[2] ={false, true};
            buffer_t errbuf{126};
            try {
                // parse command line (appname command)
                int nargs{argc-2};
                char  **args = nargs? &argv[2] : &argv[1];
                showhelp[0] = cmd->parse(argc-2, args);
            }
            catch (suil_error& ser) {
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

        void parser::handle() {
            if (parsed && parsed->handler) {
                parsed->handler(*parsed);
                return;
            }
            throw suil_error::create("parser::parse should be "
                                             "invoked before invoking handle");
        }

        zcstring parser::getvalue(const zcstring &lf, argument *arg) {
            arg = arg? arg: Ego.findarg(lf);
            if (arg == nullptr) {
                return zcstring{nullptr};
            }
            // find the parameter value
            if (parsed) {
                return (*parsed)[lf];
            }
            return zcstring{nullptr};
        }

        zcstring readparam(const zcstring& display, const char *def) {
            char line[512];
            write(STDOUT_FILENO, display.cstr, display.len);
            printf(": ");
            if (fgets(line, sizeof(line), stdin)) {
                // copy data to zcstring, trimming of \n
                return zcstring{line, strlen(line)-1, false}.dup();
            }
            return zcstring{def};
        }

        zcstring readpasswd(const zcstring& display) {
            char *pass = getpass(display());
        }
    }
}