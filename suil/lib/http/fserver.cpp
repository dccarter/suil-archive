//
// Created by dc on 8/1/17.
//

#include <fcntl.h>
#include <sys/mman.h>

#include "fserver.hpp"

namespace suil {
    namespace http {

        void file_server::init() {
            // add text mime types
            mime(".html", "text/html",
                 opt(allow_caching, false));
            mime(".css", "text/css");
            mime(".csv", "text/csv");
            mime(".txt", "text/plain");
            mime(".sgml","text/sgml");
            mime(".tsv", "text/tab-separated-values");

            // add compressed mime types
            mime(".bz", "application/x-bzip",
                 opt(allow_compress, false));
            mime(".bz2", "application/x-bzip2",
                 opt(allow_compress, false));
            mime(".gz", "application/x-gzip",
                 opt(allow_compress, false));
            mime(".tgz", "application/x-tar",
                 opt(allow_compress, false));
            mime(".tar", "application/x-tar",
                 opt(allow_compress, false));
            mime(".zip", "application/zip, application/x-compressed-zip",
                 opt(allow_compress, false));
            mime(".7z", "application/zip, application/x-compressed-zip",
                 opt(allow_compress, false));


            // add image mime types
            mime(".jpg", "image/jpeg");
            mime(".png", "image/png");
            mime(".svg", "image/svg+xml");
            mime(".gif", "image/gif");
            mime(".bmp", "image/bmp");
            mime(".tiff","image/tiff");
            mime(".ico", "image/x-icon");

            // add video mime types
            mime(".avi",  "video/avi");
            mime(".mpeg", "video/mpeg");
            mime(".mpg",  "video/mpeg");
            mime(".mp4",  "video/mp4");
            mime(".qt",   "video/quicktime");

            // add audio mime types
            mime(".au",  "audio/basic");
            mime(".midi","audio/x-midi");
            mime(".mp3", "audio/mpeg");
            mime(".ogg", "audio/vorbis, application/ogg");
            mime("ra",   "audio/x-pn-realaudio, audio/vnd.rn-realaudio");
            mime("ram",  "audio/x-pn-realaudio, audio/vnd.rn-realaudio");
            mime(".wav", "audio/wav, audio/x-wav");

            // Other common mime types
            mime(".json",  "application/json");
            mime(".js",    "application/javascript");
            mime(".ttf",   "font/ttf");
            mime(".xhtml", "application/xhtml+xml");
            mime(".xml",   "application/xml");

            char base[PATH_MAX];
            realpath(config.root.data(), base);
            struct stat st{};
            if (stat(base, &st) != 0 || !S_ISDIR(st.st_mode)) {
                throw std::runtime_error(
                        "base dir: '" + std::string(base) + "' is not a valid directory");
            }

            size_t sz = strlen(base);
            if (base[sz-1] != '/') {
                base[sz] = '/';
                base[++sz] = '\0';
            }

            // this will dup over the base
            www_dir = zcstring(base).dup();
        }

        void file_server::get(const request &req, response &resp, zcstring &path, zcstring &ext) {
            auto mime = mime_types_.find(ext);
            if (mime == mime_types_.end()) {
                trace("extension type (%s) not supported", ext.str);
                throw error::not_found();
            }

            mime_type_t& mm = mime->second;
            auto sf = load_file(path, mm);
            if (sf == cached_files_.end()) {
                trace("requested static resource (%s) does not exist", path.str);
                // static file not found;
                throw error::not_found();
            }

            cached_file_t& cf = sf->second;
            if (mm.allow_caching) {
                // if file supports cache headers employ cache headers
                strview_t cc = req.header("If-Modified-Since");
                if (!cc.empty()) {
                    time_t if_mod = datetime(cc.data());
                    if (if_mod >= cf.last_mod) {
                        // file was not modified
                        resp.end(status_t::NOT_MODIFIED);
                        return;
                    }
                }

                cache_control(req, resp, cf, mm);
            }

            // set the content type
            resp.header("Content-Type", mm.mime);

            // prepare the response
            prepare_response(req,resp, cf, mm);
        }

