//
// Created by dc on 20/06/17.
//

#include <sys/shm.h>
#include <sys/param.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <fcntl.h>

#include "worker.hpp"

#ifndef WORKER_DATA_SIZE
#define WORKER_DATA_SIZE    256
#endif

#ifndef WORKER_SHM_LOCKS
#define WORKER_SHM_LOCKS    64
#endif

/*
 * On some systems WAIT_ANY is not defined
 * */
#ifndef WAIT_ANY
#define WAIT_ANY (-1)
#endif

namespace suil {
    spid_t __spid = 0;
    const spid_t& spid = __spid;
    bool __started = 0;
    const bool& started = __started;
    version_t __ver_json{};
    const version_t& ver_json = __ver_json;

#define SPID_PARENT 0

    struct __worker {
        int         fd[2];
        pid_t       pid;
        __lock_t    lock;
        uint64_t    mask[4];
        bool        active;
        uint8_t     cpu;
        uint8_t     id;
        uint8_t     data[WORKER_DATA_SIZE];
    } __attribute((packed));

    struct __ipc {
        uint8_t     nworkers;
        uint8_t     nactive;
        __lock_t    locks[WORKER_SHM_LOCKS];
        __worker    workers[0];
    } __attribute((packed));

    static __ipc   *ipc = nullptr;
    static int shm_ipc_id;

    struct __worker_logger : public LOGGER(dtag(WORKER)) {} __wlog;
    static __worker_logger* WLOG = &__wlog;

    static  msg_handler_t ipc_handlers[256] = {nullptr};

    static std::vector<cleanup_handler_t> cleaners;

    static inline void setnon_blocking(int fd) {
        int opt = fcntl(fd, F_GETFL, 0);
        if (opt == -1)
            opt = 0;
        if(fcntl(fd, F_SETFL, opt | O_NONBLOCK) < 0)
            lcritical(WLOG, "setting fd(%d) non-block failed: %s", fd, errno_s);
    }

    static inline bool has_msg_handler(uint8_t w, uint8_t m) {
        __worker& wrk   = ipc->workers[w];
        if(w <= ipc->nworkers && wrk.active) {
            uint8_t idx = (uint8_t) floor(m / 64);
            uint64_t mask = (uint64_t) 1 << (m % 64);
            uint64_t val = __sync_add_and_fetch(&wrk.mask[idx], 0);
            return (val & mask) == mask;
        }

        return false;
    }

    static inline void set_msg_handler(uint8_t w, uint8_t m, bool en) {
        if (w <= ipc->nworkers) {
            __worker &wrk = ipc->workers[w];
            uint8_t idx = (uint8_t) floor(m / 64);
            uint64_t mask = (uint64_t) 1 << (m % 64);
            if (en) {
                __sync_fetch_and_or(&wrk.mask[idx], mask);
            } else {
                __sync_fetch_and_and(&wrk.mask[idx], ~mask);
            }
        }
    }

    static volatile sig_atomic_t sig_recv;
    static void __on_signal(int sig) {
        sig_recv = sig;

        if (spid != 0 || ipc->nworkers == 0) {
            ltrace(WLOG, "worker worker/%hhu cleanup", spid);
            for (auto& cleaner : cleaners) {
                /* call cleaners */
                cleaner();
            }
        }
    }

    static coroutine void ipc_async_receive(__worker& wrk);

    static int recv_ipc_msg(__worker& wrk, int64_t dd);

    static void ipc_reg_get_resp();

    static int worker_wait(bool last);

    static coroutine void ipc_handle(msg_handler_t h, uint8_t src, void*, size_t);

    static inline int ipc_wait_read(int fd, int64_t timeout = -1) {
        int64_t tmp = timeout < 0? -1 : mnow() + timeout;
        int events = fdwait(fd, FDW_IN, tmp);
        if (events&FDW_ERR) {
            return -1;
        } else if (events&FDW_IN) {
            return 0;
        }

        return ETIMEDOUT;
    }

    static inline bool ipc_wait_write(int fd, int64_t timeout = -1) {
        int64_t tmp = timeout < 0? -1 : mnow() + timeout;
        int events = fdwait(fd, FDW_OUT, tmp);
        if (events&FDW_ERR) {
            return -1;
        } else if (events&FDW_OUT) {
            return 0;
        }

        return ETIMEDOUT;
    }

    static inline bool ipc_msg_check(__worker& wrk, uint8_t msg) {
    }

