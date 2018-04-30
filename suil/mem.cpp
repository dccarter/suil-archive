//
// Created by dc on 4/30/18.
//

#include "mem.hpp"
#include "sys.hpp"

namespace suil {

    void *memory::alloc(size_t size) {
        void *p = ::malloc(size + sizeof(size_t));
        if (p == nullptr)
            scritical("allocating memory of size %lu failed: %s", size, errno_s);
        return p;
    }

    size_t memory::fits(void *, size_t) { return 0; }

    void *memory::calloc(size_t memb, size_t size) {
        void *p = ::calloc(memb, size);
        if (p == nullptr)
            scritical("allocating memory of size %lu failed: %s", size, errno_s);
        return p;
    }

    void *memory::realloc(void *ptr, size_t size) {
        void *p = ::realloc(ptr, size);
        if (p == nullptr)
            scritical("allocating memory of size %lu failed: %s", size, errno_s);
        return p;
    }

    void memory::free(void *ptr) { ::free(ptr); }
}