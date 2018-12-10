//
// Created by dc on 23/11/18.
//

#ifndef SUIL_VALIDATORS_H
#define SUIL_VALIDATORS_H

#include <suil/utils.h>

namespace suil::http {

    namespace validators {

        struct Email {
            bool operator()(const String &addr);

            bool operator()(const char *addr) {
                String tmp{addr};
                return Ego(tmp);
            }

            bool operator()(OBuffer &out, const String &addr);
        };

        struct Password {
            template<typename... Opts>
            Password(Opts... opts) { utils::apply_config(Ego, std::forward<Opts>(opts)...); }

            bool operator()(const String &passwd);

            bool operator()(const char *passwd) {
                String tmp{passwd};
                return Ego(tmp);
            }

            bool operator()(OBuffer &out, const String &passwd);

        private:
            int minChars{6};
            int maxChars{32};
            int nums{0};
            int special{0};
            int upper{0};
            int lower{0};
        };

        struct Time {
            template<typename... Opts>
            Time(const char *fmt = "%D")
                    : fmt(fmt) {}

            inline bool operator()(const String &tstr) {
                OBuffer ob;
                return Ego(ob, tstr);
            }

            inline bool operator()(const char *tstr) {
                String tmp{tstr};
                return Ego(tmp);
            }

            bool operator()(OBuffer &out, const String &tstr);

        private:
            const char *fmt{"%D"};
        };
    }

    template <typename O, typename... V>
    static String validate(const O& o, V... v) {
        auto vs = iod::D(std::forward<V>(v)...);
        String status{nullptr};
        OBuffer ob;

        iod::foreach2(vs) | [&](auto& m) {
            /* invoke all validators */
            if (status.empty() && !m.value()(ob, m.symbol().member_access(o))) {
                /* oops, validation failure */
                ob << "\n";
                status = String{ob};
            }
        };

        return status;
    }
}
#endif //SUIL_VALIDATORS_H
