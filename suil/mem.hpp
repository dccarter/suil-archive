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

    struct memory_priv_t;
    struct memory {
        struct pool {

            pool(const char *name, size_t len, size_t elms);

            pool(pool&& p)
                : priv_(p.priv_)
            {
                p.priv_ = nullptr;
            }

            pool&operator=(pool&& p) {
                priv_ = p.priv_;
                priv_ = nullptr;
            }

            pool(const pool&) = delete;
            pool&operator=(const pool&) = delete;

            void *get();

            void put(void *ptr);

            ~pool();

            memory_priv_t *priv_;
        };

        static void *alloc(size_t);

        static size_t fits(void*, size_t);

        static void *calloc(size_t, size_t);

        static void *realloc(void *, size_t);

        static void free(void *);

        static void init();

        static void cleanup();

        typedef decltype(iod::D(
            prop(allocs, uint64_t),
            prop(frees,  uint64_t),
            prop(name,   std::string),
            prop(entries, uint64_t)
        )) pool_info_t;

        typedef decltype(iod::D(
            prop(worker, uint64_t),
            prop(allocs,  uint64_t),
            prop(frees,   uint64_t),
            prop(entries, uint64_t),
            prop(alloc_miss, uint64_t),
            prop(pools, std::vector<memory::pool_info_t>)
        )) memory_info_t;

        static memory_info_t get_usage();

    };
}

#endif //SUIL_MEM_H