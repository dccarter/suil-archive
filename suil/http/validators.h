//
// Created by dc on 23/11/18.
//

#ifndef SUIL_VALIDATORS_H
#define SUIL_VALIDATORS_H

#include <suil/utils.h>

namespace suil::http::validators {

    struct Email {
        bool operator()(const String& addr);
        bool operator()(const char* addr) {
            String tmp{addr};
            return Ego(tmp);
        }
    };

    struct Password {
        template <typename... Opts>
        Password(Opts... opts)
        { utils::apply_config(Ego, std::forward<Opts>(opts)...); }

        bool operator()(const String& passwd);
        bool operator()(const char* passwd) {
            String tmp{passwd};
            return Ego(tmp);
        }
        bool operator()(OBuffer& out, const String& passwd);
    private:
        int  minChars{6};
        int  maxChars{32};
        int  nums{0};
        int  special{0};
        int  upper{0};
        int  lower{0};
    };
}
#endif //SUIL_VALIDATORS_H
