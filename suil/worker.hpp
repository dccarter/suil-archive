//
// Created by dc on 20/06/17.
//

#ifndef SUIL_WORKER_HPP
#define SUIL_WORKER_HPP

#include <suil/sys.hpp>
#include <suil/log.hpp>

namespace suil {

    extern const bool& started;

    struct __lock_t {
        volatile    uint64_t serving{0};
        volatile    uint64_t next{0};
        uint8_t     on;
        uint32_t    id;
    } __attribute((packed));

    struct lock {
        static void reset(__lock_t& lk, uint32_t id) {
            lk.serving = 0;
            lk.next  = 0;
            lk.on    = 0x0A;
            lk.id    = id;
        }

        static void cancel(__lock_t& lk) {
            lk.on = 0x00;
        }

        lock(__lock_t& lk)
            : lk(lk)
        {
            spin_lock(lk);

            strace("{%ld} lock-%u acquired by suil-%hhu",
                   mnow(), lk.id, spid);
        }

        ~lock() {
            unlock(lk);
            strace("{%ld} lock-%d released by suil-%hhu",
                   mnow(), lk.id, spid);
        }

    private:

        friend struct worker;
        static inline bool spin_lock(__lock_t& l, int64_t tout = -1) {
            bool status{true};
            /* request lock ticket */
            int ticket = __sync_fetch_and_add(&l.next, 1);

            strace("{%ld} lock-%d request (ticket %d,  next %d serving %d)",
                   mnow(), l.id, ticket, l.next, l.serving);
            if (tout > 0) {
                /* compute the time at which we should giveup */
                tout += mnow();
            }

            while (l.on && !__sync_bool_compare_and_swap(&l.serving, ticket, ticket))\
            {
                if (tout > 0 && tout < mnow()) {
                    status = false;
                    break;
                }

                /*yield the CPU giving other tasks a chance to start*/
                yield();
            }

            strace("{%ld} lock %d issued (ticket %d,  next %d serving %d)",
                   mnow(), l.id, ticket, l.next, l.serving);

            return status;
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
        MSG_SYS_PING = 0,
#define SYS_PING                0
        MSG_GET_RESPONSE,
#define GET_RESPONSE            1
        MSG_GET_STATS,
#define GET_STATS               2
        MSG_GET_MEMORY_INFO,
#define GET_MEMORY_INFO         3
        MSG_SYSTEM = 64
#define SYSTEM                  64
    };

    using ipc_get_handle = async_t<void*, 16>;
    struct ipc_get_payload {
        ipc_get_handle  *handle;
        int64_t         deadline;
        size_t          size;
        uint8_t         data[0];
    } __attribute((packed));

    using ipc_get_handler_t = std::function<void(const void*, uint8_t)>;

    #define ipc_msg(msg)  suil::sys_msg_t::MSG_SYSTEM + msg

    using msg_handler_t = std::function<bool(uint8_t, void*, size_t)>;
    using work_t = std::function<int(void)>;

    using post_spawn_t = std::function<int(uint8_t)>;

    using cleanup_handler_t = std::function<void(void)>;

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

        static void register_cleaner(cleanup_handler_t);

        static bool spinlock(const uint8_t idx, int64_t tout = -1);

        static void spinunlock(const uint8_t idx);

        static network_buffer get(uint8_t msg, uint8_t w, int64_t tout = -1);

        static std::vector<network_buffer> gather(uint8_t msg, int64_t tout = -1);

        static void send_get_response(const void *token, uint8_t to, const void *data, size_t len);

        static void on_get(uint8_t msg, ipc_get_handler_t&& hdlr);
    };

    enum {
        SHM_ACCEPT_LOCK = 0,
        SHM_GET_LOCK,
        SHM_GATHER_LOCK
    };
}

#endif //SUIL_WORKER_HPP
