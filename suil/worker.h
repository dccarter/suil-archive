//
// Created by dc on 12/02/19.
//

#ifndef SUIL_WORKER_H
#define SUIL_WORKER_H

#include <suil/base.h>
#include <suil/logging.h>

namespace suil {

    struct Lock_t {
        volatile    uint64_t Serving{0};
        volatile    uint64_t Next{0};
        uint8_t     On;
        uint32_t    Id;
    } __attribute((packed));

    struct Lock {
        static void reset(Lock_t& lk, uint32_t id);

        static void cancel(Lock_t& lk);

        Lock(Lock_t& lk);

        ~Lock();

    private:

        friend struct Worker;
        static bool spin_lock(Lock_t& l, int64_t tout = -1);

        static void unlock(Lock_t& l);

        Lock_t& lk;
    };

    struct Worker {
        enum : uint16_t  {
            IPCDisabled    = 0x0001
        };

        static int init(int count, uint16_t flags = 0);
        static uint8_t launch();
        static uint8_t wpid();
        static int exit(int code = 0, bool wait = true);
    };
}

#endif //SUIL_WORKER_H
