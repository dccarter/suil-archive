//
// Created by dc on 21/02/19.
//

#include "lxc.h"

namespace suil {

    static struct : LOGGER(SUIL_LXC) {} __LXCLOG;
    static auto* LXCLOG{&__LXCLOG};

    lxc::NativeBdevSpecs* lxc::BdevSpecsOps::toNative(lxc::NativeBdevSpecs &specs)
    {
        if (ref.Configured) {
            specs.dir = ref.Dir.data();
            specs.fssize = ref.FsSize;
            specs.fstype = ref.FsType.data();
            specs.lvm.lv = ref.LlvmLv.data();
            specs.lvm.vg = ref.LlvmVg.data();
            specs.lvm.thinpool = ref.LlvmThinpool.data();
            specs.zfs.zfsroot  = ref.ZfsRoot.data();
            specs.rbd.rbdname  = ref.RbdName.data();
            specs.rbd.rbdpool  = ref.RbdPool.data();

            return &specs;
        }
        else {
            return nullptr;
        }
    }

#define FromNativeStr(str) (str)? String(str).dup() : nullptr
#define ToNativeStr(str) str.data()

    void lxc::SnapshotOps::init(suil::lxc::NativeSnapshot *from) {
        Ego->LxcPath = FromNativeStr(from->lxcpath);
        Ego->Timestamp = FromNativeStr(from->timestamp);
        Ego->Name = FromNativeStr(from->name);
        Ego->CommentPath = FromNativeStr(from->comment_pathname);
    }

    lxc::NativeSnapshot* lxc::SnapshotOps::toNative(suil::lxc::NativeSnapshot &native) {
        native.name = ToNativeStr(Ego->Name);
        native.comment_pathname = ToNativeStr(Ego->CommentPath);
        native.timestamp = ToNativeStr(Ego->Timestamp);
        native.lxcpath = ToNativeStr(Ego->LxcPath);
        return &native;
    }

    lxc::NativeAttachOptions* lxc::AttachOptionsOps::toNative(lxc::NativeAttachOptions &native) {
        throw Exception::create("lxc::AttachOptionsOps::toNative not implemented");
    }

    Container lxc::create(suil::lxc::Create &params, const char *configPath)
    {
        struct bdev_specs tmp{};
        int    status{0};

        if (params.Name.empty() ||
            params.Distribution.empty() ||
            params.Release.empty() ||
            params.Arch.empty())
        {
            // name required
            lerror(LXCLOG, "Lxc::create required arguments. Name=%s, Dist=%s, Release=%s, Arch=%s",
                    params.Name(), params.Distribution(), params.Release(), params.Arch());
            return Container{nullptr};
        }

        Container::Handle handle = lxc_container_new(
                params.Name(), configPath);
        if (handle == nullptr) {
            // failed to create handle
            lerror(LXCLOG, "Lxc::create failed to create new container handle for Name=%s, configPath=%s",
                    params.Name(), configPath);
            return Container{nullptr};
        }

        if (handle->is_defined(handle)) {
            // container with given name already exists
            lerror(LXCLOG, "Lxc::create container with Name '%s' already exists", params.Name());
            goto Error;
        }

        status = handle->createl(
                handle, params.Template.c_str("download"),
                params.BdevType.c_str(nullptr),
                BdevSpecsOps(params.Specs).toNative(tmp),
                params.Flags||LXC_CREATE_QUIET,
                "-d", params.Distribution(),
                "-r", params.Release(),
                "-a", params.Arch(), nullptr);
        if (!status) {
            // creating container failed
            lerror(LXCLOG, "Lxc::create creating container failed: %s", handle->error_string);
            goto Error;
        }

        ldebug(LXCLOG, "Container '%s' successfully created", params.Name());
        return Container{handle};

    Error:
        lxc_container_put(handle);
        return Container{nullptr};
    }

#define ThrowInvalidHandle(handle) \
    if ((handle) == nullptr) \
        throw Exception::create((handle), "Container handle is not valid");

