//
// Created by dc on 21/02/19.
//

#ifndef SUIL_LXC_H
#define SUIL_LXC_H

#include <lxc/lxccontainer.h>

#include <suil/zstring.h>
#include <suil/logging.h>

#include "lxc.scc.h"

namespace suil {

    define_log_tag(SUIL_LXC);

    namespace lxc {

        struct ConsoleFd {
            ConsoleFd() = default;

            ConsoleFd(int fd, int ttynum, int master);

            ConsoleFd(const ConsoleFd&) = delete;

            ConsoleFd& operator=(const ConsoleFd&) = delete;

            ConsoleFd(ConsoleFd&& other) noexcept
                    : Fd(other.Fd),
                      TtyNum(other.TtyNum),
                      MasterFd(other.MasterFd)
            {
                other.MasterFd = -1;
                other.TtyNum = -1;
                other.Fd = -1;
            }

            ConsoleFd& operator=(ConsoleFd&& other) noexcept {
                if (this != &other) {
                    MasterFd  = other.MasterFd ;
                    TtyNum  = other.TtyNum ;
                    Fd  = other.Fd ;
                    other.MasterFd = -1;
                    other.TtyNum = -1;
                    other.Fd = -1;
                }

                return Ego;
            }

            operator bool() {
                return Fd != -1;
            }

            ~ConsoleFd() {
                throw Exception::create("lxd::ConsoleFd::~ConsoleFd not implemented");
            }


            int     Fd{-1};
            int     TtyNum{-1};
            int     MasterFd{-1};
        };

        using AttachExec = lxc_attach_exec_t;
        using AttacnEvnPolicy = lxc_attach_env_policy_t;
        using NativeAttachOptions = lxc_attach_options_t;
        using NativeSnapshot = struct lxc_snapshot;
        using NativeMigrateOpts = struct migrate_opts;
        using NativeConsoleLog = struct lxc_console_log;
        using NativeBdevSpecs = struct bdev_specs;

        struct BdevSpecsOps: Ref<BdevSpecs>  {
            BdevSpecsOps(lxc::BdevSpecs& ref)
                : Ref(ref)
            {}

            NativeBdevSpecs* toNative(NativeBdevSpecs& specs);
        };

        struct AttachOptionsOps: Ref<AttachOptions> {
            AttachOptionsOps(lxc::AttachOptions& ref)
                : Ref(ref)
            {}

            NativeAttachOptions* toNative(NativeAttachOptions& native);
        };

        struct SnapshotOps: Ref<Snapshot> {
            SnapshotOps(Snapshot& snapshot, NativeSnapshot* from = nullptr)
                : Ref(snapshot)
            {
                if (from) {
                    init(from);
                }
            }

            NativeSnapshot* toNative(NativeSnapshot& native);

        private:
            void init(NativeSnapshot* from);
        };

        struct MigrateOptsOps: Ref<MigrateOpts> {
            MigrateOptsOps(MigrateOpts& ref, NativeMigrateOpts* from = nullptr)
                : Ref(ref)
            {
                if (from) {
                    init(from);
                }
            }

            NativeMigrateOpts *toNative(NativeMigrateOpts& native) {
                throw Exception::create("lxd::ConsoleFd::~ConsoleFd not implemented");
            }

        private:
            void init(NativeMigrateOpts* from) {
                throw Exception::create("lxd::ConsoleFd::~ConsoleFd not implemented");
            }
        };
    }

    struct Container {

        typedef struct ::lxc_container* Handle ;

        Container(Handle handle)
            : mHandle(handle)
        {}

        Container(const Container&) = delete;

        Container& operator=(const Container&) = delete;

        Container(Container&& other)
            : mHandle(other.mHandle)
        {
            other.mHandle = nullptr;
        }

        Container& operator=(Container&& other)
        {
            if (this != &other) {
                mHandle = other.mHandle;
                other.mHandle = nullptr;
            }
            return *this;
        }

        String getName() const;

        String getPidFile() const;

        String getConfigFile() const;

        int getNumThreads() const;

        String getErrorString() const;

        int    getErrorNum() const;

        bool   isDemonize() const;

        operator bool();

        String getState() const;

        bool isRunning() const;

        void freeze();

        void unfreeze();

        pid_t getInitPid() const;

        bool loadConfig(const char *altFile);

        bool start(bool useinit, const char *argv[]);

        bool stop();

        bool wantDaemonize(bool state);

        bool wantCloseAllFds(bool state);

        bool wait(const char *state, int timeout);

        bool setConfigItem(const char *key, const char *value);

        bool destroy(bool withSnapshots = false);

        bool saveConfig(const char *altFile);

        bool rename(const char* newName);

