//
// Created by dc on 12/02/19.
//
#include <sys/shm.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <fcntl.h>

#include <suil/worker.h>

#ifndef WORKER_DATA_SIZE
#define WORKER_DATA_SIZE    256
#endif

#ifndef WORKER_SHM_LOCKS
#define WORKER_SHM_LOCKS    64
#endif


namespace suil {

    uint8_t g_spid{0};
    const uint8_t& spid = g_spid;

    struct Worker_t {
        Lock_t      Lock;
        int         Fd[2];
        pid_t       Pid;
        uint8_t     Cpu;
        uint8_t     Wid;
        uint8_t     Active;
        uint8_t     Data[WORKER_DATA_SIZE];
    } __attribute__((packed));

    struct Ipc_t {
        uint8_t     nWorkers;
        uint8_t     nActive;
        Lock_t      Locks[WORKER_SHM_LOCKS];
        Worker_t    Workers[0];
    } __attribute__((packed));

    define_log_tag(WORKER);
    static struct : public LOGGER(WORKER) {} wLog;
    static auto* WLOG{&wLog};

    static Ipc_t    *mIpc{nullptr};
    static int      mShmId{0};
    static int      mWorkers{0};
    static bool     mLaunched{false};
    static int      nLaunched{0};
    static uint16_t mLaunchFlags{0};

    static inline void setnon_blocking(int fd) {
        int opt = fcntl(fd, F_GETFL, 0);
        if (opt == -1)
            opt = 0;
        if(fcntl(fd, F_SETFL, opt | O_NONBLOCK) < 0)
            lcritical(WLOG, "setting fd(%d) non-block failed: %s", fd, errno_s);
    }

    static int waitRead(int fd, int64_t timeout = -1) {
        int64_t tmp = timeout < 0? -1 : mnow() + timeout;
        int events = fdwait(fd, FDW_IN, tmp);
        if (events&FDW_ERR) {
            return -1;
        } else if (events&FDW_IN) {
            return 0;
        }

        return ETIMEDOUT;
    }

    static int waitWrite(int fd, int64_t timeout = -1) {
        int64_t tmp = timeout < 0? -1 : mnow() + timeout;
        int events = fdwait(fd, FDW_OUT, tmp);
        if (events&FDW_ERR) {
            return -1;
        } else if (events&FDW_OUT) {
            return 0;
        }

        return ETIMEDOUT;
    }

    static volatile sig_atomic_t sigrecv;
    static void onSignal(int sig) {
        ldebug(WLOG, "received signal %d", sig);
        if (g_spid) {
            if ((g_spid == 1) && (sig == SIGCHLD)) {
                ldebug(WLOG, "parent worker, ignoring signal");
                return;
            }
            Worker::exit(0);
        }
    }

    void Parent_sa_handler(int sig, siginfo_t *info, void *context) {
        ldebug(WLOG, "received signal=%d for pid=%d", sig, info->si_pid);
        if ((sig == SIGCHLD) && mIpc) {
            // mark worker associated with Pid as non active
            for (int i = 1; i < mIpc->nWorkers; i++) {
                Worker_t& worker = mIpc->Workers[i];
                if (worker.Pid == info->si_pid) {
                    ldebug(WLOG, "worker/%hhu exiting...", worker.Wid);
                    worker.Active = 0;
                    __sync_sub_and_fetch(&mIpc->nActive, 1);
                    break;
                }
            }
        }
    }

    void Worker_sa_handler(int sig, siginfo_t *info, void *context) {
        ldebug(WLOG, "parent received signal sig=%d pid=%d", sig, info->si_pid);
        if ((sig == SIGCHLD) && mIpc) {
            // mark worker associated with Pid as none active
            auto& worker = mIpc->Workers[0];
            if (worker.Pid == info->si_pid) {
                ldebug(WLOG, "worker/%hhu exiting...", worker.Wid);
                worker.Active = 0;
                __sync_sub_and_fetch(&mIpc->nActive, 1);
            }

            ldebug(WLOG, "exiting last worker %hhu", __sync_sub_and_fetch(&mIpc->nActive, 0));
            Worker::exit(info->si_code);
        }
    }

    static coroutine void asyncReceive(Worker_t& worker){}

