//
// Created by dc on 15/12/17.
//

#ifndef SUIL_CMDL_HPP
#define SUIL_CMDL_HPP


#include <suil/utils.h>

namespace suil {

    namespace cmdl {

        static const char NOSF = '\0';
        struct Arg {
            String        lf{nullptr};
            String        help;
            char            sf{NOSF};
            bool            option{true};
            bool            required{false};
            bool            global{false};
        private:
            friend struct Cmd;

            inline bool check(const String& arg) const {
                return lf == arg;
            }

            inline bool check(char c) const {
                return c == sf;
            }

            inline bool check(char s, const String& l) {
                return ((s != NOSF) && Ego.check(s)) || Ego.check(l);
            }
        };

        struct Cmd {
            Cmd(String&& name, const char *descript = nullptr, bool help =  true);
            Cmd&operator()(String&& lf, String&& help, char sf, bool opt, bool req);
            Cmd&operator<<(Arg&&arg);
            Cmd&operator()(std::function<void(Cmd&)> handler) {
                if (Ego.handler != nullptr) {
                    throw Exception::create("command '", name,
                                             "' already assigned a handler");
                }
                Ego.handler = handler;
                return Ego;
            }
            void showhelp(const char *app, OBuffer &help, bool ishelp = false) const;
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
                    String name{n.data(), n.size(), false};
                    getvalue(name, m.value());
                };

                return false;
            }

            inline bool interactive(bool showhelp=false) {
                Ego.inter = true;
                Ego.interhelp = showhelp;
                return true;
            }

            String operator[](char sf) {
                Arg& arg = Ego.check(nullptr, sf);
                return Ego[arg.lf].peek();
            }
            String operator[](const char *lf) {
                Arg& arg = Ego.check(lf, NOSF);
                return Ego[arg.lf].peek();
            }

            String operator[](const String& lf);

            template <typename V>
            V getvalue(const char*lf, const V def) {
                String zlf{lf};
                return std::move(getvalue(zlf, def));
            }

            String getvalue(const char*lf, const char* def) {
                String zlf{lf};
                String zdef{def};
                return std::move(getvalue(zlf, zdef));
            }

            template <typename V>
            V getvalue(const String& name, const V def) {
                Arg *_;
                if (!check(_, name, NOSF)) {
                    throw Exception::create("passed parameter '",
                            name, "' is not an argument");
                }
                V tmp = def;
                String zstr = Ego[name];
                if (!zstr.empty()) {
                    setvalue(tmp, zstr);
                }
                return std::move(tmp);
            }

        private:
            friend struct Parser;

            template <typename V>
            inline void setvalue(V& out, String& from) {
                utils::cast(from, out);
            }

            template <typename V>
            inline void setvalue(std::vector<V>& out, String& from) {
                auto& parts = from.split(",");
                for (auto& part: parts) {
                    V val;
                    String tmp{part};
                    String trimd = tmp.trim(' ');
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

            void requestvalue(Arg& arg);

            Arg& check(const String& lf, char sf);
            bool check(Arg*& found, const String& lf, char sf);
            String    name;
            String    descript;
            std::vector<Arg> args;
            size_t      longest{0};
            bool        required{false};
            bool        internal{false};
            bool        inter{false};
            bool        interhelp{false};
            CaseMap<String> passed;
            std::function<void(Cmd&)> handler;
        };

        struct Parser {
            Parser(const char* app, const char *version, const char *descript = nullptr);
            template <typename... Commands>
            void add(Cmd&& cmd, Commands&&... cmds) {
                add(std::forward<Cmd>(cmd));
                add(std::forward<Commands>(cmds)...);
            }

            void add(Cmd&& cmd);

            Parser&operator<<(Arg&& arg);

            void  parse(int argc, char *argv[]);
            void  handle();
            void  showcmdhelp(OBuffer& out, Cmd& cmd, bool ishelp);
            const Cmd* getcmd() const {
                return parsed;
            }

            template <typename __T>
            String operator[](char sf) {
                String _{nullptr};
                Arg *arg = findarg(_, sf);
                if (arg) {
                    return Ego.getvalue(arg->lf, arg);
                }
                return String{};
            }
            String operator[](const char* lf) {
                String zlf{lf};
                return getvalue(zlf, nullptr);
            }

            String operator[](const String& lf) {
                return getvalue(lf, nullptr);
            }

        private:
            String  getvalue(const String&, Arg* arg);
            void      showhelp(const char *prefix = nullptr);
            Cmd*  find(const String& name);
            Arg* findarg(const String& name, char sf=NOSF);
            Arg  shallowcopy(const Arg& arg);
            void add(){}
            std::vector<Cmd>  commands;
            std::vector<Arg> globals;
            // this is the command that successfully passed
            Cmd               *parsed{nullptr};
            String              appname;
            String              descript;
            String              appversion;
            size_t                longestcmd{0};
            size_t                longestflag{0};
            bool                  required{false};
            bool                  inter{false};
        };

        String readparam(const String& display, const char *def);
        String readpasswd(const String& display);
    }

}
#endif //SUIL_CMDL_HPP
