//
// Created by dc on 31/05/17.
//

#ifndef SUIL_MEM_H
#define SUIL_MEM_H

#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <vector>

#include <suil/symbols.h>

#define prop(pp, tt)  s::_##pp = tt ()
#define oprop(pp, tt) s::_##pp (s::_optional) = tt ()

namespace suil {

    struct memory {
        static void *alloc(size_t);

        static size_t fits(void *, size_t);

        static void *calloc(size_t, size_t);

        static void *realloc(void *, size_t);

        static void free(void *);
    };
}

#endif //SUIL_MEM_H