    static void ipc_init(__worker&  wrk) {
        // set the process id
        __spid = wrk.id;
        wrk.pid = getpid();

        // set worker process name
        char name[16];
        snprintf(name, sizeof(name)-1, "worker/%hhu", spid);
        prctl(PR_SET_NAME, name);

        // set process affinity
        if (spid != SPID_PARENT) {
            cpu_set_t mask;
            CPU_ZERO(&mask);
            CPU_SET(wrk.cpu, &mask);
            sched_setaffinity(0, sizeof(mask), &mask);
            ldebug(WLOG, "worker/%hhu scheduled on cpu %hhu", spid, wrk.cpu);
        }

        // Setup pipe's
        for (uint8_t w = 0; w <= ipc->nworkers; w++) {
            __worker& tmp = ipc->workers[w];
            if (w == spid) {
                // close the writing end for current process
                close(tmp.fd[1]);
                setnon_blocking(tmp.fd[0]);
            }
            else {
                // close the reading ends for all the other processes
                close(tmp.fd[0]);
                setnon_blocking(tmp.fd[1]);
            }
        }

        signal(SIGHUP,  __on_signal);
        signal(SIGQUIT, __on_signal);
        signal(SIGTERM, __on_signal);
        signal(SIGINT,  __on_signal);
        signal(SIGPIPE, SIG_IGN);

        wrk.active = true;

        if (spid != SPID_PARENT) {
            __sync_fetch_and_add(&ipc->nactive, 1);
            // start receiving messages
            go(ipc_async_receive(wrk));
        }

        __started = true;
    }


    int worker::spawn(uint8_t n, work_t work, post_spawn_t ps, post_spawn_t pps) {
        int status = 0;

        if (ipc || spid != 0 || work == nullptr) {
            lerror(WLOG, "system supports only 1 worker group");
            return ENOTSUP;
        }

        int quit = 0;
        uint8_t cpu = 0;
        // spawn different process
        long ncpus = sysconf(_SC_NPROCESSORS_ONLN);
        if (n > ncpus)
            lwarn(WLOG, "number of workers more than number of CPU's");

        // create our worker's ipc
        size_t len = sizeof(__worker) * (n+1);
        shm_ipc_id =  shmget(IPC_PRIVATE, len, IPC_EXCL | IPC_CREAT | 0700);
        if (shm_ipc_id == -1)
            lcritical(WLOG, "shmget() error: %s", errno_s);

        void *shm = shmat(shm_ipc_id, NULL, 0);
        if (shm == (void *) -1) {
            lerror(WLOG, "shmat() error: %s", errno_s);
            status = errno;
            goto ipc_dealloc;
        }

        ipc = (__ipc *) shm;

        // clear the attached memory
        memset(ipc, 0, len);
        // initialize accept lock
        for (uint8_t i = 0; i < WORKER_SHM_LOCKS; i++)
            lock::reset(ipc->locks[i], 256+i);

        // initialize worker memory and pipe descriptors
        ipc->nworkers = n;
        ipc->nactive = 0;

        for (uint8_t w = 0; w <= n; w++) {
            __worker& wrk = ipc->workers[w];
            lock::reset(wrk.lock, w);
            wrk.id = w;
            wrk.cpu = cpu;

            if (pipe(wrk.fd) < 0) {
                lerror(WLOG, "ipc - opening pipes for suil-%hhu failed: %s",
                        w, errno_s);
                status = errno;
                goto ipc_detach;
            }
            if (n)
                cpu = (uint8_t) ((cpu +1) % ncpus);
        }

        // spawn worker process
        for (uint8_t w = 1; w <= n; w++) {
            pid_t pid = mfork();
            if (pid < 0) {
                lerror(WLOG, "spawning worker %hhu/%hhu failed: %s",
                        w, n, errno_s);
                status = errno;
                goto ipc_detach;
            }
            else if (pid == 0) {
                ipc_init(ipc->workers[w]);
                break;
            }
        }

        sig_recv = 0;
        if (n == 0)
            ipc_init(ipc->workers[SPID_PARENT]);

        if (n == 0 || spid != SPID_PARENT) {
            lnotice(WLOG, "worker/%hhu started", spid);
            // if not parent handle work in continuous loop
            if (ps) {
                // start post spawn delegate
                try {
                    quit = ps(spid);
                    ipc_reg_get_resp();
                }
                catch(...) {
                    lerror(WLOG, "unhandled exception in post spawn delegate");
                    quit  = 1;
                }
            }

            while (sig_recv == 0 && !quit) {
                try {
                    quit = work();
                }
                catch (const std::exception &ex) {
                    if (sig_recv == 0)
                        lerror(WLOG, "unhandled error in work: %s", ex.what());
                    break;
                }
            }


            lnotice(WLOG, "worker/%hhu exit", spid, sig_recv);
            __sync_fetch_and_sub(&ipc->nactive, 1);

            // force worker to exit
            __started = false;
            if (n != 0)
                exit(0);
        }
        else {
            __worker& wrk = ipc->workers[SPID_PARENT];
            ipc_init(wrk);
            lnotice(WLOG, "parent{pid:%d} started", wrk.pid);

            if (pps) {
                // start post spawn delegate
                try {
                    sig_recv = pps(spid);
                }
                catch(...) {
                    lerror(WLOG, "unhandled exception in parent post spawn delegate");
                    sig_recv  = EXIT_FAILURE;
                }
            }

            // loop until a termination signal all workers exited
            while (!quit) {
                if (sig_recv) {
                    quit = 1;
                    for (uint8_t w = 1; w <= n; w++) {
                        // pass signal through to worker processes
                        __worker& tmp = ipc->workers[w];
                        if (kill(tmp.pid, SIGABRT) == -1) {
                            ldebug(WLOG, "kill worker/%hhu failed: %s", tmp.pid, errno_s);
                        }
                    }

                    uint8_t done = 0;
                    while (done < ipc->nworkers) {
                        // wait for workers to shutdown
                        for (uint8_t w = 1; w <= n; w++) {
                            // pass signal through to worker processes
                            __worker &tmp = ipc->workers[w];
                            if (tmp.active) {
                                if (worker_wait(true) == ECHILD) {
                                    break;
                                }
                                done++;
                            }
                        }
                    }
                    sig_recv = 0;
                }

                if (worker_wait(false) == ECHILD) quit = true;
                recv_ipc_msg(wrk, 500);
            }
            lnotice(WLOG, "parent exiting {exit_code:%d}", sig_recv);
            __started = false;
        }

    ipc_detach:
        shmdt(shm);
        ipc = nullptr;

    ipc_dealloc:
        shmctl(shm_ipc_id, IPC_RMID, 0);
        if (status)
            lcritical(WLOG, "worker::spawn failed");

        return 0;
    }

