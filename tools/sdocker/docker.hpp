//
// Created by dc on 28/09/18.
//

#ifndef SUIL_DOCKER_HPP
#define SUIL_DOCKER_HPP

#include <suil/http/clientapi.h>
#include <sdocker/types.json.h>

namespace suil::docker {

    define_log_tag(DOCKER);
    struct Docker;

    struct Container : LOGGER(DOCKER) {
    sptr(Container)

        template <typename... Opts>
        json::Object ps(Opts&&... options) {
            auto opts = iod::D(std::forward<Opts>(options)...);
            bool all    = opts.get(var(all), false);
            int  limit  = opts.get(var(limit), 0);
            bool size   = opts.get(var(size), false);
            std::string filter = opts.get(var(filters), "");

            return Ego.ps(all, limit, size, filter);
        }

        template <typename... Opts>
        json::Object ps(ListFilter  filter, Opts&&... options) {
            auto opts = iod::D(std::forward<Opts>(options)...);
            bool all   = opts.get(var(all), false);
            int  limit = opts.get(var(limit), 0);
            bool size  = opts.get(var(size), false);
            std::string filterStr = json::encode(filter);

            return Ego.ps(all, limit, size, filterStr);
        }

        json::Object create(const CreateReq& req);

        json::Object inspect(const String id, bool size = false);

        json::Object top(const String id, const String args = nullptr);

        template <typename... Args>
        String logs(const String id, Args... args) {
            LogsReq req;
            iod::zero(req);
            if constexpr (sizeof...(args)) {
                auto opts = iod::D(std::forward<Args>(args)...);
                req.follow = opts.get(var(follow), false);
                req.stdOut = opts.get(var(stdOut), false);
                req.stdErr = opts.get(var(stdErr), false);
                req.since = opts.get(var(since), 0);
                req.until = opts.get(var(until), 0);
                req.timestamps = opts.get(var(timestamps), 0);
                req.tail = opts.get(var(tail), nullptr);
            }
            return Ego.logs(id, req);
        }

        String logs(const String id, const LogsReq& req);

        json::Object changes(const String id);

        void Export(const String id);

        json::Object stats(const String id, bool stream = false);

        void resize(const String id, uint32_t x, uint32_t y);

        void start(const String id, const String detachKeys = nullptr);

        void stop(const String id, uint64_t t = 0);

        void restart(const String id, uint64_t t = 0);

        void kill(const String id, const String sig = nullptr);

        json::Object update(const String id, const UpdateReq& request);

        void rename(const String id, const String name);

        void pause(const String id);

        void unpause(const String id);

        json::Object wait(const String id, const String condition = nullptr);

        void     remove(const String id, const RemoveQuery& query);

        template <typename... Args>
        void remove(const String id, Args... args) {
            RemoveQuery req;
            iod::zero(req);
            if constexpr (sizeof...(args)) {
                auto opts = iod::D(std::forward<Args>(args)...);
                req.v = opts.get(var(v), false);
                req.force = opts.get(var(force), false);
                req.link  = opts.get(var(link), false);
            }
            Ego.remove(id, req);
        }

        String archiveInfo(const String id, const String path);

        void getArchive(const String id, const String path, const String localDir = nullptr) {}

        void putArchive(const String id, const String dstDir, const String localArchive, bool force = false) {}

        json::Object prune(const PruneQuery& query);

        template <typename... Args>
        json::Object prune(Args... args) {
            PruneQuery req;
            iod::zero(req);
            if constexpr (sizeof...(args)) {
                auto opts = iod::D(std::forward<Args>(args)...);
                uint64_t u = opts.get(var(until), 0);
                if (u) {
                    req.until.emplace_back(utils::tostr(u));
                }
                req.label = opts.get(var(label), std::vector<String>{});
            }
            return Ego.prune(req);
        }

        json::Object exec(const String id, const ExecCreateReq& request);

