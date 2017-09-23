//
// Created by dc on 31/05/17.
//

#include <sys/mman.h>
#include <sys/queue.h>
#include <sys/param.h>
#include <fcntl.h>

#include "sys.hpp"
#include "log.hpp"

namespace suil {
    define_log_tag(MEMORY);
    struct memory_priv_t : LOGGER(dtag(MEMORY)) {

        enum class elm_status : uint8_t {
            BUSY = 0,
            FREE = 1
        };

        struct region {
            LIST_ENTRY(region) list;
            uint8_t *start;
            size_t length;
        };

        struct entry {
            LIST_ENTRY(entry) list;
            elm_status state;
            region *reg;
        };

        memory_priv_t(const char *name, size_t len, size_t elm) {
            trace("new pool: name %s, len %lu elm %lu", name, len, elm);
            if ((this->name = strdup(name)) == nullptr) {
                critical("new pool, strdup: %s", errno_s);
            }
            lock = 0;
            elms = 0;
            inuse = 0;
            elen = len;
            slen = elen + sizeof(entry);

            LIST_INIT(&regions);
            LIST_INIT(&freelist);

            create_region(elm);
        }

        void create_region(size_t nelms) {
            size_t i;
            uint8_t *p;
            region *reg;
            entry *ent;

            trace("create_regions(%p, %zu)", this, nelms);

            if ((reg = (region *) ::calloc(1, sizeof(region))) == NULL) {
                critical("create_region: calloc: %s", errno_s);
            }

            LIST_INSERT_HEAD(&regions, reg, list);

            if (SIZE_MAX / nelms < slen) {
                critical("create_region: overflow");
            }

            reg->length = nelms * slen;
            reg->start = (uint8_t *) mmap(NULL, reg->length, PROT_READ | PROT_WRITE,
                                          MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

            if (reg->start == NULL) {
                critical("mmap: %s", errno_s);
            }

            p = reg->start;

            for (i = 0; i < nelms; i++) {
                ent = (entry *) p;
                ent->reg = reg;
                ent->state = elm_status::FREE;
                LIST_INSERT_HEAD(&freelist, ent, list);

                p = p + slen;
            }

            elms += nelms;
        }

        void destroy_region() {
            region *reg;

            trace("destroy_region(%p)", this);

            /* Take care iterating when modifying list contents */
            while (!LIST_EMPTY(&regions)) {
                reg = LIST_FIRST(&regions);
                LIST_REMOVE(reg, list);
                (void) munmap(reg->start, reg->length);
                ::free(reg);
            }

            /* Freelist references into the regions memory allocations */
            LIST_INIT(&freelist);
            elms = 0;
        }

        ~memory_priv_t() {
            lock = 0;
            elms = 0;
            inuse = 0;
            elen = 0;
            slen = 0;

            if (name != NULL) {
                ::free(name);
                name = NULL;
            }
            destroy_region();
        }

        size_t elen;
        size_t slen;
        size_t elms;
        size_t inuse;
        volatile int lock;
        char *name;

        LIST_HEAD(, region) regions;
        LIST_HEAD(, entry) freelist;
    };

    memory::pool::pool(const char *name, size_t len, size_t elm)
        : priv_(new memory_priv_t(name, len, elm))
    {}

    void* memory::pool::get() {
        uint8_t			*ptr;
        memory_priv_t::entry	*entry;

        if (LIST_EMPTY(&(priv_->freelist))) {
            linfo(priv_, "pool %s is exhausted (%zu/%zu)",
                  priv_->name, priv_->inuse, priv_->elms);
            priv_->create_region(priv_->elms);
        }

        entry = LIST_FIRST(&(priv_->freelist));
        if (entry->state != memory_priv_t::elm_status::FREE) {
            lcritical(priv_, "%s: element %p was not free", priv_->name, entry);
        }

        LIST_REMOVE(entry, list);

        entry->state = memory_priv_t::elm_status::BUSY;
        ptr = (uint8_t *) entry + sizeof(memory_priv_t::entry);
        priv_->inuse++;

        ltrace(priv_, "pool got: %p %zu", ptr, priv_->elen-16);
        return (ptr);
    }

    void memory::pool::put(void *ptr) {
        memory_priv_t::entry		*entry;
        ltrace(priv_, "pool put: %p %zu", ptr, priv_->elen-16);
        entry = (memory_priv_t::entry *)
                ((uint8_t *)ptr - sizeof(memory_priv_t::entry));

        if (entry->state != memory_priv_t::elm_status::BUSY) {
            scritical("%s: element %p was not busy", priv_->name, ptr);
        }

        entry->state = memory_priv_t::elm_status::FREE;
        LIST_INSERT_HEAD(&(priv_->freelist), entry, list);
        priv_->inuse--;
    }

    memory::pool::~pool() {
        if (priv_) {
            delete priv_;
            priv_ = nullptr;
        }
    }

    struct meminfo {
        uint16_t        magic;
        uint16_t        len;
    };

    #define MEM_BLOCKS			    11
    #define MEM_BLOCK_SIZE_MAX		8912
    #define MEM_BLOCK_PREALLOC		64
    #define MEM_ALIGN		        (sizeof(size_t))
    #define MEM_MAGIC		        0xd0d0
    #define MEMINFO(x)		        ((meminfo *)(((uint8_t *)x)-sizeof(meminfo)))
    #define MEMSIZE(x)              MEMINFO(x)->len

    struct memblock {
        memory::pool    *p;
        size_t          allocs;
        size_t          frees;