    ssize_t worker::send(uint8_t dst, uint8_t msg, const void *data, size_t len) {
        ltrace(WLOG, "worker::send - dst %hhu, msg %hhu, data %p, len %lu",
               dst, msg, data, len);
        errno = 0;

        if (spid == dst || dst > ipc->nworkers) {
            // prevent self destined messages
            lwarn(WLOG, "dst %hhu is an invalid send destination", dst);
            errno = EINVAL;
            return -1;
        }

        if (!has_msg_handler(dst, msg)) {
            // worker does not handle the given message
            ltrace(WLOG, "dst %hhu does not handle message %02X", dst, msg);
            errno = ENOTSUP;
            return -1;
        }

        ipc_msg_hdr hdr{};
        hdr.len = len;
        hdr.id = msg;
        hdr.src = spid;
        __worker &wrk = ipc->workers[dst];

        // acquire send lock of destination
        lock l(wrk.lock);

        ltrace(WLOG, "sending header %02X to worker %hhu", msg, dst);
        do {
            if (::write(wrk.fd[1], &hdr, sizeof(hdr)) <= 0) {
                int ws = 0;
                if (errno == EAGAIN || errno == -EWOULDBLOCK) {
                    ws = ipc_wait_write(wrk.fd[1]);
                    if (ws) {
                        lwarn(WLOG, "sending message header to %hhu failed: %s",
                              dst, errno_s);
                        return -1;
                    }
                    continue;
                }

                lwarn(WLOG, "sending message header to %hhu failed: %s",
                      dst, errno_s);
                return -1;
            }
            break;
        } while (1);

        ltrace(WLOG, "sending data of size %lu to worker %hhu", len, dst);
        ssize_t nsent = 0, tsent = 0;
        do {
            size_t chunk = MIN(len - tsent, PIPE_BUF);
            nsent = write(wrk.fd[1], ((char *) data) + tsent, chunk);
            if (nsent <= 0) {
                int ws = 0;
                if (errno == EAGAIN || errno == -EWOULDBLOCK) {
                    ws = ipc_wait_write(wrk.fd[1]);
                    if (ws) {
                        lwarn(WLOG, "sending message to %hhu failed: %s", dst, errno_s);
                        return -1;
                    }
                    continue;
                }

                lwarn(WLOG, "sending message header to %hhu failed: %s", dst, errno_s);
                return -1;
            }
            tsent += nsent;
        } while ((size_t) tsent < len);

        return tsent;
    }