        bool reboot(bool sigint = false, int timeout = 0);

        bool shutdown(int timeout);

        void clearConfig();

        bool clearConfigItem(const char *key);

        String getConfigItem(const char *key) const;

        String getRunningConfigItem(const char *key) const;

        std::vector<String> getKeys(const char *prefix) const;

        std::vector<String> getInterfaces() const;

        std::vector<String> getIps(const char* interface, const char* family, int scope) const;

        bool setCgroupItem(const char *subsys, const char *value);

        String getCgroupItem(const char *subsys) const;

        String getConfigPath() const;

        bool   setConfigPath(const char *configPath);

        template <typename... Args>
        bool start(bool useinit, Args... args) {
            const char *argv[sizeof...(args) + 1] = {nullptr};
            Ego.pack(argv, 0, std::forward<Args>(args)..., nullptr);
            return Ego.start(useinit, argv);
        }

        bool start(bool useinit, std::vector<String> args) {
            const char *argv[args.size() + 1];
            Ego.pack(argv, args);
            return Ego.start(useinit, argv);
        }

        Container clone(const lxc::Clone& params, const char **hookargs) const;

        template <typename... Args>
        Container clone(const lxc::Clone& params, Args... args) const {
            const char *argv[sizeof...(args) + 1] = {nullptr};
            Ego.pack(argv, 0, std::forward<Args>(args)..., nullptr);
            return Ego.clone(params, argv);
        }

        Container clone(const lxc::Clone& params, std::vector<String> args) const {
            const char *argv[args.size() + 1];
            Ego.pack(argv, args);
            return Ego.clone(params, argv);
        }

        lxc::ConsoleFd consoleGetFd(int ttynum = -1) const;

        int console(int ttynum, int stdinfd, int stdoutfd, int stderrfd, int escape);

        pid_t attach(lxc::AttachOptions& options, void *payload, lxc::AttachExec func);

        int run(lxc::AttachOptions& options, const char *program, const char *argv[]);

        template <typename... Args>
        int run(lxc::AttachOptions& options, const char *program, Args... args) {
            const char *argv[1+sizeof...(args)] = {nullptr};
            Ego.pack(argv, 0, std::forward<Args>(args)...);
            return Ego.run(options, program, argv);
        }

        int run(lxc::AttachOptions& options, const char *program, std::vector<String>& args) {
            const char *argv[1+args.size()];
            Ego.pack(argv, args);
            return Ego.run(options, program, argv);
        }

        int snapshot(const char *commentFile);

        std::vector<lxc::Snapshot> listSnapshots();

        bool restoreSnapshot(const char *snapname, const char *newname);

        bool destroySnapshot(const char *snapname);

        bool mayControl() const;

        bool addDeviceNode(const char *src_path, const char *dest_path);

        bool removeDeviceNode(const char *src_path, const char *dest_path);

        bool attachInterface(const char *dev, const char *dst_dev);

        bool detachInterface(const char *dev, const char *dst_dev);

        bool checkpoint(char *directory, bool stop = false, bool verbose = false);

        bool restore(char *directory, bool verbose);

        bool destroyAllSnapshots();

        int migrate(uint32_t cmd, lxc::MigrateOpts& opts, uint32_t size = sizeof(lxc::NativeMigrateOpts));

        String readConsoleLog(size_t readMax = 0, bool clear = true);

        int clearConsoleLog(size_t clearMax = 0);

        void get();

        void put();

        ~Container();

    private:
        template <typename Arg, typename ...Args>
        void pack(const char *argv[], int i, Arg arg, Args... args) {
            argv[i] = arg;
            if constexpr (sizeof...(args)) {
                Ego.pack(argv, i+1, std::forward<Args>(args)...);
            }
        }

        template <typename Arg, typename ...Args>
        void pack(const char *argv[], int i, Arg arg, Args... args) const {
            argv[i] = arg;
            if constexpr (sizeof...(args)) {
                Ego.pack(argv, i+1, std::forward<Args>(args)...);
            }
        }

        void pack(const char *argv[], std::vector<String>& vec) {
            int i = 0;
            for (; i < vec.size(); i++)
                argv[i] = vec[i]();
            argv[i] = nullptr;
        }

        void pack(const char *argv[], std::vector<String>& vec) const {
            int i = 0;
            for (; i < vec.size(); i++)
                argv[i] = vec[i]();
            argv[i] = nullptr;
        }

        struct lxc_container *mHandle{nullptr};
    };

    namespace lxc {

        static Container create(lxc::Create& params, const char *configPath);

        Map<Container> listAllContainers(const char *lxcpath);

    };

}
#endif //SUIL_LXC_H