    private:
        friend struct Docker;
        Container(Docker& ref)
                : ref(ref)
        {}

        json::Object ps(bool all, int limit, bool size, std::string& filter);
        Docker& ref;
    };

    using XRegistryConfig = Map<RegistryCredentials>;
    using Filter = std::vector<String>;
    using Filters = Map<Filter>;

    template <typename T>
    inline void add_Filter(Filters& filters, const char *name, T& value) {
        String tmp(name);
        if (filters.find(tmp) == filters.end()) {
            // add vector for filter
            filters.emplace(tmp.dup(), Filter{});
        }
        filters[tmp].emplace_back(utils::tostr(value));
    }

    struct Images : LOGGER(DOCKER) {
    sptr(Images)

        inline json::Object ls(bool all = false, bool digest = false) {
            ImagesFilter filters;
            return Ego.ls(filters, all, digest);
        }

        template <typename... Args>
        json::Object ls(ImagesFilter filters, Args... args) {
            bool all = false;
            bool digests = false;
            if constexpr (sizeof...(args)) {
                auto opts = iod::D(std::forward<Args>(args)...);
                all = opts.get(var(all), false);
                digests = opts.get(var(digests), false);
            }
            return Ego.ls(filters, all, digests);
        }

        template <typename... Args>
        inline void build(const String archive, const String contentType, Args... args) {
            XRegistryConfig registry;
            Ego.build(archive.peek(), contentType.peek(), registry, std::forward<Args>(args)...);
        }

        template <typename... Args>
        void build(const String archive, const String contentType, const XRegistryConfig& registries, Args... args) {
            BuildParams params;
            iod::zero(params);
            if constexpr (sizeof...(args)) {
                // set values accordingly
                utils::apply_config(params, std::forward<Args...>(args)...);
            }
            Ego.build(archive.peek(), contentType.peek(), registries, params);
        }

        void build(const String archive, const String contentType, const XRegistryConfig& registries, const BuildParams& params);

        json::Object buildPrune();

        template <typename... Args>
        void create(Args... args) {
            ImagesCreateParams params;
            iod::zero(params);
            if constexpr (sizeof...(args)) {
                // set values accordingly
                utils::apply_config(params, std::forward<Args...>(args)...);
            }
            Ego.create(params);
        }

        void create(ImagesCreateParams& params);

        json::Object inspect(const String name);

        json::Object history(const String name);

        void push(const String name, const String tag = nullptr);

        void tag(const String name, const ImagesTagParams& params);

        template <typename... Args>
        void tag(const String name, Args... args) {
            ImagesTagParams params;
            if constexpr (sizeof...(args)) {
                // set values accordingly
                utils::apply_config(params, std::forward<Args...>(args)...);
            }
            Ego.tag(name, params);
        }

        json::Object remove(const String name, const ImagesRemoveParams& params);

        template <typename... Args>
        json::Object tag(const String name, Args... args) {
            ImagesRemoveParams params;
            if constexpr (sizeof...(args)) {
                // set values accordingly
                utils::apply_config(params, std::forward<Args...>(args)...);
            }
            return Ego.remove(name, params);
        }

        json::Object search(ImagesSearchParams& params);

        template <typename... Args>
        json::Object search(Args... args) {
            auto opts = iod::D(args...);
            ImagesSearchParams params;
            params.term  = opts.get(var(term), nullptr);
            params.limit = opts.get(var(limit), nullptr);
            if (opts.has(var(filters))) {
                // get filters
                Filters filters = opts.get(var(filters), Filters{});
                params.filters = json::encode(filters);
            }
            return Ego.search(params);
        }

        json::Object prune(Filters filters);

        json::Object commit(const ContainerConfig& container, const ImagesCommitParams& params);

        template <typename... Args>
        json::Object commit(const ContainerConfig& container, Args... args) {
            ImagesCommitParams  params;
            if constexpr (sizeof...(args)) {
                // set values accordingly
                utils::apply_config(params, std::forward<Args...>(args)...);
            }
            return Ego.commit(container, params);
        }