    coroutine void async_send(async_t<int>& ch, uint8_t dst, uint8_t msg, const void *data, size_t len) {
        // send asynchronously
        ltrace(WLOG, "async_send ch %p, dst %hhu, msg 0x%hhX data %p, len %lu",
               &ch, dst, msg, data, len);

        if (worker::send(dst, msg, data, len) < 0) {
            lwarn(WLOG, "async send message 0x%hhX to %hhu failed", msg, dst);
        }

        if (ch) {
            ch << dst;
        }
    }

    coroutine void async_broadcast(uint8_t msg, void *data, size_t len) {
        // for each worker send the message
        int wait = 0;
        async_t<int> async(-1);

        // send asynchronously
        ltrace(WLOG, "async_broadcast msg %02X data %p, len %lu", msg, data, len);

        for(uint8_t i = 1; i <= ipc->nworkers; i++) {
            if (i != spid && has_msg_handler(i, msg)) {
                // send to all other worker who are interested in the message
                wait++;
                go(async_send(async, i, msg, data, len));
            }
        }

        ltrace(WLOG, "waiting for %d messages to be sent, %lu",  wait, mnow());
        async(wait) | [&](bool, const int dst) {
            ltrace(WLOG, "message %02X sent to %hhu", msg, dst);
        };

        ltrace(WLOG, "messages sent, free data dup %p, %lu",  data, mnow());
        // free the dup
        memory::free(data);
    }

    void worker::broadcast(uint8_t msg, const void *data, size_t len) {
        void *copy = memory::alloc(len);
        ltrace(WLOG, "worker::broadcast dup %p msg %02X, data %p len %lu",
               copy, msg, data, len);

        memcpy(copy, data, len);
        // start a go-routine to broadcast the message to all workers
        go(async_broadcast(msg, copy, len));
    }

    uint8_t worker::broadcast(async_t<int> &ch, uint8_t msg, const void *data, size_t len) {
        // send message to all workers
        int wait = 0;

        ltrace(WLOG, "worker::broadcast ch %p, msg %02X, data %p len %lu",
               &ch, msg, data, len);

        for(uint8_t i = 1; i <= ipc->nworkers; i++) {
            if (i != spid && has_msg_handler(i, msg)) {
                // send to all other worker who are interested in the message
                wait++;
                go(async_send(ch, i, msg, data, len));
            }
        }

        strace("sent out %d %02X ipc broadcast messages", wait, msg);

        return (uint8_t) wait;
    }

    coroutine void ipc_async_receive(__worker& wrk) {
        // loop until the worker terminates
        int status = 0;

        ltrace(WLOG, "ipc receive loop starting");
        while (wrk.active) {
            status = recv_ipc_msg(wrk, -1);
            if (status == EINVAL) {
                lwarn(WLOG, "receiving message failed, aborting");
                yield();
            }
        }
        ltrace(WLOG, "ipc receive loop exiting %d, %d", status, wrk.active);
    }

    int recv_ipc_msg(__worker& wrk, int64_t dd) {
        if (!wrk.active) {
            lwarn(WLOG, "read on an inactive worker not supported");
            return ENOTSUP;
        }

        ipc_msg_hdr hdr{0};
        ssize_t nread;
        int64_t fd = wrk.fd[0];

        // read header
        while (true) {
            nread = read(fd, &hdr, sizeof(hdr));
            if (nread <= 0) {
                int ws = errno;
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // ltrace(WLOG, "going to wait for ipc message hdr");
                    ws = ipc_wait_read(fd, dd);
                    if (ws == ETIMEDOUT) {
                        // it's ok to timeout
                        return ETIMEDOUT;
                    } else if (ws == 0) continue;
                }

                if (ws < 0 || errno == ECONNRESET || errno == EPIPE || nread == 0) {
                    ltrace(WLOG, "waiting for ipc header failed");
                    return EINVAL;
                }

                return errno;
            }
            break;
        }

