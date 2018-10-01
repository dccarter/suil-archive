//
// Created by dc on 8/1/17.
//

#ifndef SUIL_FSERVER_HPP
#define SUIL_FSERVER_HPP

#include <suil/http/endpoint.hpp>

namespace suil {
    namespace http {

        define_log_tag(FILE_SERVER);

        struct FileServer : LOGGER(dtag(FILE_SERVER)) {
            struct config_t {
                size_t          compress_min{2048};
                bool            enable_send_file{false};
                int64_t         cache_expires{86400};
                size_t          mapped_min{2048};
                std::string     root{"./www/"};
                std::string     route{"/" SUIL_FILE_SERVER_ROUTE};
            };

            template <typename __Endpoint, typename... __Opts>
            FileServer(__Endpoint& ep, __Opts&&... opts)
            {
                // apply  file server configurations
                utils::apply_config(config, opts...);

                // initialize file server
                init();

                ep(config.route + "")
                ("GET"_method, "HEAD"_method)
                .attrs(opt(STATIC, true),
                       opt(AUTHORIZE, Auth{false}))
                ([&](const Request& req, Response& res) {
                    // handle static file Request
                    zcstring path(req.url);
                    const char *ext = strrchr(path.data(), '.');
                    if (!ext) {
                        // find path in the redirect lists
                        path = Ego.aliased(path);
                        if (!path) {
                            // path does not exist
                            throw error::not_found();
                        }
                        ext = strrchr(path.data(), '.');
                    }

                    // get path from extension
                    try {
                        zcstring p(path.data(), ext - path.data(), false);
                        zcstring e(ext, path.size() - p.size(), false);
                        if ((Method) req.method == Method::Get) {
                            get(req, res, p, e);
                        } else {
                            head(req, res, p, e);
                        }
                    } catch (...) {
                        const char *emsg = suil::SuilError::getmsg(std::current_exception());
                        res << "Error serving '" << path << "': " << emsg;
                        ierror("'%s': %s", path(), emsg);
                        res.end(Status::INTERNAL_ERROR);
                    }
                });
            }

            template <typename... __Opts>
            void mime(zcstring ext, const char *mime, __Opts... opts) {

                // configure mime type
                auto it = mime_types_.find(ext);
                if (it == mime_types_.end()) {
                    // create a new mime type
                    mime_type_t tmp(mime);
                    // dup the extension
                    zcstring cp = ext;
                    auto bit = mime_types_.emplace(
                            std::make_pair(std::move(cp), std::move(tmp)));
                    it = bit.first;
                }

                // apply the setting to the mime
                utils::apply_config(it->second, opts...);
                if (it->second.cache_expires < 0) {
                    // set cache expiry to global configuration
                    it->second.cache_expires = config.cache_expires;
                }
            }

            template<typename... __Opts>
            void mime(zcstring ext, __Opts... opts) {
                auto it = mime_types_.find(ext);
                if (it == mime_types_.end()) {
                    throw std::runtime_error("mime not registered");
                }

                // apply the settings
                utils::apply_config(it->second, opts...);

                if (it->second.cache_expires < 0) {
                    // if expires for mime was not modified, set to global expires
                    it->second.cache_expires = config.cache_expires;
                }
            }

            template<typename... __Opts>
            void mime(std::vector<zcstring> exts, __Opts... opts) {
                for(auto ext : exts) {
                    mime(ext, opts...);
                }
            }

            void alias(const zcstring from, const zcstring to);

            config_t config;

        private:
            void init();

            void get(const Request& req, Response& resp, zcstring& path, zcstring& ext);

            void head(const Request& req, Response& resp, zcstring& path, zcstring& ext);

            zcstring aliased(const zcstring &path);

            struct mime_type_t {
                mime_type_t(const char *mm)
                    : mime(utils::strdup(mm), strlen(mm), true)
                {}
                zcstring mime;
                bool     allow_compress{false};
                bool     allow_caching{true};
                bool     allow_range{true};
                int64_t  cache_expires{-1};
            };
            typedef zmap<mime_type_t> mime_types_t;

            struct cached_file_t {
                int     fd{-1};
                void    *data{nullptr};
                struct {
                    uint8_t use_fd: 1;
                    uint8_t is_mapped: 1;
                    uint8_t flags: 4;
                };
                zcstring path{};
                size_t   len{0};
                size_t   size{0};
                time_t   last_mod{0};
                time_t   last_access{0};

                cached_file_t(){};

                cached_file_t(cached_file_t&& cf)
                    : fd(cf.fd),
                      data(cf.data),
                      use_fd(cf.use_fd),
                      is_mapped(cf.is_mapped),
                      flags(cf.flags),
                      path(std::move(cf.path)),
                      len(cf.len),
                      size(cf.size),
                      last_mod(cf.last_mod),
                      last_access(cf.last_access)
                {
                    cf.fd = -1;
                    cf.data = nullptr;
                    cf.use_fd = cf.is_mapped = 0;
                    cf.flags = 0;
                    cf.len = cf.size = 0;
                    cf.last_mod = cf.last_access = 0;
                }

                cached_file_t&operator=(cached_file_t&& cf) {
                    fd = cf.fd;
                    data = cf.data;
                    use_fd = cf.use_fd;
                    is_mapped = cf.is_mapped;
                    flags = cf.flags;
                    path = std::move(cf.path);
                    len = cf.len;
                    size = cf.size;
                    last_mod = cf.last_mod;
                    last_access = cf.last_access;

                    cf.fd = -1;
                    cf.data = nullptr;
                    cf.use_fd = cf.is_mapped = 0;
                    cf.flags = 0;
                    cf.len = cf.size = 0;
                    cf.last_mod = cf.last_access = 0;

                    return *this;
                }

                //cached_file_t(cached_file_t&) = delete;
                //cached_file_t&operator=(cached_file_t&) = delete;

                void clear();

                ~cached_file_t() {
                    clear();
                }
            };
            using cached_files_t = zmap<cached_file_t>;

            typename cached_files_t::iterator load_file(const zcstring&, const mime_type_t&);

            bool file_exists(zcstring & path, const zcstring& rel) const;

            bool read_file(cached_file_t& cf, const struct stat& st);

            void cache_control(const Request&, Response&, cached_file_t&, mime_type_t&);

            void prepare_response(const Request&, Response&, cached_file_t&, mime_type_t&);

            void build_range_resp(
                    const Request&, Response&, strview&, cached_file_t&, mime_type_t&);

            mime_types_t    mime_types_;
            cached_files_t  cached_files_;
            zcstring        www_dir;
            zmap<zcstring>  redirects;
        };
    }
}
#endif //SUIL_FSERVER_HPP