    Container::~Container() {
        if (mHandle != nullptr) {
            Ego.put();
            mHandle = nullptr;
        }
    }

    void Container::get() {
        if (mHandle) {
            int status = lxc_container_get(mHandle);
            if (status < 0) {
                lwarn(LXCLOG, "acquiring container reference failed");
            }
        }
    }

    void Container::put() {
        if (mHandle) {
            int status = lxc_container_put(mHandle);
            if (status < 0) {
                lwarn(LXCLOG, "dropping container reference failed");
            }
        }
    }

    String Container::getName() const {
        ThrowInvalidHandle(mHandle);
        return String(mHandle->name);
    }

    String Container::getConfigFile() const {
        ThrowInvalidHandle(mHandle);
        return String(mHandle->config_path);
    }

    String Container::getErrorString() const {
        ThrowInvalidHandle(mHandle);
        return String(mHandle->error_string);
    }

    String Container::getPidFile() const {
        ThrowInvalidHandle(mHandle);
        return String(mHandle->pidfile);
    }

    int Container::getErrorNum() const {
        ThrowInvalidHandle(mHandle);
        return mHandle->error_num;
    }

    String Container::getState() const {
        ThrowInvalidHandle(mHandle);
        return String(mHandle->state(mHandle));
    }

    int Container::getNumThreads() const {
        ThrowInvalidHandle(mHandle);
        return mHandle->numthreads;
    }

    pid_t Container::getInitPid() const {
        ThrowInvalidHandle(mHandle);
        return mHandle->init_pid(mHandle);
    }

    Container::operator bool() {
        return mHandle != nullptr &&
                mHandle->is_defined(mHandle);
    }

    void Container::freeze() {
        ThrowInvalidHandle(mHandle);
        mHandle->freeze(mHandle);
    }

    void Container::unfreeze() {
        ThrowInvalidHandle(mHandle);
        mHandle->unfreeze(mHandle);
    }

    bool Container::isRunning() const {
        ThrowInvalidHandle(mHandle);
        return mHandle->is_running(mHandle);
    }

    bool Container::isDemonize() const {
        ThrowInvalidHandle(mHandle);
        return mHandle->daemonize;
    }

    bool Container::loadConfig(const char *altFile) {
        ThrowInvalidHandle(mHandle);
        return mHandle->load_config(mHandle, altFile);
    }

    bool Container::start(bool useinit, const char *argv[]) {
        ThrowInvalidHandle(mHandle);
        return mHandle->start(mHandle, useinit, (char * const *)argv);
    }

    bool Container::stop() {
        ThrowInvalidHandle(mHandle);
        return mHandle->stop(mHandle);
    }

    bool Container::wantDaemonize(bool state) {
        ThrowInvalidHandle(mHandle);
        return mHandle->want_daemonize(mHandle, state);
    }

    bool Container::wantCloseAllFds(bool state) {
        ThrowInvalidHandle(mHandle);
        return mHandle->want_close_all_fds(mHandle, state);
    }

    bool Container::wait(const char *state, int timeout) {
        ThrowInvalidHandle(mHandle);
        return mHandle->wait(mHandle, state, timeout);
    }

    bool Container::setConfigItem(const char *key, const char *value) {
        ThrowInvalidHandle(mHandle);
        return mHandle->set_config_item(mHandle, key, value);
    }

    bool Container::destroy(bool withSnapshots) {
        ThrowInvalidHandle(mHandle);
        bool status;
        if (withSnapshots)
            status = mHandle->destroy_with_snapshots(mHandle);
        else
            status = mHandle->destroy(mHandle);
        lxc_container_put(mHandle);
        mHandle = nullptr;
        return status;
    }

    bool Container::saveConfig(const char *altFile) {
        ThrowInvalidHandle(mHandle);
        return mHandle->save_config(mHandle, altFile);
    }