        // header received, ensure that the message is supported
        if (!has_msg_handler(wrk.id, hdr.id)) {
            ltrace(WLOG, "received unsupported ipc message %hhu", hdr.id);
            return 0;
        }

        ltrace(WLOG, "received header [%02X|%02X|%08X]", hdr.id, hdr.src, hdr.len);
        // receive message body
        buffer_t b((uint32_t) hdr.len);
        void *ptr = b.data();
        size_t tread = 0;

        do {
            size_t len = MIN(PIPE_BUF, (hdr.len - tread));
            nread = read(fd, ((char *)ptr + tread), len);

            if (nread <= 0) {
                int ws = -errno;
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    ltrace(WLOG, "going to wait for ipc message hdr");
                    ws = ipc_wait_read(fd, dd);
                    if (ws == ETIMEDOUT) {
                        // it's ok to timeout
                        return ETIMEDOUT;
                    } else if (ws == 0) {
                        /* received read event */
                        continue;
                    }
                }

                if (ws < 0 || errno == ECONNRESET || errno == EPIPE || nread == 0) {
                    ltrace(WLOG, "waiting for ipc header failed: %s", errno_s);
                    return EINVAL;
                }
            }

            tread += nread;
        } while (tread < hdr.len);

        ltrace(WLOG, "received data [msg:%02X|src:%02X|len:%08X]", hdr.id, hdr.src, hdr.len);

        if (tread != hdr.len) {
            ldebug(WLOG, "reading message [msg:%02X|src:%02X|len:%08X] tread:%lu failed",
                   hdr.id, hdr.src, hdr.len, tread);
            return EINVAL;
        }

        b.seek(tread);
        // handle ipc message
        msg_handler_t h = ipc_handlers[hdr.id];
        (const char *) b;

        go(ipc_handle(h, hdr.src, b.release(), hdr.len));

        return 0;
    }

    int worker_wait(bool quit) {
        int status;
        pid_t pid;

        if (quit)
            pid = waitpid(WAIT_ANY, &status, 0);
        else
            pid = waitpid(WAIT_ANY, &status, WNOHANG);

        if (pid == -1) {
            ltrace(WLOG, "waitpid() failed: %s", errno_s);
            return ECHILD;
        }

        if (pid == 0) return 0;

        for (uint8_t w = 1; w <= ipc->nworkers; w++) {
            __worker& wrk = ipc->workers[w];
            if (wrk.pid != pid) continue;

            ltrace(WLOG, "worker/%hhu exiting, status %d", wrk.id, status);
            wrk.active = false;
            break;
        }

        return 0;
    }

    void worker::ipcreg(uint8_t msg, msg_handler_t h) {
        if (msg <= (1<<8)) {
            ipc_handlers[msg] = h;
            set_msg_handler(spid, msg, true);
        }
        else
            lerror(WLOG, "registering a message handler for out of bound message");
    }

    void worker::ipcunreg(uint8_t msg) {
        if (msg >= (1<<sizeof(msg))) {
            ipc_handlers[msg] = nullptr;
            set_msg_handler(spid, msg, false);
        }
        else
            lerror(WLOG, "un-registering a message handler for out of bound message");
    }

    bool worker::spinlock(const uint8_t idx, int64_t tout) {
        assert(idx < WORKER_SHM_LOCKS);
        return lock::spin_lock(ipc->locks[idx], tout);
    }

    void worker::spinunlock(const uint8_t idx) {
        assert(idx < WORKER_SHM_LOCKS);
        return lock::unlock(ipc->locks[idx]);
    }

    void worker::register_cleaner(cleanup_handler_t cleaner) {
        cleaners.insert(cleaners.begin(), cleaner);
    }

    network_buffer worker::get(uint8_t msg, uint8_t w, int64_t tout){
        if (w == SPID_PARENT || w == spid) {
            lerror(WLOG, "executing get to (%hhu) parent or current worker not supported",
                   w);
            return network_buffer();
        }

        ipc_get_handle async(nullptr);
        ipc_get_payload payload{};

        payload.handle = &async;
        payload.deadline = mnow() + tout;
        payload.size = 0;
        worker::send(w, msg, &payload, sizeof(payload));

        // receive the response
        void *data;
        ltrace(WLOG, "sent get request waiting for 1 response in %ld ms", tout);
        bool status = async[tout] >> data;

        network_buffer netbuf;
        if (status && data != nullptr) {
            /* successfully received data from worker */
            auto *resp = (ipc_get_payload *) data;
            ltrace(WLOG, "got response: data %p, size %lu", data, resp->size);
            return std::move(network_buffer(resp, resp->size, sizeof(ipc_get_payload)));
        } else {
            lerror(WLOG, "get from worker %hhu failed, msg %hhu", w, msg);
        }

        return netbuf;
    }

    std::vector<network_buffer> worker::gather(uint8_t msg, int64_t tout) {
        if (ipc->nworkers == 0) {
            lwarn(WLOG, "executing get %hhu not supported when no workers", msg);
            return std::vector<network_buffer>();
        }

        if (worker::spinlock(SHM_GATHER_LOCK, tout)) {
            /* buffered channel of up to 16 entries */
            ipc_get_handle async(nullptr);
            ipc_get_payload payload{};

            payload.handle = &async;
            payload.deadline = mnow() + tout;
            payload.size = 0;
            worker::broadcast(msg, &payload, sizeof(payload));

            // receive the response
            std::vector<network_buffer> all;
            ltrace(WLOG, "sent gather request waiting for %hhu responses in %ld ms",
                   ipc->nactive-1, tout);

            bool status = async[tout](ipc->nactive-1) |
            [&](bool /* unsused */, void *data) {
                auto *resp = (ipc_get_payload *) data;
                network_buffer res(resp, resp->size, sizeof(ipc_get_payload));
                all.push_back(std::move(res));
            };

            if (!status) {
                lwarn(WLOG, "gather failed, msg %hhu", msg);
            }

            worker::spinunlock(SHM_GATHER_LOCK);
            /* return gathered results */
            return std::move(all);
        } else {
            lwarn(WLOG, "acquiring SHM_GET_LOCK timed out");
            return std::vector<network_buffer>();
        }

    }

    void worker::send_get_response(const void *token, uint8_t to, const void *data, size_t len) {
        ltrace(WLOG, "token %p, to %hhu, data %p, len %lu", token, to, data, len);
        if (token == nullptr) {
            lerror(WLOG, "invalid token %p", token);
            return;
        }

        void *resp;
        bool allocd{false};
        size_t tlen = len + sizeof(ipc_get_payload);
        if (len < 256) {
            static char SEND_BUF[256+(sizeof(ipc_get_payload))];
            resp = SEND_BUF;
        }
        else {
            allocd = true;
            resp = memory::alloc(tlen);
        }

        auto *payload = (ipc_get_payload *)resp;
        memcpy(resp, token, sizeof(ipc_get_payload));
        payload->size = len;
        memcpy(payload->data, data, len);
        worker::send(to, GET_RESPONSE, resp, tlen);

        if (allocd) {
            /* free buffer if it was allocated */
            memory::free(resp);
        }
    }

    void worker::on_get(uint8_t msg, ipc_get_handler_t&& hdlr) {
        if (hdlr == nullptr) {
            lcritical(WLOG, "cannot register a null handler");
            return;
        }

        worker::ipcreg(msg,
        [&](uint8_t src, void *data, size_t/*len*/) {
            /* invoke the handler with the buffer as the token */
            hdlr(data, src);

            return false;
        });
    }

    void ipc_reg_get_resp() {
        worker::ipcreg(GET_RESPONSE,
        [&](uint8_t src, void *data, size_t /* len */) {
            auto *payload = (ipc_get_payload *) data;
            /* go 500 ms ahead in time */
            int64_t tmp = mnow() + 500;
            if (payload->deadline > tmp) {
                ltrace(WLOG, "got response in time deadline:%ld now:%ld",
                       payload->deadline, tmp);
                /*we haven't timed out waiting for response */
                ipc_get_handle *async = payload->handle;
                (*async) << data;

                return true;
            }
            else {
                lwarn(WLOG, "got response from %hhu after timeout dd %ld now %ld",
                      src, payload->deadline, tmp);
                return false;
            }
        });
    }

    coroutine void ipc_handle(msg_handler_t h, uint8_t src, void *data, size_t len)
    {
        bool kept{false};
        try {
            kept = h(src, data, len);
        } catch(std::exception& ex) {
            lwarn(WLOG, "un-handled exception in msg handler: %s", ex.what());
        }

        if (!kept) {
            /* memory released back to coroutine free */
            memory::free(data);
        }
    }
}