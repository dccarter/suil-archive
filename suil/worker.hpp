//
// Created by dc on 20/06/17.
//

#ifndef SUIL_WORKER_HPP
#define SUIL_WORKER_HPP

#include "sys.hpp"
#include "log.hpp"

namespace suil {

    extern const bool& started;

    struct __lock_t {
        volatile    uint64_t serving{0};
        volatile    uint64_t next{0};
        uint8_t     on;
    } __attribute((packed));

    struct lock {
        static void reset(__lock_t& lk) {
            lk.serving = 0;
            lk.next  = 0;
            lk.on    = 0x0A;
        }

        static void cancel(__lock_t& lk) {
            lk.on = 0x00;
        }

        lock(__lock_t& lk)
            : lk(lk)
        {
            spin_lock(lk);

            strace("(%ld) lock %p acquired by suil-%hhu",
                   mnow(), &lk, spid);
        }

        ~lock() {
            unlock(lk);
            strace("(%ld) lock %p released by suil-%hhu",
                   mnow(), &lk, spid);
        }

    private:

        friend struct worker;
        static inline void spin_lock(__lock_t& l) {
            int ticket = __sync_fetch_and_add(&l.next, 1);
            // while our ticket is not being served we yield
            // otherwise we continue
            while (l.on && !__sync_bool_compare_and_swap(&l.serving, ticket, ticket)) {
                strace("lock (%ld) %p ticket %d,  next %d serving %d",
                       mnow(), &l, ticket, l.next, l.serving);

                yield();

                strace("lock (%ld) %p ticket %d,  next %d serving %d",
                       mnow(), &l, ticket, l.next, l.serving);
            }
        }

        static inline void unlock(__lock_t& l) {
            // serve the next waiting
            (void) __sync_fetch_and_add(&l.serving, 1);
        }
        __lock_t& lk;
    };

    define_log_tag(WORKER);

    struct ipc_msg_hdr {
        uint8_t     id;
        uint8_t     src;
        size_t      len;
    } __attribute((packed));

    enum sys_msg_t : uint8_t {
        PING        = 0,
        PONG_REPLY,
        SYSTEM = 64
    };

    struct icp_ping_hdr {
        async_t<bool>       *async;
        int64_t             timeout;
        size_t              nbytes;
        uint8_t             data[0];
    } __attribute((packed));

    #define icp_msg(msg)  suil::sys_msg_t::SYSTEM + msg

    using msg_handler_t = std::function<void(uint8_t, const void*, size_t)>;
    using work_t = std::function<int(void)>;

    using post_spawn_t = std::function<int(uint8_t)>;

    struct worker {

        static int spawn(uint8_t, work_t, post_spawn_t ps = nullptr, post_spawn_t pps = nullptr);

        static ssize_t send(uint8_t, uint8_t, const void *, size_t len);

        static inline ssize_t send(uint8_t dst, uint8_t msg, const char *str) {
            return send(dst, msg, str, strlen(str));
        }

        static inline ssize_t send(uint8_t dst, uint8_t msg, const buffer_t& b) {
            return send(dst, msg, b.data(), b.size());
        }

        static inline ssize_t send(uint8_t dst, uint8_t msg, const zcstring& str) {
            return send(dst, msg, str.cstr, str.len);
        }

        static inline ssize_t send(uint8_t dst, uint8_t msg, strview_t& sv) {
            return send(dst, msg, sv.data(), sv.size());
        }

        static void broadcast(uint8_t, const void *, size_t);

        static uint8_t broadcast(async_t<int>& ch, uint8_t msg, const void *data, size_t len);

        static inline void broadcast(uint8_t msg, const char *str) {
            broadcast(msg, str, strlen(str));
        }

        static inline void broadcast(uint8_t msg, const buffer_t& b) {
            broadcast(msg, b.data(), b.size());
        }

        static inline void broadcast(uint8_t msg, const zcstring& str) {
            broadcast(msg, str.cstr, str.len);
        }

        static inline void broadcast(uint8_t msg, const strview_t& sv) {
            broadcast(msg, sv.data(), sv.size());
        }

        static void ipcreg(uint8_t, msg_handler_t);

        static void ipcunreg(uint8_t);

        static void spinlock(const uint8_t idx);
        static void spinunlock(const uint8_t idx);
    };

    enum {
        SHM_LOCK_ACCEPT = 0,
    };

    using app = std::function<void(void)>;
    extern int launch(app a, int argc, char *argv[]);
}

#endif //SUIL_WORKER_HPP