    bool Container::rename(const char *newName) {
        ThrowInvalidHandle(mHandle);
        return mHandle->rename(mHandle, newName);
    }

    bool Container::reboot(bool sigint, int timeout) {
        ThrowInvalidHandle(mHandle);
        if (sigint)
            return mHandle->reboot2(mHandle, timeout);
        else
            return mHandle->reboot(mHandle);
    }

    bool Container::shutdown(int timeout) {
        ThrowInvalidHandle(mHandle);
        return  mHandle->shutdown(mHandle, timeout);
    }

    void Container::clearConfig() {
        ThrowInvalidHandle(mHandle);
        mHandle->clear_config(mHandle);
    }

    bool Container::clearConfigItem(const char *key) {
        ThrowInvalidHandle(mHandle);
        mHandle->clear_config_item(mHandle, key);
    }

    String Container::getConfigItem(const char *key) const {
        ThrowInvalidHandle(mHandle);
        int len = mHandle->get_config_item(mHandle, key, nullptr, 0);
        if (len < 0) {
            lerror(LXCLOG, "sizeof config item '%s' not found", key);
            return nullptr;
        }

        char *retv = (char *) malloc((size_t)(len+1));
        if (retv == NULL) {
            lerror(LXCLOG, "failed to allocate memory for config item size=%d", len);
            return nullptr;
        }
        len = mHandle->get_config_item(mHandle, key, retv, len+1);
        if (len < 0) {
            lerror(LXCLOG, "failed to retrieve key '%s': %s", key, mHandle->error_string);
            return nullptr;
        }
        return String(retv, (size_t) len, true);
    }

    String Container::getRunningConfigItem(const char *key) const {
        ThrowInvalidHandle(mHandle);
        return String{mHandle->get_running_config_item(mHandle, key)};
    }

    std::vector<String> Container::getKeys(const char *prefix) const {
        ThrowInvalidHandle(mHandle);
        std::vector<String> out{};
        int len = mHandle->get_keys(mHandle, prefix, nullptr, 0);
        if (len < 0) {
            lerror(LXCLOG, "size of keys with prefix '%s' not found", prefix);
            return out;
        }

        char *retv = (char *) malloc((size_t)(len+1));
        if (retv == NULL) {
            lerror(LXCLOG, "failed to allocate memory for keys prefix size=%d", len);
            return out;
        }
        len = mHandle->get_keys(mHandle, prefix, retv, len+1);
        if (len < 0) {
            lerror(LXCLOG, "failed to retrieve keys with prefix '%s': %s", prefix, mHandle->error_string);
            return out;
        }

        out.emplace_back(retv, len, true);
        auto parts = out.front().split("\n");
        for (auto& p: parts) {
            out.emplace_back(p);
        }
        return std::move(out);
    }

    std::vector<String> Container::getInterfaces() const {
        ThrowInvalidHandle(mHandle);
        auto intfs = mHandle->get_interfaces(mHandle);
        std::vector<String> out{};

        if (intfs != nullptr) {
            auto it = intfs;
            while (*it != nullptr) {
                out.emplace_back(*it);
                it++;
            }
            free(intfs);
        }
        return std::move(out);
    }

    std::vector<String> Container::getIps(const char *interface, const char *family, int scope) const {
        ThrowInvalidHandle(mHandle);
        auto ips = mHandle->get_ips(mHandle, interface, family, scope);
        std::vector<String> out{};

        if (ips != nullptr) {
            auto it = ips;
            while (*it != nullptr) {
                out.emplace_back(*it);
                it++;
            }
            free(ips);
        }

        return std::move(out);
    }