        static inline size_t index(size_t len) {
            size_t		mlen, idx;
            idx = 0;
            mlen = 8;
            while (mlen < len) {
                idx++;
                mlen = mlen << 1;
            }

            if (idx > (MEM_BLOCKS - 1))
                scritical("memblock::index idx %lu too high, len %lu", idx, len);

            return (idx);
        }
    };

    static struct {
        bool     initialized{false};
        memblock blocks[MEM_BLOCKS];
        size_t   allocs{0};
        size_t   alloc_miss{0};
        size_t   frees{0};
    } s_MEMORY;

    void memory::init() {
        int		i, len;
        char		name[32];
        uint32_t	size, elm, mlen;
        size = 8;

        if (s_MEMORY.initialized) {
            swarn("system memory already initialized\n");
            return;
        }

        s_MEMORY.frees  = 0;
        s_MEMORY.allocs = 0;
        s_MEMORY.alloc_miss = 0;

        for (i = 0; i < MEM_BLOCKS; i++) {
            len = snprintf(name, sizeof(name), "block-%u", size);
            if (len == -1 || (size_t)len >= sizeof(name)) {
                scritical("memory::init: snprintf");
            }

            elm = (MEM_BLOCK_PREALLOC * 1024) / size;
            mlen = size + sizeof(struct meminfo) + MEM_ALIGN;
            mlen = mlen & ~(MEM_ALIGN - 1);

            s_MEMORY.blocks[i].p = new memory::pool(name, mlen, elm);
            s_MEMORY.blocks[i].allocs = 0;
            s_MEMORY.blocks[i].frees = 0;
            size = size << 1;
        }

        s_MEMORY.initialized = true;
    }

    void memory::cleanup() {
        int		i;
        for (i = 0; i < MEM_BLOCKS; i++) {
            // deleting the pool will clean it up
            delete s_MEMORY.blocks[i].p;
        }
        memset(&s_MEMORY, 0, sizeof(s_MEMORY));
    }

    void *memory::alloc(size_t len) {
        void    *ptr;
        meminfo		*mem;
        uint8_t	*addr;
        size_t	mlen, idx;

        if (len == 0)
            len = 8;

        if (len <= MEM_BLOCK_SIZE_MAX) {
            idx = memblock::index(len);
            ptr = s_MEMORY.blocks[idx].p->get();
            s_MEMORY.blocks[idx].allocs++;
        } else {
            mlen = sizeof(size_t) + len + sizeof(meminfo);
            if ((ptr = ::calloc(1, mlen)) == NULL) {
                scritical("memory::malloc(%zd): %d", len, errno);
            }
            s_MEMORY.alloc_miss++;
        }
        addr = (uint8_t *)ptr + sizeof(size_t);

        mem = MEMINFO(addr);
        mem->magic = MEM_MAGIC;
        mem->len   = (uint16_t)(8<<idx);

        s_MEMORY.allocs++;

        return (addr);
    }

    size_t memory::fits(void *ptr, size_t len) {
        meminfo *mem = MEMINFO(ptr);
        if (mem->magic != MEM_MAGIC)
            return 0;
        size_t sz = MEMSIZE(ptr);
        if (memblock::index(len) == memblock::index(sz)) {
            // memory block size
            return sz;
        }
        return 0;
    }

    void *memory::calloc(size_t memb, size_t len) {
        void		*ptr;
        size_t		total;

        if (SIZE_MAX / memb < len)
            scritical("memory::calloc(): memb * len > SIZE_MAX");

        total = memb * len;
        ptr = memory::alloc(total);
        memset(ptr, 0, total);
        return (ptr);
    }

    void *memory::realloc(void *ptr, size_t len) {
        meminfo		*mem;
        void			*nptr;

        if (ptr == NULL) {
            nptr = memory::alloc(len);
        } else {
            if (len <= MEMSIZE(ptr))
                return (ptr);
            mem = MEMINFO(ptr);
            if (mem->magic != MEM_MAGIC) {
                scritical("memory::realloc(): magic boundary not found");
            }

            nptr = memory::alloc(len);
            memcpy(nptr, ptr, MIN(len, MEMSIZE(ptr)));
            memory::free(ptr);
        }

        return (nptr);
    }

    void memory::free(void *ptr) {
        uint8_t		*addr;
        meminfo		*mem;
        size_t			len, idx;

        if (ptr == NULL)
            return;

        mem = MEMINFO(ptr);
        if (mem->magic != MEM_MAGIC) {
            scritical("memory::free(%p): magic boundary not found", ptr);
        }

        len = MEMSIZE(ptr);
        addr = (uint8_t *)ptr - sizeof(size_t);

        if (len <= MEM_BLOCK_SIZE_MAX) {
            idx = memblock::index(len);
            s_MEMORY.blocks[idx].p->put(addr);
            s_MEMORY.blocks[idx].frees++;
        } else {
            ::free(addr);
        }

        s_MEMORY.frees++;
    }

    memory::memory_info_t memory::get_usage() {
        pool_info_t   p;
        memory_info_t m;
        for (int i = 0; i < MEM_BLOCKS; i++) {
            std::string str = std::string(s_MEMORY.blocks[i].p->priv_->name);
            p.name    =  str;
            p.entries =  s_MEMORY.blocks[i].p->priv_->elms;
            p.allocs  =  s_MEMORY.blocks[i].allocs;
            p.frees   =  s_MEMORY.blocks[i].frees;
            m.pools.push_back(p);
        }

        m.worker = spid;
        m.allocs = s_MEMORY.allocs;
        m.frees  = s_MEMORY.frees;
        m.alloc_miss = s_MEMORY.alloc_miss;

        return m;
    }
}