    static int initializeWorkers(int count)
    {
        int status = 0;

        if (mIpc != nullptr) {
            lerror(WLOG, "system supports only 1 worker group");
            return -ENOTSUP;
        }

        int quit = 0;
        uint8_t cpu = 0;
        // get number of CPU's
        long ncpus = sysconf(_SC_NPROCESSORS_ONLN);
        if (count > ncpus)
            lwarn(WLOG, "number of workers more than number of CPU's");

        // create our worker's ipc
        size_t len = sizeof(Ipc_t) + (sizeof(Worker_t) * count);
        mShmId =  shmget(IPC_PRIVATE, len, IPC_EXCL | IPC_CREAT | 0700);
        if (mShmId == -1)
            lcritical(WLOG, "shmget() error: %s", errno_s);

        void *shm = shmat(mShmId, NULL, 0);
        if (shm == (void *) -1) {
            lerror(WLOG, "shmat() error: %s", errno_s);
            status = -errno;
            goto ipc_dealloc;
        }
        mIpc = (Ipc_t *)shm;
        // clear the attached memory
        memset(mIpc, 0, len);
        // initialize accept lock
        for (uint8_t i = 0; i < WORKER_SHM_LOCKS; i++)
            Lock::reset(mIpc->Locks[i], 256+i);

        mIpc->nWorkers = (uint8_t) count;
        mIpc->nActive = 0;

        for (uint8_t w = 0; w < count; w++) {
            Worker_t& wrk = mIpc->Workers[w];
            Lock::reset(wrk.Lock, w);
            wrk.Wid = (uint8_t) (w+1);
            wrk.Cpu = cpu;

            if (count)
                cpu = (uint8_t) ((cpu +1) % ncpus);
        }
        nLaunched = 1;
        return 0;

    ipc_dealloc:
        shmctl(mShmId, IPC_RMID, 0);
        if (status)
            lcritical(WLOG, "initializing workers failed, use Worker::exit(code) to signal parent");
        mShmId = 0;
        return status;
    }

    static void initializeIpc(Worker_t& worker)
    {
        // set worker process name
        char name[16];
        g_spid = worker.Wid;

        snprintf(name, sizeof(name)-1, "worker/%hhu", spid);
        prctl(PR_SET_NAME, name);

        if (mIpc->nWorkers > 1) {
            // set process affinity
            cpu_set_t mask;
            CPU_ZERO(&mask);
            CPU_SET(worker.Cpu, &mask);
            sched_setaffinity(0, sizeof(mask), &mask);
            ldebug(WLOG, "worker/%hhu scheduled on cpu %hhu", spid, worker.Cpu);

            // Setup pipe's
            for (uint8_t i = 0; i < mIpc->nWorkers; i++) {
                Worker_t &tmp = mIpc->Workers[i];
                if (tmp.Wid == spid) {
                    // close the writing end for current process
                    close(tmp.Fd[1]);
                    setnon_blocking(tmp.Fd[0]);
                } else {
                    // close the reading ends for all the other processes
                    close(tmp.Fd[0]);
                    setnon_blocking(tmp.Fd[1]);
                }
            }
        }
        worker.Active = 1;
    }

    static int workerWait(bool quit) {
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

        for (uint8_t w = 0; w < mIpc->nWorkers; w++) {
            Worker_t& wrk = mIpc->Workers[w];
            if (wrk.Pid != pid) continue;

            ltrace(WLOG, "worker/%hhu exiting, status %d", wrk.id, status);
            wrk.Active = 0;
            break;
        }

        return 0;
    }


    int Worker::init(int count, uint16_t flags)
    {
        if (mLaunched) {
            lerror(WLOG, "system already initialized with workers");
            return -EINPROGRESS;
        }

        ldebug(WLOG, "initializing system with %d workers", count);
        if (count == 0) {
            lerror(WLOG, "cannot initialize 0 workers");
            return -EINVAL;
        }

        mWorkers = count;
        mLaunchFlags = flags;

        initializeWorkers(count);
        __sync_fetch_and_add(&mIpc->nActive, 1);

        auto pid = mfork();
        if (pid < 0) {
            // failed to fork worker process
            lerror(WLOG, "failed: %s", errno_s);
            return -errno;
        }
        else if (pid == 0) {
            // workers will be launched
            struct sigaction sa{0};
            signal(SIGHUP,  onSignal);
            signal(SIGQUIT, onSignal);
            signal(SIGTERM, onSignal);
            signal(SIGINT,  onSignal);
            signal(SIGPIPE, SIG_IGN);

            sa.sa_sigaction = &Parent_sa_handler;
            sa.sa_flags = SA_SIGINFO | SA_NOCLDWAIT | SA_NOCLDSTOP;
            if (sigaction(SIGCHLD, &sa, nullptr) == -1) {
                // error subscribing to child process exit
                serror("sigaction failed: %s", errno_s);
                return errno;
            }

            g_spid = 1;
            return 1;
        }
        else {
            // parent process
            ldebug(WLOG, "system initialized with %d workers, use Worker::launch() api to launch the workers",
                   count);
            g_spid = 0;
            signal(SIGPIPE, SIG_IGN);
            return 0;
        }
    }