        void get(const String name, const char *output);

        void gets(const std::vector<String> names, const char *output);

        void load(const char *input, bool quiet = false);

    private:

        friend struct Docker;

        Images(Docker& ref)
            : ref(ref)
        {}

        json::Object ls(const ImagesFilter& filters, bool all = false, bool digest = false);

        Docker& ref;
    };

    struct Networks : LOGGER(DOCKER) {
    sptr(Networks)

        json::Object ls(const Filters& filters);

        json::Object inspect(const String id, const NetworksInspectParams& params);

        template <typename... Args>
        json::Object inspect(const String id, Args... args) {
            NetworksInspectParams params;
            if constexpr (sizeof...(args)) {
                utils::apply_config(params, std::forward<Args>(args)...);
            }
            return Ego.inspect(id, params);
        }

        void remove(const String id);

        json::Object create(const NetworksCreateReq& request);

        void connect(const String id, const NetworksConnectReq& request);

        void disconnect(const String id, const NetworksDisconnectReq& request);

        json::Object prune(Filters filters);

    private:
        friend struct Docker;
        Networks(Docker& ref)
            : ref(ref)
        {}

        Docker& ref;
    };

    struct Volumes : LOGGER(DOCKER) {
    sptr(Volumes)

        json::Object ls(const Filters& filters);

        json::Object inspect(const String id);

        void remove(const String id, bool force = false);

        json::Object create(const VolumesCreateReq& request);


        json::Object prune(Filters filters);

    private:
        friend struct Docker;
        Volumes(Docker& ref)
                : ref(ref)
        {}

        Docker& ref;
    };

    struct Exec : LOGGER(DOCKER) {
    sptr(Exec)

        void start(const String id, const ExecStartReq& request);
        void resize(const String id, uint32_t h, uint32_t w);
        json::Object inspect(const String id);

    private:
        friend struct Docker;
        Exec(Docker& ref)
                : ref(ref)
        {}

        Docker& ref;
    };

    struct Docker : LOGGER(DOCKER) {
    public:
        Docker(String host, int port);

        template <typename... Options>
        bool connect(Options... options) {
            auto opts = iod::D(options...);
            if (opts.has(var(loginAuth))) {
                // login using login credentials
                LoginReq params;
                params = opts.get(var(loginAuth), params);
                return connect(params);
            }
            else if (opts.has(var(tokenAuth))) {
                // login using token authentication
                AuthToken token;
                token = opts.get(var(loginAuth), token);
                return connect(token);
            }
            else {
                // connect without authentication
                return connect(false);
            }
        }

        docker::Container Container;
        docker::Images    Images;
        docker::Networks  Networks;
        docker::Volumes   Volumes;
        docker::Exec      Exec;

    protected:

        sptr(Docker)

        VersionResp version();
        bool connect(LoginReq& params);
        bool connect(AuthToken& token);
        bool connect(bool loaded);
        static void reportFailure(http::client::Response& resp);

        template <typename R, typename T>
        static inline void arg(R& r, const char *name, const T& t) {
            if constexpr (std::is_same<bool, T>::value)
                if (t) r.args(name, "true");
            else
                if (t) r.args(name, t);
        }
        template <typename R, typename... O>
        static inline void arg(R& r, const char *name, const iod::sio<O...>& t) {
            r.args(name, json::encode(t));
        }

        template <typename R, typename O>
        static void pack(R& r, O& o) {
            iod::foreach(o) | [&](auto m) {
                Docker::arg(r, m.symbol().name(), m.value());
            };
        }

        friend struct Images;
        friend struct Container;
        String              apiBase{nullptr};
        String              host{"localhost"};
        int                   port{4243};
        http::client::Session httpSession;
    };
}

#endif //SUIL_DOCKER_HPP