        void file_server::head(const request &req, response &resp, zcstring &path, zcstring &ext) {
            // FIXME: part of code is the similar to GET code, move to common stub
            auto mime = mime_types_.find(ext);
            if (mime == mime_types_.end()) {
                trace("extension type (%s) not supported", ext.str);
                throw error::not_found();
            }
            mime_type_t& mm = mime->second;
            auto sf = load_file(path, mm);
            if (sf == cached_files_.end()) {
                trace("requested static resource (%s) does not exist", path.str);
                // static file not found;
                throw error::not_found();
            }
        }

        void file_server::prepare_response(
                const request &req, response &resp, cached_file_t &cf, mime_type_t &mm)
        {
            if (mm.allow_range) {
                // let clients know that the server accepts ranges for current mime type
                resp.header("Accept-Ranges", "bytes");
            }
            else {
                // let clients know that the server doesn't accepts ranges for current mime type
                resp.header("Accept-Ranges", "none");
            }

            // check for range base request support
            strview_t range = req.header("Range");
            if (!range.empty() && mm.allow_range) {
                // prepare range based request
                build_range_resp(req, resp, range, cf, mm);
                if (resp.completed) {
                    return;
                }
            }
            else {
                // send the entire content
                if (config.enable_send_file) {
                    resp.chunk(std::move(response::chunk_t(cf.fd, cf.len)));
                }
                else {
                    resp.chunk(std::move(response::chunk_t(cf.data, cf.len)));
                }
                resp.end(status_t::OK);
            }
        }

        void file_server::build_range_resp(
                const request &req, response &resp, strview_t &rng, cached_file_t &cf, mime_type_t &mm)
        {
            const char *pend = rng.data() + rng.size();
            const char *it = strchr(rng.data(), '=');
            size_t  from = 0, to = 0;
            const char *pfrom = nullptr;
            std::vector<std::pair<size_t, size_t>> ranges;

            while (it && it != pend) {
                // get rid of white space
                while(*it++ == ' ');
                pfrom = it;
                from = strtoul(pfrom, nullptr, 10);

                it = strchr(it, '-');
                if (it != nullptr && (++it != pend)) {
                    // get range end
                    to = strtoul(it, nullptr, 10)+1;
                }
                else {
                    to = cf.len;
                }
                trace("partial content: %lu-%lu/%lu", from, to,cf.len);
                // build partial content chunk
                if (from > to || from >= cf.len || to > cf.len) {
                    trace("requested range is out of bounds");
                    resp.end(status_t::REQUEST_RANGE_INVALID);
                    return;
                }
                // add the range to the end of the ranges list
                ranges.emplace_back(from, to);
                it = strchr(it, ',');
            }

            // depending on the number of requested ranges, build response
            if (ranges.size() == 1) {
                auto& range = ranges[0];
                if (config.enable_send_file) {
                    resp.chunk(response::chunk_t(cf.fd, range.first, range.second));
                }
                else {
                    resp.chunk(response::chunk_t(cf.data, range.first, range.second));
                }
                // add the range header
                buffer_t b(0);
                b.appendf("bytes %lu-%lu/%lu", range.first, range.second-1, cf.len);
                resp.header("Content-Range", b);

                resp.end(status_t::PARTIAL_CONTENT);
            }
            else if (ranges.size() != 0) {
                // multiple ranges specified, not supported for now
                resp.end(status_t::NOT_ACCEPTABLE);
            }
        }

        void file_server::cache_control(
                const request &req, response &resp, cached_file_t &cf, mime_type_t &mm)
        {
            // get http data format
            zcstring dt(datetime(cf.last_mod)(datetime::HTTP_FMT));
            resp.header("Last-Modified", dt);
            // add cache control header
            if (mm.cache_expires > 0) {
                buffer_t b(31);
                b.appendnf(30, "public, max-age=%ld", mm.cache_expires);
                resp.header("Cache-Control", b);
            }
        }