    uint8_t Worker::launch()
    {
        if (mLaunched == mWorkers) {
            lwarn(WLOG, "workers already launched");
            return 0;
        }

        if (mIpc->nWorkers > 1) {
            // open communication pipes for the workers
            for (int w = 0; w < mIpc->nWorkers; w++) {
                auto& wrk = mIpc->Workers[w];
                if (pipe(wrk.Fd) < 0) {
                    lerror(WLOG, "ipc - opening pipes for wrk-%hhu failed: %s",
                           w, errno_s);
                    Worker::exit(errno, false);
                }
            }
        }
        else {
            // if there is only 1 worker, there will be no IPC between the workers
            ldebug(WLOG, "only 1 worker requested, IPC disabled for 1 worker");
        }

        // spawn worker process
        Worker_t& worker = mIpc->Workers[0];
        bool parent{true};
        for (uint8_t w = 1; w < mWorkers; w++) {
            pid_t pid = mfork();
            if (pid < 0) {
                lerror(WLOG, "spawning worker %hhu/%hhu failed: %s",
                       w, mWorkers, errno_s);
                return 0;
            }
            else if (pid == 0) {
                initializeIpc(mIpc->Workers[w]);
                __sync_fetch_and_add(&mIpc->nActive, 1);
                parent = false;
                break;
            }
            mIpc->Workers[w].Pid = pid;
        }

        if (parent) {
            // we need to do this after other processes have been forked
            initializeIpc(worker);
            worker.Pid = getpid();
        }

        if (mIpc->nWorkers > 1) {
            // IPC enabled if only more than 1 worker is enabled
            go (asyncReceive(worker));
        }

        return worker.Wid;
    }

    int Worker::exit(int code, bool wait)
    {
        if (spid == 0) {
            // super process
            ldebug(WLOG, "requested exit with code %d", code);
            if ((mIpc != nullptr) && __sync_sub_and_fetch(&mIpc->nActive, 0)) {

                if (!wait) {
                    for (uint8_t w = 0; w <= mWorkers; w++) {
                        // pass signal through to worker processes
                        Worker_t &tmp = mIpc->Workers[w];
                        if (kill(tmp.Pid, SIGABRT) == -1) {
                            ldebug(WLOG, "kill worker/%hhu failed: %s", tmp.Pid, errno_s);
                        }
                    }
                }

                uint8_t done = 0;
                while (done < mIpc->nWorkers) {
                    // wait for workers to shutdown
                    for (uint8_t w = 0; w <= mWorkers; w++) {
                        // pass signal through to worker processes
                        Worker_t &tmp = mIpc->Workers[w];
                        if (tmp.Active) {
                            if (workerWait(true) == ECHILD) {
                                break;
                            }
                            ldebug(WLOG, "done waiting...");
                            done++;
                        }
                    }
                }
            }

            ldebug(WLOG, "parent process done");
            // cleanup shared memory
            shmdt(mIpc);
            mIpc = nullptr;
            if (shmctl(mShmId, IPC_RMID, 0))
                lerror(WLOG, "shmctl(IPC_RMID) failed: %s", errno_s);
        }
        else {
            ldebug(WLOG, "requested exit with code %d", code);
            ::exit(code);
        }

        return code;
    }

    uint8_t Worker::wpid() {
        return spid;
    }

    void Lock::reset(Lock_t& lk, uint32_t id) {
        lk.Serving = 0;
        lk.Next  = 0;
        lk.On    = 0x0A;
        lk.Id    = id;
    }

    void Lock::cancel(Lock_t& lk) {
        lk.On = 0x00;
    }

    Lock::Lock(Lock_t& lk)
        : lk(lk)
    {
        spin_lock(lk);

        strace("{%ld} lock-%u acquired by suil-%hhu",
        mnow(), lk.Id, spid);
    }

    Lock::~Lock() {
        unlock(lk);
        strace("{%ld} lock-%d released by suil-%hhu",
               mnow(), lk.Id, spid);
    }

    bool Lock::spin_lock(Lock_t& l, int64_t tout) {
        bool status{true};
        /* Request lock ticket */
        int ticket = __sync_fetch_and_add(&l.Next, 1);

        strace("{%ld} lock-%d Request (ticket %d,  next %d serving %d)",
               mnow(), l.id, ticket, l.next, l.serving);
        if (tout > 0) {
            /* compute the time at which we should giveup */
            tout += mnow();
        }

        while (l.On && !__sync_bool_compare_and_swap(&l.Serving, ticket, ticket))
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

    void Lock::unlock(Lock_t& l) {
        // serve the next waiting
        (void) __sync_fetch_and_add(&l.Serving, 1);
    }
}