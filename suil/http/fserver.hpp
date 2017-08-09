//
// Created by dc on 8/1/17.
//

#ifndef SUIL_FSERVER_HPP
#define SUIL_FSERVER_HPP

#include "endpoint.hpp"

namespace suil {
    namespace http {

        define_log_tag(FILE_SERVER);

        struct file_server : LOGGER(dtag(FILE_SERVER)) {
            struct config_t {
                size_t          compress_min{2048};
                bool            enable_send_file{false};
                int64_t         cache_expires{86400};
                size_t          mapped_min{2048};
                std::string     root{"./www/"};
                std::string     route{"/www/"};
            };

            template <typename __Endpoint, typename... __Opts>
            file_server(__Endpoint& ep, __Opts&&... opts)
            {
                // apply  file server configurations
                utils::apply_config(config, opts...);

                // initialize file server
                init();

                ep(config.route + "<path>")
                ("GET"_method, "HEAD"_method)
                ([&](const request& req, response& res, std::string path) {
                    // handle static file request
                    const char *ext = strrchr(path.data(), '.');
                    if (!ext) {
                        // file not found
                        throw error::not_found();
                    }

                    // get path from extension
                    zcstring p(path.data(), ext-path.data(), false);
                    zcstring e(ext, path.size()-p.len, false);
                    if ((method_t)req.method == method_t::Get) {
                        get(req, res, p, e);
                    }
                    else {
                        head(req, res, p, e);
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
                    // copy the extension
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

            config_t config;

        private:
            void init();

            void get(const request& req, response& resp, zcstring& path, zcstring& ext);

            void head(const request& req, response& resp, zcstring& path, zcstring& ext);

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
            typedef zcstr_map_t<mime_type_t> mime_types_t;

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
                    cf.use_fd = is_mapped = 0;
                    cf.flags = 0;
                    cf.len = size = 0;
                    cf.last_mod = last_access = 0;
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
                    cf.use_fd = is_mapped = 0;
                    cf.flags = 0;
                    cf.len = size = 0;
                    cf.last_mod = last_access = 0;

                    return *this;
                }

                void clear();

                ~cached_file_t() {
                    clear();
                }
            };
            using cached_files_t = zcstr_map_t<cached_file_t>;

            typename cached_files_t::iterator load_file(const zcstring&, const mime_type_t&);

            bool file_exists(zcstring & path, const zcstring& rel) const;

            bool read_file(cached_file_t& cf, const struct stat& st);

            void cache_control(const request&, response&, cached_file_t&, mime_type_t&);

            void prepare_response(const request&, response&, cached_file_t&, mime_type_t&);

            void build_range_resp(
                    const request&, response&, strview_t&, cached_file_t&, mime_type_t&);

            mime_types_t    mime_types_;
            cached_files_t  cached_files_;
            zcstr<>         www_dir;
        };
    }
}
#endif //SUIL_FSERVER_HPP