    String Container::getCgroupItem(const char *subsys) const {
        ThrowInvalidHandle(mHandle);
        int len = mHandle->get_cgroup_item(mHandle, subsys, nullptr, 0);
        if (len < 0) {
            lerror(LXCLOG, "sizeof cgroup item '%s' not found", subsys);
            return nullptr;
        }

        char *retv = (char *) malloc((size_t)(len+1));
        if (retv == nullptr) {
            lerror(LXCLOG, "failed to allocate memory for cgroup item size=%d", len);
            return nullptr;
        }
        len = mHandle->get_cgroup_item(mHandle, subsys, retv, len+1);
        if (len < 0) {
            lerror(LXCLOG, "failed to retrieve cgroup item '%s': %s", subsys, mHandle->error_string);
            return nullptr;
        }
        return String(retv, (size_t) len, true);
    }

    bool Container::setCgroupItem(const char *subsys, const char *value) {
        ThrowInvalidHandle(mHandle);
        return mHandle->set_config_item(mHandle, subsys, value);
    }

    String Container::getConfigPath() const {
        ThrowInvalidHandle(mHandle);
        return String{mHandle->get_config_path(mHandle)};
    }

    bool Container::setConfigPath(const char *configPath) {
        ThrowInvalidHandle(mHandle);
        return mHandle->set_config_path(mHandle, configPath);
    }

    Container Container::clone(const lxc::Clone &params, const char **hookargs) const {
        ThrowInvalidHandle(mHandle);
        Container::Handle  cloned =
                mHandle->clone(mHandle,
                        params.NewName.c_str(nullptr),
                        params.LxcPath.c_str(nullptr),
                        params.Flags,
                        params.BdevType.c_str(nullptr),
                        params.BdevData.c_str(nullptr),
                        params.NewSize,
                        (char **) hookargs);
        if (cloned == nullptr) {
            lerror(LXCLOG, "failed to clone container '%s': %s",
                    Ego.getName()(), mHandle->error_string);
        }
        return Container{cloned};
    }

    lxc::ConsoleFd Container::consoleGetFd(int ttynum) const {
        ThrowInvalidHandle(mHandle);
        lxc::ConsoleFd fd;
        fd.TtyNum = ttynum;
        fd.Fd = mHandle->console_getfd(mHandle, &fd.TtyNum, &fd.MasterFd);
        return std::move(fd);
    }

    int Container::console(int ttynum, int stdinfd, int stdoutfd, int stderrfd, int escape) {
        ThrowInvalidHandle(mHandle);
        return mHandle->console(mHandle, ttynum, stdinfd, stdoutfd, stderrfd, escape);
    }

    pid_t Container::attach(lxc::AttachOptions &options,void *payload, suil::lxc::AttachExec func) {
        ThrowInvalidHandle(mHandle);
        pid_t  pid{-1};
        lxc::NativeAttachOptions native LXC_ATTACH_OPTIONS_DEFAULT;
        int status = mHandle->attach(mHandle,
                func,
                payload,
                lxc::AttachOptionsOps(options).toNative(native),
                &pid);
        if (!status) {
            lerror(LXCLOG, "attach to container '%s' failed - %s", Ego.getName()(), mHandle->error_string);
            pid = -1;
        }
        return pid;
    }

    int Container::run(suil::lxc::AttachOptions &options, const char *program, const char **argv) {
        ThrowInvalidHandle(mHandle);
        lxc::NativeAttachOptions native LXC_ATTACH_OPTIONS_DEFAULT;
        return mHandle->attach_run_wait(mHandle,
                lxc::AttachOptionsOps(options).toNative(native),
                program,
                argv);
    }

    int Container::snapshot(const char *commentFile) {
        ThrowInvalidHandle(mHandle);
        return mHandle->snapshot(mHandle, commentFile);
    }

