//
// Created by dc on 15/12/17.
//

#ifndef SUIL_CMDL_HPP
#define SUIL_CMDL_HPP


#include <suil/sys.hpp>

namespace suil {

    namespace cmdl {

        static const char NOSF = '\0';
        struct argument {
            zcstring        lf{nullptr};
            zcstring        help;
            char            sf{NOSF};
            bool            option{true};
            bool            required{false};
            bool            global{false};
        private:
            friend struct command;

            inline bool check(const zcstring& arg) const {
                return lf == arg;
            }

            inline bool check(char c) const {
                return c == sf;
            }

            inline bool check(char s, const zcstring& l) {
                return ((s != NOSF) && Ego.check(s)) || Ego.check(l);
            }
        };

        struct command {
            command(zcstring&& name, const char *descript = nullptr, bool help =  true);
            command&operator()(zcstring&& lf, zcstring&& help, char sf, bool opt, bool req);
            command&operator<<(argument&&arg);
            command&operator()(std::function<void(command&)> handler) {
                if (Ego.handler != nullptr) {
                    throw suil_error::create("command '", name,
                                             "' already assigned a handler");
                }
                Ego.handler = handler;
            }
            void showhelp(const char *app, buffer_t &help, bool ishelp = false) const;
            bool parse(int argc, char *argv[], bool debug = false);

            template <typename... O>
            bool parse(iod::sio<O...>& obj, int argc, char *argv[]) {
                // parse arguments as usual
                bool ishelp = parse(argc, argv, true);
                if (ishelp) {
                    return ishelp;
                }

                // parse arguments based on object
                iod::foreach2(obj) |
                [&](auto& m) {
                    auto& n = m.symbol().name();
                    zcstring name{n.data(), n.size(), false};
                    getvalue(name, m.value());
                };
            }

            inline bool interactive(bool showhelp=false) {
                Ego.inter = true;
                Ego.interhelp = showhelp;
            }

            zcstring operator[](char sf) {
                argument& arg = Ego.check(nullptr, sf);
                return Ego[arg.lf].peek();
            }
            zcstring operator[](const char *lf) {
                argument& arg = Ego.check(lf, NOSF);
                return Ego[arg.lf].peek();
            }

            zcstring operator[](const zcstring& lf);

            template <typename V>
            V getvalue(const char*lf, const V def) {
                zcstring zlf{lf};
                return std::move(getvalue(zlf, def));
            }

            zcstring getvalue(const char*lf, const char* def) {
                zcstring zlf{lf};
                zcstring zdef{def};
                return std::move(getvalue(zlf, zdef));
            }

            template <typename V>
            V getvalue(const zcstring& name, const V def) {
                argument *_;
                if (!check(_, name, NOSF)) {
                    throw suil_error::create("passed parameter '",
                            name, "' is not an argument");
                }
                V tmp = def;
                zcstring zstr = Ego[name];
                if (!zstr.empty()) {
                    setvalue(tmp, zstr);
                }
                return std::move(tmp);
            }

        private:
            friend struct parser;

            template <typename V>
            inline void setvalue(V& out, zcstring& from) {
                utils::cast(from, out);
            }

            template <typename V>
            inline void setvalue(std::vector<V>& out, zcstring& from) {
                auto& parts = utils::strsplit(from, ",");
                for (auto& part: parts) {
                    V val;
                    zcstring tmp{part};
                    zcstring trimd = utils::strtrim(tmp, ' ');
                    setvalue(val, trimd);
                    out.emplace_back(std::move(val));
                }
            }

            static int isvalid(const char *flag) {
                if (flag[0] == '-') {
                    if (flag[1] == '-') {
                        return (flag[2] != '\0' && flag[2] != '-' && isalpha(flag[2]))?
                               2 : 0;
                    }
                    return (flag[1] != '\0' && isalpha(flag[1]))? 1: 0;
                }
                return 0;
            }

            void requestvalue(argument& arg);

            argument& check(const zcstring& lf, char sf);
            bool check(argument*& found, const zcstring& lf, char sf);
            zcstring    name;
            zcstring    descript;
            std::vector<argument> args;
            size_t      longest{0};
            bool        required{false};
            bool        internal{false};
            bool        inter{false};
            bool        interhelp{false};
            zcstr_map_t<zcstring> passed;
            std::function<void(command&)> handler;
        };

        struct parser {
            parser(const char* app, const char *version, const char *descript = nullptr);
            template <typename... Commands>
            void add(command&& cmd, Commands&&... cmds) {
                add(std::forward<command>(cmd));
                add(std::forward<Commands>(cmds)...);
            }

            void add(command&& cmd);

            parser&operator<<(argument&& arg);

            void  parse(int argc, char *argv[]);
            void  handle();
            void  showcmdhelp(buffer_t& out, command& cmd, bool ishelp);
            const command* getcmd() const {
                return parsed;
            }

            template <typename __T>
            zcstring operator[](char sf) {
                zcstring _{nullptr};
                argument *arg = findarg(_, sf);
                if (arg) {
                    return Ego.getvalue(arg->lf, arg);
                }
                return zcstring{};
            }
            zcstring operator[](const char* lf) {
                zcstring zlf{lf};
                return getvalue(zlf, nullptr);
            }

            zcstring operator[](const zcstring& lf) {
                return getvalue(lf, nullptr);
            }

        private:
            zcstring  getvalue(const zcstring&, argument* arg);
            void      showhelp(const char *prefix = nullptr);
            command*  find(const zcstring& name);
            argument* findarg(const zcstring& name, char sf=NOSF);
            argument  shallowcopy(const argument& arg);
            void add(){}
            std::vector<command>  commands;
            std::vector<argument> globals;
            // this is the command that successfully passed
            command               *parsed{nullptr};
            zcstring              appname;
            zcstring              descript;
            zcstring              appversion;
            size_t                longestcmd{0};
            size_t                longestflag{0};
            bool                  required{false};
            bool                  inter{false};
        };

        zcstring readparam(const zcstring& display, const char *def);
        zcstring readpasswd(const zcstring& display);
    }

}
#endif //SUIL_CMDL_HPP
