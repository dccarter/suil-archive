//
// Created by dc on 28/09/18.
//

#ifndef SUIL_DOCKER_HPP
#define SUIL_DOCKER_HPP

#include <suil/http/clientapi.hpp>
#include <sdocker/types.json.h>

namespace suil::docker {

    define_log_tag(DOCKER);
    struct Docker;

    struct Container : LOGGER(dtag(DOCKER)) {
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

        json::Object inspect(const zcstring id, bool size = false);

        json::Object top(const zcstring id, const zcstring args = nullptr);

        template <typename... Args>
        zcstring logs(const zcstring id, Args... args) {
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

        zcstring logs(const zcstring id, const LogsReq& req);

        json::Object changes(const zcstring id);

        void Export(const zcstring id);

        json::Object stats(const zcstring id, bool stream = false);

        void resize(const zcstring id, uint32_t x, uint32_t y);

        void start(const zcstring id, const zcstring detachKeys = nullptr);

        void stop(const zcstring id, uint64_t t = 0);

        void restart(const zcstring id, uint64_t t = 0);

        void kill(const zcstring id, const zcstring sig = nullptr);

        json::Object update(const zcstring id, const UpdateReq& request);

        void rename(const zcstring id, const zcstring name);

        void pause(const zcstring id);

        void unpause(const zcstring id);

        json::Object wait(const zcstring id, const zcstring condition = nullptr);

        void     remove(const zcstring id, const RemoveQuery& query);

        template <typename... Args>
        void remove(const zcstring id, Args... args) {
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

        zcstring archiveInfo(const zcstring id, const zcstring path);

        void getArchive(const zcstring id, const zcstring path, const zcstring localDir = nullptr) {}

        void putArchive(const zcstring id, const zcstring dstDir, const zcstring localArchive, bool force = false) {}

        json::Object prune(const PruneQuery& query);

        template <typename... Args>
        json::Object prune(Args... args) {
            PruneQuery req;
            iod::zero(req);
            if constexpr (sizeof...(args)) {
                auto opts = iod::D(std::forward<Args>(args)...);
                uint64_t u = opts.get(var(until), 0);
                if (u) {
                    req.until.emplace_back(utils::tozcstr(u));
                }
                req.label = opts.get(var(label), std::vector<zcstring>{});
            }
            return Ego.prune(req);
        }

        json::Object exec(const zcstring id, const ExecCreateReq& request);

    private:
        friend struct Docker;
        Container(Docker& ref)
                : ref(ref)
        {}

        json::Object ps(bool all, int limit, bool size, std::string& filter);
        Docker& ref;
    };

    using XRegistryConfig = zstrmap<RegistryCredentials>;
    using Filter = std::vector<zcstring>;
    using Filters = zstrmap<Filter>;

    template <typename T>
    inline void add_Filter(Filters& filters, const char *name, T& value) {
        zcstring tmp(name);
        if (filters.find(tmp) == filters.end()) {
            // add vector for filter
            filters.emplace(tmp.dup(), Filter{});
        }
        filters[tmp].emplace_back(utils::tozcstr(value));
    }

    struct Images : LOGGER(dtag(DOCKER)) {
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
        inline void build(const zcstring archive, const zcstring contentType, Args... args) {
            XRegistryConfig registry;
            Ego.build(archive.peek(), contentType.peek(), registry, std::forward<Args>(args)...);
        }

        template <typename... Args>
        void build(const zcstring archive, const zcstring contentType, const XRegistryConfig& registries, Args... args) {
            BuildParams params;
            iod::zero(params);
            if constexpr (sizeof...(args)) {
                // set values accordingly
                utils::apply_config(params, std::forward<Args...>(args)...);
            }
            Ego.build(archive.peek(), contentType.peek(), registries, params);
        }

        void build(const zcstring archive, const zcstring contentType, const XRegistryConfig& registries, const BuildParams& params);

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

        json::Object inspect(const zcstring name);

        json::Object history(const zcstring name);

        void push(const zcstring name, const zcstring tag = nullptr);

        void tag(const zcstring name, const ImagesTagParams& params);

        template <typename... Args>
        void tag(const zcstring name, Args... args) {
            ImagesTagParams params;
            if constexpr (sizeof...(args)) {
                // set values accordingly
                utils::apply_config(params, std::forward<Args...>(args)...);
            }
            Ego.tag(name, params);
        }

        json::Object remove(const zcstring name, const ImagesRemoveParams& params);

        template <typename... Args>
        json::Object tag(const zcstring name, Args... args) {
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

        void get(const zcstring name, const char *output);

        void gets(const std::vector<zcstring> names, const char *output);

        void load(const char *input, bool quiet = false);

    private:

        friend struct Docker;

        Images(Docker& ref)
            : ref(ref)
        {}

        json::Object ls(const ImagesFilter& filters, bool all = false, bool digest = false);

        Docker& ref;
    };

    struct Networks : LOGGER(dtag(DOCKER)) {
    sptr(Networks)

        json::Object ls(const Filters& filters);

        json::Object inspect(const zcstring id, const NetworksInspectParams& params);

        template <typename... Args>
        json::Object inspect(const zcstring id, Args... args) {
            NetworksInspectParams params;
            if constexpr (sizeof...(args)) {
                utils::apply_config(params, std::forward<Args>(args)...);
            }
            return Ego.inspect(id, params);
        }

        void remove(const zcstring id);

        json::Object create(const NetworksCreateReq& request);

        void connect(const zcstring id, const NetworksConnectReq& request);

        void disconnect(const zcstring id, const NetworksDisconnectReq& request);

        json::Object prune(Filters filters);

    private:
        friend struct Docker;
        Networks(Docker& ref)
            : ref(ref)
        {}

        Docker& ref;
    };

    struct Volumes : LOGGER(dtag(DOCKER)) {
    sptr(Volumes)

        json::Object ls(const Filters& filters);

        json::Object inspect(const zcstring id);

        void remove(const zcstring id, bool force = false);

        json::Object create(const VolumesCreateReq& request);


        json::Object prune(Filters filters);

    private:
        friend struct Docker;
        Volumes(Docker& ref)
                : ref(ref)
        {}

        Docker& ref;
    };

    struct Exec : LOGGER(dtag(DOCKER)) {
    sptr(Exec)

        void start(const zcstring id, const ExecStartReq& request);
        void resize(const zcstring id, uint32_t h, uint32_t w);
        json::Object inspect(const zcstring id);

    private:
        friend struct Docker;
        Exec(Docker& ref)
                : ref(ref)
        {}

        Docker& ref;
    };

    struct Docker : LOGGER(dtag(DOCKER)) {
    public:
        Docker(zcstring host, int port);

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
        zcstring              apiBase{nullptr};
        zcstring              host{"localhost"};
        int                   port{4243};
        http::client::Session httpSession;
    };
}

#endif //SUIL_DOCKER_HPP