    std::vector<lxc::Snapshot> Container::listSnapshots() {
        ThrowInvalidHandle(mHandle);
        std::vector<lxc::Snapshot> snapshots;
        lxc::NativeSnapshot *natives;
        int num = mHandle->snapshot_list(mHandle, &natives);
        if (num) {
            for (int i = 0; i < num; i++) {
                lxc::Snapshot snap;
                lxc::SnapshotOps(snap, &natives[i]);
                natives[i].free(&natives[i]);
                snapshots.push_back(std::move(snap));
            }
            free(natives);
        }

        return std::move(snapshots);
    }

    bool Container::restoreSnapshot(const char *snapname, const char *newname) {
        ThrowInvalidHandle(mHandle);
        return mHandle->snapshot_restore(mHandle, snapname, newname);
    }

    bool Container::destroySnapshot(const char *snapname) {
        ThrowInvalidHandle(mHandle);
        return mHandle->snapshot_destroy(mHandle, snapname);
    }

    bool Container::mayControl() const {
        ThrowInvalidHandle(mHandle);
        return mHandle->may_control(mHandle);
    }

    bool Container::addDeviceNode(const char *src_path, const char *dest_path) {
        ThrowInvalidHandle(mHandle);
        return mHandle->add_device_node(mHandle, src_path, dest_path);
    }

    bool Container::removeDeviceNode(const char *src_path, const char *dest_path) {
        ThrowInvalidHandle(mHandle);
        return mHandle->remove_device_node(mHandle, src_path, dest_path);
    }

    bool Container::attachInterface(const char *dev, const char *dst_dev) {
        ThrowInvalidHandle(mHandle);
        return mHandle->attach_interface(mHandle, dev, dst_dev);
    }

    bool Container::detachInterface(const char *dev, const char *dst_dev) {
        ThrowInvalidHandle(mHandle);
        return mHandle->detach_interface(mHandle, dev, dst_dev);
    }

    bool Container::checkpoint(char *directory, bool stop, bool verbose) {
        ThrowInvalidHandle(mHandle);
        return mHandle->checkpoint(mHandle, directory, stop, verbose);
    }

    bool Container::restore(char *directory, bool verbose) {
        ThrowInvalidHandle(mHandle);
        return mHandle->restore(mHandle, directory, verbose);
    }

    bool Container::destroyAllSnapshots() {
        ThrowInvalidHandle(mHandle);
        return mHandle->snapshot_destroy_all(mHandle);
    }

    int Container::migrate(uint32_t cmd, lxc::MigrateOpts &opts, uint32_t size) {
        ThrowInvalidHandle(mHandle);
        lxc::NativeMigrateOpts native{};
        return mHandle->migrate(mHandle,
                cmd,
                lxc::MigrateOptsOps(opts).toNative(native),
                size);
    }

    String Container::readConsoleLog(size_t readMax, bool clear) {
        ThrowInvalidHandle(mHandle);
        lxc::NativeConsoleLog log{clear, true, &readMax, nullptr};
        if (!mHandle->console_log(mHandle, &log)) {
            return String{log.data, readMax, true};
        }
        else {
            lerror(LXCLOG, "reading console log on container '%s' failed - %s",
                    Ego.getName()(), mHandle->error_string);
            return String{};
        }
    }

    int Container::clearConsoleLog(size_t clearMax) {
        ThrowInvalidHandle(mHandle);
        lxc::NativeConsoleLog log{true, false, &clearMax, nullptr};
        return mHandle->console_log(mHandle, &log);
    }

    Map<Container> lxc::listAllContainers(const char *lxcpath)
    {
        char **names;
        struct lxc_container **cret;
        int count = list_all_containers(lxcpath, &names, &cret);
        if (count > 0) {
            Map<Container> containers;
            for (int i = 0; i < count; i++) {
                String name{names[i]};
                if (name.empty()) {
                    // ignore container with invalid name
                    continue;
                }
                containers.emplace(std::move(name), cret[i]);
            }
            return std::move(containers);
        }
        else {
            lerror(LXCLOG, "failed to find container: %d", count);
            return Map<Container>{};
        }
    }
}