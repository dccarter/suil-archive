/*
 * Copyright (c) 2004-2013 Sergey Lyubka <valenok@gmail.com>
 * Copyright (c) 2013 Cesanta Software Limited
 * All rights reserved
 *
 * This library is dual-licensed: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. For the terms of this
 * license, see <http://www.gnu.org/licenses/>.
 *
 * You are free to use this library under the terms of the GNU General
 * Public License, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * Alternatively, you can license this library under a commercial
 * license, as set out in <http://cesanta.com/products.html>.
 */

/*
 * This is a regular expression library that implements a subset of Perl RE.
 * Please refer to README.md for a detailed reference.
 */

#ifndef SLRE_SLRE_H_
#define SLRE_SLRE_H_

#include <suil/sys.hpp>

/* slre_match() failure codes */
#define SLRE_NO_MATCH               -1
#define SLRE_UNEXPECTED_QUANTIFIER  -2
#define SLRE_UNBALANCED_BRACKETS    -3
#define SLRE_INTERNAL_ERROR         -4
#define SLRE_INVALID_CHARACTER_SET  -5
#define SLRE_INVALID_METACHARACTER  -6
#define SLRE_CAPS_ARRAY_TOO_SMALL   -7
#define SLRE_TOO_MANY_BRANCHES      -8
#define SLRE_TOO_MANY_BRACKETS      -9

namespace suil {

    namespace regex {

        enum { ignore_case = 1 };

        struct capture {
            const char *ptr;
            int len;
        };

        int match(const char *regexp, const char *buf, size_t size,
                       capture caps[], size_t ncaps, int flags = 0);

        int match(const char *regexp, const char *buf, size_t size, std::vector<capture>& caps, int flags = 0);

        inline int match(
                const char *regexp,
                const suil::zcstring& data,
                std::vector<capture>& cap, int flags = 0)
        {
            return match(regexp, data.data(), data.size(), nullptr, 0, flags);
        }

        inline int match(
                const char *regexp,
                const zbuffer& data,
                std::vector<capture>& cap, int flags = 0)
        {
            return match(regexp, data.data(), data.size(), nullptr, 0, flags);
        }

        inline bool match(const char *regexp, const char *buf, int size, int flags = 0) {
            return match(regexp, buf, size, nullptr, 0, flags) >= 0;
        }

        inline bool match(const char *regexp, const suil::zcstring& data, int flags = 0) {
            return match(regexp, data.data(), data.size(), nullptr, 0, flags) >= 0;
        }

        inline bool match(const char *regexp, const suil::zbuffer& buf, int flags = 0) {
            return match(regexp, buf.data(), buf.size(), nullptr, 0, flags) >= 0;
        }
    };
}
#endif /* SLRE_SLRE_H_ */