        file_server::cached_files_t::iterator file_server::load_file(const zcstring &rel, const mime_type_t &mm)
        {
            auto it = cached_files_.find(rel);

            if (it == cached_files_.end()) {
                zcstring path;
                if (file_exists(path, rel)) {
                    cached_file_t cf;
                    cf.clear();

                    struct stat st;
                    stat(path.str, &st);

                    cf.fd = open(path.str, O_RDONLY);
                    if (cf.fd < 0) {
                        warn("opening static resource(%s) failed: %s",
                             path.str, errno_s);
                        return cached_files_.end();
                    }
                    else if (config.enable_send_file) {
                        trace("enable send fd(%d) for %s", cf.fd, path.str);
                        cf.use_fd = 1;
                    }
                    else {
                        if (!read_file(cf, st)) {
                            trace("loading file (%s) failed", path.str);
                            close(cf.fd);
                            return cached_files_.end();
                        }
                    }

                    cf.last_mod    = (time_t) st.st_mtim.tv_sec;
                    cf.last_access = (time_t) st.st_atim.tv_sec;
                    cf.len         = (size_t) st.st_size;
                    cf.path        = std::move(path);

                    // file successfully loaded, add file to cache
                    it = cached_files_.emplace(
                            std::move(rel), std::move(cf)).first;
                }
            }
            else {
                cached_file_t& cf = it->second;
                struct stat st;
                stat(cf.path.str, &st);

                // reload file if it was recently modified
                if(cf.last_mod != (time_t)st.st_mtim.tv_sec) {
                    cf.clear();

                    cf.fd = open(cf.path.str, O_RDONLY);
                    if (cf.fd < 0) {
                        warn("opening static resource(%s) failed: %s",
                             cf.path.str, errno_s);
                        cached_files_.erase(it);
                        return cached_files_.end();
                    }
                    else if (config.enable_send_file) {
                        trace("enable send fd(%d) for %s", cf.fd, cf.path.str);
                        cf.use_fd = 1;
                    }
                    else {
                        if (!read_file(cf, st)) {
                            trace("loading file (%s) failed", cf.path.str);
                            close(cf.fd);
                            cached_files_.erase(it);
                            return cached_files_.end();
                        }
                    }

                    cf.last_mod    = (time_t) st.st_mtim.tv_sec;
                    cf.last_access = (time_t) st.st_atim.tv_sec;
                    cf.len         = (size_t) st.st_size;
                }
            }

            return it;
        }

        bool file_server::read_file(cached_file_t &cf, const struct stat &st)
        {
            if (cf.fd < 0) {
                trace("reloading a closed file not allowed");
                return false;
            }

            if ((size_t)st.st_size >= config.mapped_min) {
                size_t total = (size_t) st.st_size;
                int page_sz = getpagesize();
                total += page_sz-(total % page_sz);
                cf.data = mmap(NULL, total, PROT_READ, MAP_SHARED , cf.fd, 0);
                if (cf.data == MAP_FAILED) {
                    warn("mapping static resource (%d) of size %d failed: %s",
                         cf.fd, total, errno_s);
                    return false;
                }
                cf.is_mapped = 1;
                cf.size = total;
            }
            else {
                // read file
                uint32_t total = (uint32_t) st.st_size + 8;
                buffer_t b(total);
                size_t nread = 0, toread = (size_t) st.st_size;
                ssize_t cread = 0;
                char *ptr = b;
                do {
                    cread = read(cf.fd, (ptr + nread), toread - nread);
                    if (cread < 0) {
                        warn("reading file %d failed: %s", cf.fd, errno_s);
                        return false;
                    }
                    nread += cread;
                } while(nread < toread);
                // adjust size
                b.bseek(toread);
                // release buffer to file
                cf.data = b.release();
                cf.is_mapped = 0;
                cf.size = total;
            }

            return true;
        }

        bool file_server::file_exists(zcstring &path, const zcstring &rel) const
        {
            // we want to ensure that the file is within the base directory
            buffer_t b(0);
            // append base, followed by file
            b << www_dir << rel.str;
            char absolute[PATH_MAX];
            realpath((char *)b, absolute);
            zcstring tmp(absolute, www_dir.len, false);

            if (tmp != www_dir) {
                // path violation
                debug("requested path has back references: %s", rel.str);
                return false;
            }

            struct stat st;
            if (stat(absolute, &st) != 0 || !S_ISREG(st.st_mode)) {
                // file does not exist
                debug("request path does not exist: %s", absolute);
                return false;
            }

            // dup over the generated path buffer
            path = zcstring(absolute, strlen(absolute) + 1, false).dup();
            return true;
        }

        void file_server::cached_file_t::clear() {
            if (data) {
                if (is_mapped) {
                    munmap(data, size);
                }
                else {
                    memory::free(data);
                }
                data = nullptr;
            }
            close(fd);
            is_mapped = use_fd = 0;
            size = len = 0;
            fd = -1;
            last_mod = last_access = 0;
        }
    }
}