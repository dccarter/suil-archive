//
// Created by dc on 01/06/17.
//

#include <dirent.h>
#include <openssl/sha.h>
#include <openssl/rand.h>
#include <openssl/hmac.h>
#include <openssl/md5.h>
#include <syslog.h>

#include <suil/sys.hpp>
#include <suil/config.hpp>

namespace suil {

    namespace version {
        const uint16_t  MAJOR  = SUIL_MAJOR_VERSION;
        const uint16_t  MINOR  = SUIL_MINOR_VERSION;
        const uint16_t  PATCH  = SUIL_PATCH_VERSION;
        const uint16_t  BUILD  = SUIL_BUILD_NUMBER;
        const char*     TAG    = SUIL_BUILD_TAG;
        const char*     STRING = SUIL_VERSION_STRING;
        const char*     SWNAME = SUIL_SOFTWARE_NAME;
    };

    namespace __internal {
        bool init() {
            static bool initialized{false};
            if (initialized) return false;
            //signal(SIGPIPE, SIG_IGN);

            console::println("");
            console::println("Powered by suil C++1y web framework");
            console::printred("v" SUIL_VERSION_STRING "\n");
            console::printblue("http://suil.suilteam.com\n");
            console::println("---------------------------------------\n");
            memory::init();
            initialized = true;

            return initialized;
        }

        void chwdir(const std::string& to) {
            if (!utils::fs::isdir(to.c_str())) {
                throw suil_error::create("working directory: '", to, "' does not exist");
            }

            if (::chdir(to.c_str())) {
                throw suil_error::create("setting working dir to('", to, "') failed: ", errno_s);
            }
        }

        void daemonize(const std::string& wdir) {
            pid_t pid, sid;
            pid = fork();
            if (pid < 0) {
                console::printred("error: daemonizing failed: \n", errno_s);
                exit(EXIT_FAILURE);
            }

            if (pid > 0) {
                // close parent
                exit(EXIT_SUCCESS);
            }

            /*Change file mode mask */
            umask(0);

            openlog("suil", LOG_PID|LOG_CONS, LOG_USER);
            // temporarily redirect logs to syslogs
            log::setup(var(logsink) =  [](const char *d, size_t  s, log::level l) {
                syslog(LOG_INFO, "%s", d);
            });

            sid = setsid();
            if (sid < 0) {
                serror("Creating session ID for daemon failed: %s", errno_s);
                closelog();
                exit(EXIT_FAILURE);
            }
            sinfo("Created session ID %d for suil daemon", sid);

            std::string tmp(wdir);
            if (wdir.empty())
                tmp = "/";
            try {
                chwdir(tmp);
            }
            catch (...) {
                serror("changing work director to '%s' failed: %s", tmp.c_str(), errno_s);
                closelog();
                exit(EXIT_FAILURE);
            }

            sinfo("suil application demonized %d", sid);
            close(STDIN_FILENO);
            close(STDOUT_FILENO);
            close(STDERR_FILENO);

            /* set pid file */
            pid = getpid();
            if (utils::fs::exists("suil.pid")) {
                utils::fs::remove("suil.pid");
            }
            utils::fs::append("suil.pid", pid);
        }
    }


    datetime::datetime(time_t t)
        : t_(t)
    {
        gmtime_r(&t_, &tm_);
    }

    datetime::datetime()
        : datetime(time(nullptr))
    {}

    datetime::datetime(const char *fmt, const char *str)
    {
        const char *tmp = HTTP_FMT;
        if (fmt) {
            tmp = fmt;
        }
        strptime(str, tmp, &tm_);
    }

    datetime::datetime(const char *http_time)
        : datetime(HTTP_FMT, http_time)
    {}

    buffer_t::buffer_t(size_t init_size)
        : data_{nullptr},
          size_((uint32_t) init_size),
          offset_(0)
    {
        if (size_)
            grow(init_size);
    }

    buffer_t::buffer_t(buffer_t && other)
        : data_(other.data_),
          size_(other.size_),
          offset_(other.offset_)
    {
        other.size_ = 0;
        other.offset_ = 0;
        other.data_ = nullptr;
    }

    buffer_t& buffer_t::operator=(buffer_t &&other) {
        size_ = other.size_;
        data_ = other.data_;
        offset_ = other.offset_;
        other.data_ = nullptr;
        other.offset_ = other.size_ = 0;

        return *this;
    }

    buffer_t::~buffer_t() {
        if (data_ != nullptr) {
            memory::free(data_);
            data_ = nullptr;
            size_ = 0;
            offset_ = 0;
        }
    }

    void buffer_t::appendf(const char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        appendv(fmt, args);
        va_end(args);
    }

    void buffer_t::appendv(const char *fmt, va_list args) {
        char	sb[2048];
        int ret;
        ret = vsnprintf(sb, sizeof(sb), fmt, args);
        if (ret == -1) {
            throw std::runtime_error(
                    "buffer_t::appendv(): error: " + std::string(errno_s));
        }
        append(sb, (uint32_t) ret);
    }

    void buffer_t::appendnf(uint32_t hint, const char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        appendnv(hint, fmt, args);
        va_end(args);
    }

    void buffer_t::appendnv(uint32_t hint, const char *fmt, va_list args) {
        if ((size_-offset_) < hint)
            grow(hint);

        int ret;
        assert(data_ && hint);
        ret = vsnprintf((char *)(data_+offset_), (size_-offset_), fmt, args);
        if (ret == -1 || (ret + offset_) > size_) {
            throw std::runtime_error(
                    "buffer_t::appendnv(): error: " + std::string(errno_s));
        }
        offset_ += ret;
    }

    void buffer_t::append(const void *data, size_t len) {
        if ((size_-offset_) < len) {
            grow(len);
        }
        if (len)
            assert(data_);
        memcpy((data_+offset_), data, len);
        offset_ += len;
    }

    void buffer_t::append(time_t t, const char *fmt) {
        // reserve 64 bytes
        reserve(64);
        const char *pfmt = fmt? fmt : datetime::HTTP_FMT;
        ssize_t sz = strftime((char*)(data_+offset_), 64, pfmt, localtime(&t));
        if (sz < 0) {
            strace("formatting time failed: %s", errno_s);
            return;
        }
        offset_ += sz;
    }

    void buffer_t::grow(uint32_t add) {
        // check if the current buffer_t fits
        data_ = (uint8_t *)memory::realloc(data_,(offset_+add+1));
        if (data_ == nullptr)
            throw std::runtime_error(
                    "buffer_t::grow(): error: " + std::string(errno_s));
        // change the size of the memory
        size_ = (uint32_t) memory::fits(data_, (offset_+add+1));
    }

    void buffer_t::reset(size_t size, bool keep) {
        offset_ = 0;
        if (!keep || (size_ < size)) {
            size = (size < 8) ? 8 : size;
            grow((uint32_t) size);
        }
    }

    void buffer_t::seek(off_t off) {
        off_t to = offset_ + off;
        if (to < 0 || to > size_) {
            offset_ = 0;
        }
        else {
            offset_ = (uint32_t) to;
        }
    }

    void buffer_t::bseek(off_t off) {
        if (off < size_ && off >= 0) {
            offset_ = (uint32_t) off;
        }
    }

    void buffer_t::reserve(size_t size) {
        size_t  remaining = size_ - offset_;
        if (size > remaining) {
            grow((uint32_t) (size-remaining));
        }
    }

    char* buffer_t::release() {
        if (data_) {
            char *raw = (char *) (*this);
            data_ = nullptr;
            clear();
            return raw;
        }
        return (char *)"";
    }

    void buffer_t::clear() {
        if (data_) {
            memory::free(data_);
            data_ = nullptr;
        }
        offset_ = 0;
        size_ = 0;
    }

    buffer_t::operator char*() {
        if (data_ && data_[offset_] != '\0') {
            data_[offset_] = '\0';
        }
        return data();
    }

    buffer_t::operator strview_t() {
        if (data_ && offset_) {
            return boost::string_view((const char *) data_, offset_);
        }
        return strview_t();
    }

    file_t::file_t(mfile fd)
        : fd(fd)
    {}

    file_t::file_t(const char *pname, int flags, mode_t mode)
        : file_t(mfopen(pname, flags, mode))
    {
        if (fd == nullptr) {
            throw suil_error::create("opening file '", pname, "' failed: ",
                                     errno_s);
        }
    }

    void file_t::close() {
        if (fd != nullptr) {
            flush(500);
            mfclose(fd);
            fd = nullptr;
        }
    }

    void file_t::flush(int64_t timeout) {
        assert(fd);
        mfflush(fd, utils::after(timeout));
        if (errno) {
            printf("flushing failed: %s", errno_s);
        }
    }

    off_t file_t::seek(off_t off) {
        assert(fd);
        return mfseek(fd, off);
    }

    bool file_t::eof() {
        assert(fd);
        return mfeof(fd) != 0;
    }

    off_t file_t::tell() {
        assert(fd);
        return mftell(fd);
    }

    bool file_t::read(void *buf, size_t &len, int64_t timeout) {
        bool status{true};
        assert(fd);
        size_t ret = mfread(fd, buf, len, utils::after(timeout));
        if (errno) {
            strace("%p: file_t read error: %s", fd, errno_s);
            status = false;
        }
        len = ret;
        return status;
    }


    size_t file_t::write(const void *buf, size_t len, int64_t timeout) {
        assert(fd);
        size_t ret = mfwrite(fd, buf, len, utils::after(timeout));
        if (errno) {
            strace("%p: file_t write error: %s", fd, errno_s);
        }
        return ret;
    }

    file_t::~file_t() {
        close();
    }

    file_logger::file_logger(const std::string dir, const std::string prefix)
        : dst(nullptr)
    {
        open(dir, prefix);
    }

    void file_logger::open(const std::string &dir, const std::string& prefix) {
        if (!utils::fs::exists(dir.c_str())) {
            sdebug("creating file logger directory: %s", dir.c_str());
            utils::fs::mkdir(dir.c_str(), true, 0777);
        }
        zcstring tmp{utils::catstr(dir, "/", prefix, "-", datetime()("%Y%m%d_%H%M%S"), ".log")};
        dst = std::move(file_t(tmp.cstr, O_WRONLY|O_APPEND|O_CREAT, 0666));
    }

    void file_logger::log(const char *data, size_t sz, log::level) {
        dst.write(data, sz, 1500);
        dst.flush(1500);
    }

    zcstring base64::encode(const uint8_t *data, size_t sz) {
        static char b64table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

        char *out = (char *) memory::alloc((sz+2)/3*4);
        char *it = out;
        while (sz >= 3) {
            // |X|X|X|X|X|X|-|-|
            *it++ = b64table[((*data & 0xFC) >> 2)];
            // |-|-|-|-|-|-|X|X|
            uint8_t h = (uint8_t) (*data++ & 0x03) << 4;
            // |-|-|-|-|-|-|X|X|_|X|X|X|X|-|-|-|-|
            *it++ = b64table[h | ((*data & 0xF0) >> 4)];
            // |-|-|-|-|X|X|X|X|
            h = (uint8_t) (*data++ & 0x0F) << 2;
            // |-|-|-|-|X|X|X|X|_|X|X|-|-|-|-|-|-|
            *it++ = b64table[h | ((*data & 0xC0) >> 6)];
            // |-|-|X|X|X|X|X|X|
            *it++ = b64table[(*data++ & 0x3F)];
            sz -= 3;
        }

        if (sz == 1) {
            // pad with ==
            // |X|X|X|X|X|X|-|-|
            *it++ = b64table[((*data & 0xFC) >> 2)];
            // |-|-|-|-|-|-|X|X|
            uint8_t h = (uint8_t) (*data++ & 0x03) << 4;
            *it++ = b64table[h];
            *it++ = '=';
            *it++ = '=';
        } else if (sz == 2) {
            // pad with =
            // |X|X|X|X|X|X|-|-|
            *it++ = b64table[((*data & 0xFC) >> 2)];
            // |-|-|-|-|-|-|X|X|
            uint8_t h = (uint8_t) (*data++ & 0x03) << 4;
            // |-|-|-|-|-|-|X|X|_|X|X|X|X|-|-|-|-|
            *it++ = b64table[h | ((*data & 0xF0) >> 4)];
            // |-|-|-|-|X|X|X|X|
            h = (uint8_t) (*data++ & 0x0F) << 2;
            *it++ = b64table[h];
            *it++ = '=';
        }

        *it = '\0';
        // own the memory
        zcstring ret(out, it-out, true);
        return std::move(ret);
    }

    zcstring base64::decode(const uint8_t *in, size_t size) {
        buffer_t b((uint32_t) (size/4)*3);
        static const unsigned char ASCII_LOOKUP[256] =
        {
            /* ASCII table */
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63,
            52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64,
            64,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
            15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,
            64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
            41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64
        };
        size_t sz = size;
        const uint8_t *it = in;

        while (sz > 4) {
            if (ASCII_LOOKUP[it[0]] == 64 ||
                ASCII_LOOKUP[it[1]] == 64 ||
                ASCII_LOOKUP[it[2]] == 64 ||
                ASCII_LOOKUP[it[3]] == 64)
            {
                // invalid base64 character
                throw std::runtime_error("invalid base64 encoded string passed");
            }

            b.append((uint8_t)(ASCII_LOOKUP[it[0]] << 2 | ASCII_LOOKUP[it[1]] >> 4));
            b.append((uint8_t)(ASCII_LOOKUP[it[1]] << 4 | ASCII_LOOKUP[it[2]] >> 2));
            b.append((uint8_t)(ASCII_LOOKUP[it[2]] << 6 | ASCII_LOOKUP[it[3]]));
            sz -= 4;
            it += 4;
        }
        int i = 0;
        while ((it[i] != '=') && (ASCII_LOOKUP[it[i]] != 64) && (i++ < 4));
        if ((sz-i) && (it[i] != '=')) {
            // invalid base64 character
            throw std::runtime_error("invalid base64 encoded string passed");
        }
        sz -= 4-i;

        if (sz > 1) {
            b.append((uint8_t)(ASCII_LOOKUP[it[0]] << 2 | ASCII_LOOKUP[it[1]] >> 4));
        }
        if (sz > 2) {
            b.append((uint8_t)(ASCII_LOOKUP[it[1]] << 4 | ASCII_LOOKUP[it[2]] >> 2));
        }
        if (sz > 3) {
            b.append((uint8_t)(ASCII_LOOKUP[it[2]] << 6 | ASCII_LOOKUP[it[3]]));
        }

        return std::move(zcstring(b));
    }

    char* utils::strndup(const char *str, size_t len) {
        char *nstr = nullptr;
        if ((nstr = (char *) memory::alloc(len+1)) != nullptr) {
            strncpy(nstr, str, len);
            nstr[len] = '\0';
        }
        return nstr;
    }

    int64_t utils::strtonum(const zcstring &str, int base, long long min, long long max) {
        long long l;
        char *ep;

        if (min > max) {
            throw std::range_error("min value specified is greater than max");
        }

        errno = 0;
        l = strtoll(str.str, &ep, base);
        if (errno != 0 || str.str == ep || !matchany(*ep, '.', '\0')) {
            strace("strtoll error: (str = %p, ep = %p), *ep = %02X, errno = %d",
                    str.str, ep, *ep, errno);
            throw std::range_error("invalid converting t o number failed");
        }

        if (l < min) {
            throw std::range_error("converted value is less than min value");
        }

        if (l > max) {
            throw std::range_error("converted value is greator than max value");
        }

        return (l);
    }

    zcstring utils::find(zcstring &src, char what, size_t after) {
        char *pos = nullptr, *end = src.str + src.len;
        while ((pos = strchr(src.str, what)) && 0 != after--);
        if (pos == nullptr) {
            return zcstring();
        }
        else {
            *pos++ = '\0';
            return zcstring(pos, end-pos, false);
        }
    }

    const std::vector<char *> utils::strsplit(zcstring &str, const char *delim) {
        int		count;
        char		*ap = NULL, *ptr = str.str, *eptr = ptr + str.len;
        std::vector<char*> out;

        count = 0;
        for (ap = strsep(&ptr, delim); ap != NULL; ap = strsep(&ptr, delim)) {
            if (*ap != '\0' && ap != eptr) {
                out.push_back(ap);
                ap++;
                count++;
            }
        }

        return std::move(out);
    }

    zcstring utils::strstrip(zcstring &str, char strip, bool ends) {
        size_t	len = str.len;
        char		*s, *p, *e;

        buffer_t b(str.len);
        void *tmp = b;
        p = (char *)tmp;
        s = str.str;
        e = str.str + (str.len-1);
        while (strip == *s) s++;
        while (strip == *e) e--;
        e++;

        if (ends) {
            memcpy(p, s, (e-s));
            b.seek(e-s);
        }
        else {
            for (; s < e; s++) {
                if (*s == strip)
                    continue;
                *p++ = *s;
            }
            b.seek(p - (char *)tmp);
        }

        // the zcstr will take ownership of the buffer
        return std::move(zcstring(b));
    }

    void* utils::memfind(void *src, size_t slen, const void *needle, size_t len) {
        size_t pos;

        for (pos = 0; pos < slen; pos++) {
            if ( *((u_int8_t *)src + pos) != *(u_int8_t *)needle)
                continue;

            if ((slen - pos) < len)
                return (NULL);

            if (!memcmp((u_int8_t *)src + pos, needle, len))
                return ((u_int8_t *)src + pos);
        }

        return (NULL);
    }

    zcstr<> utils::bytestr(const uint8_t* buf, size_t len) {
        if (buf == nullptr)
            return zcstring{};
        char *OUT = (char *)memory::alloc((len*2)+2);
        ssize_t rc = 0;
        for (size_t i = 0; i < len; i++) {
            rc += sprintf(&OUT[rc], "%02x", buf[i]);
        }
        OUT[rc] = '\0';

        // the memory now belong to caller
        zcstring tmp(OUT, (size_t)rc, true);
        return std::move(tmp);
    }

    inline uint8_t __ctoi(char c) {
        if (c >= '0' && c <= '9') {
            return (uint8_t) (c - '0');
        } else if (c >= 'a' && c <= 'f') {
            return (uint8_t) (c - '`');
        } else if (c >= 'A' && c <= 'F') {
            return (uint8_t) (c - '@');
        }
        throw suil_error::create("invalid hex number");
    };

    void utils::bytearr(const zcstring& str, uint8_t* out, size_t olen) {
        size_t size = str.len>>1;
        if (out == nullptr || olen < size) {
            suil_error::create("utils::bytearr - output buffer invalid");
        }

        int i{0};
        char v;

        char *p = str.str;
        for (i; i < size; i++) {
            out[i] = (uint8_t) (__ctoi(*p++) << 4 || __ctoi(*p++));
        }
    }

    zcstr<> utils::urlencode(const zcstring &str) {
        uint8_t *buf((uint8_t *) memory::alloc(str.len*3));
        char *src = str.str, *end = src + str.len;
        uint8_t *dst = buf;
        register uint8_t c;
        while (src != end) {
            c = *src++;
            if (!isalnum(c) && strchr("-_.~", c) == nullptr) {
                static uint8_t hexchars[] = "0123456789ABCDEF";
                dst[0] = '%';
                dst[1] = hexchars[(c&0xF0)>>4];
                dst[2] = hexchars[(c&0x0F)];
                dst += 3;
            }
            else {
                *dst++ = c;
            }
        }
        *dst = '\0';

        return zcstring((char *)buf, (dst - buf), true);
    }

    zcstr<> utils::randbytes(size_t size) {
        uint8_t buf[size];
        RAND_bytes(buf, (int) size);
        return bytestr(buf, size);
    }

    zcstring utils::md5Hash(const uint8_t *data, size_t len) {
        if (data == nullptr)
            return zcstring{};

        uint8_t RAW[MD5_DIGEST_LENGTH];
        MD5(data, len, RAW);
        return std::move(bytestr(RAW, MD5_DIGEST_LENGTH));
    }

    zcstr<> utils::HMAC_Sha256(zcstr<> &secret, const uint8_t *data, size_t len, bool  b64) {
        if (data == nullptr)
            return zcstring{};

        uint8_t *result = HMAC(EVP_sha256(), secret.cstr, secret.len,
                                  data, len, nullptr, nullptr);
        if (b64) {
            return base64::encode(result, SHA256_DIGEST_LENGTH);
        }
        else {
            return std::move(bytestr(result, SHA256_DIGEST_LENGTH));
        }
    }

    zcstr<> utils::sha256Hash(const uint8_t *data, size_t len, bool b64) {
        if (data == nullptr)
            return zcstring{nullptr};

        uint8_t *result = SHA256(data, len, nullptr);
        if (b64) {
            return base64::encode(bytestr(result, SHA256_DIGEST_LENGTH));
        }
        else {
            return bytestr(result, SHA256_DIGEST_LENGTH);
        }
    }

    static inline bool _mkdir(const char *dir, mode_t m) {
        bool rc = true;
        if (::mkdir(dir, m) != 0)
            if (errno != EEXIST)
                rc = false;
        return rc;
    }

    static inline bool _mkpath(char *p, mode_t m) {
        char *base = p, *dir = p;
        bool rc;

        while (*dir == '/') dir++;

        while ((dir = strchr(dir,'/')) != nullptr) {
            *dir = '\0';
            rc = _mkdir(base, m);
            *dir = '/';
            if (!rc)
                break;

            while (*dir == '/') dir++;
        }


        return _mkdir(base, m);
    }

    void utils::fs::mkdir(const char *path, bool recursive, mode_t mode) {

        bool status;
        zcstring tmp = zcstring(path).dup();
        if (!tmp) {
            /* creating directory failed */
            throw suil_error::create("mkdir '", path, "' failed: ",  errno_s);
        }

        if (recursive)
            status = _mkpath(tmp.str, mode);
        else
            status = _mkdir(tmp.cstr, mode);

        if (!status) {
            /* creating directory failed */
            throw suil_error::create("mkdir '", path, "' failed: ",  errno_s);
        }
    }

    static void _forall(const char *base, zcstring& path, std::function<bool(const zcstring&, bool)> h, bool recursive, bool pod) {
        zcstring cdir(utils::catstr(base, "/", path));
        DIR *d = opendir(cdir.cstr);

        if (d == nullptr) {
            /* openining directory failed */
            throw suil_error::create("opendir('", path, "') failed: ", errno_s);
        }

        struct dirent *tmp;

        while ((tmp = readdir(d)) != NULL)
        {
            /* ignore parent and current directory */
            if (utils::strmatchany(tmp->d_name, ".", ".."))
                continue;


            zcstring ipath = path? utils::catstr(path, "/", tmp->d_name) : zcstring(tmp->d_name);
            bool is_dir{tmp->d_type == DT_DIR};

            if (!pod) {
                if (!h(ipath, is_dir)) {
                    /* delegate cancelled travesal*/
                    break;
                }

                if (recursive && is_dir) {
                    /* recursively iterate current directory*/
                    _forall(base, ipath, h, recursive, pod);
                }
            }
            else {
                if (recursive && is_dir) {
                    /* recursively iterate current directory*/
                    _forall(base, ipath, h, recursive, pod);
                }

                if (!h(ipath, is_dir)) {
                    /* delegate cancelled travesal*/
                    break;
                }
            }
        }

        closedir(d);
    }

    void utils::fs::forall(const char *path, std::function<bool(const zcstring&, bool)> h, bool recursive, bool pod) {
        zcstring tmp{""};
        _forall(path, tmp, h, recursive, pod);
    }

    std::vector<zcstring> utils::fs::ls(const char *path, bool recursive) {
        std::vector<zcstring> all;
        fs::forall(path, [&all](const zcstring& d, bool dir) {
            all.emplace_back(d.dup());
            return true;
        }, recursive);

        return std::move(all);
    }

    static inline void __remove(const char *path) {
        if (::remove(path) != 0) {
            throw suil_error::create("remove '", path, "' failed: ", errno_s);
        }
    }

    static inline void _rmdir(const char *path) {
        utils::fs::forall(path,
        [&](const zcstring& name, bool d) -> bool {
            zcstring tmp = utils::catstr(path, "/", name);
            __remove(tmp.cstr);
            return true;
        }, true, true);
    }

    void utils::fs::remove(const char *path, bool recursive, bool contents) {
        bool is_dir{isdir(path)};
        if (recursive && is_dir) {
            _rmdir(path);
        }

        if (!is_dir || !contents)
            __remove(path);
    }

    zcstring utils::fs::readall(const char *path, bool async) {
        /* read file contents into a buffer */
        if (!exists(path)) {
            throw suil_error::create("file '", path, "' does not exist");
        }

        struct stat st;
        if (::stat(path, &st)) {
            /* getting file information failed */
            throw suil_error::create("stat('", path, "') failed: ", errno_s);
        }

        if (st.st_size > 8910) {
            /* size too large to be read by this API */
            throw suil_error::create("file '", path, "' to large (",
                                     st.st_size, " bytes) to be read by fs::read_all");
        }

        int fd = open(path, O_RDONLY);
        if (fd < 0) {
            /* opening file failed */
            throw suil_error::create("opening file '", path, "' failed: ", errno_s);
        }

        buffer_t b((uint32_t) st.st_size);
        char *data = b.data();
        ssize_t nread = 0, rc = 0;
        do {
            rc = ::read(fd, &data[nread], (size_t)(st.st_size - nread));
            if (rc < 0) {
                /* reading file failed */
                throw suil_error::create("reading '", path, "' failed: ", errno_s);
            }

            nread += rc;
        } while (nread < st.st_size);
        b.seek(nread);

        return zcstring(b);
    }

    void utils::fs::append(const char *path, const void *data, size_t sz, bool async) {
        file_t f(path, O_WRONLY|O_CREAT|O_APPEND, 0666);
        f.write(data, sz, 1500);
        f.close();
    }

    zcstring utils::uuidstr(unsigned char *id) {
        static uuid_t UUID;
        if (id == nullptr) {
            id = uuid(UUID);
        }

        char OUT[(sizeof(uuid_t)*2+4)];
        ssize_t rc = 0;
        for (size_t i = 0; i < sizeof(uuid_t); i++) {
            rc += sprintf(&OUT[rc], "%02x", id[i]);
        }

        return std::move(zcstring(OUT).dup());
    }

    const char *utils::mimetype(const zcstring filename) {
        const char *ext = strrchr(filename.cstr, '.');
        static zcstr_map_t<const char*> mimetypes = {
            {".html", "text/html"},
            {".css",  "text/css"},
            {".csv",  "text/csv"},
            {".txt",  "text/plain"},
            {".sgml", "text/sgml"},
            {".tsv",  "text/tab-separated-values"},
            // add compressed mime types
            {".bz",   "application/x-bzip"},
            {".bz2",  "application/x-bzip2"},
            {".gz",   "application/x-gzip"},
            {".tgz",  "application/x-tar"},
            {".tar",  "application/x-tar"},
            {".zip",  "application/zip, application/x-compressed-zip"},
            {".7z",   "application/zip, application/x-compressed-zip"},
            // add image mime types
            {".jpg",  "image/jpeg"},
            {".png",  "image/png"},
            {".svg",  "image/svg+xml"},
            {".gif",  "image/gif"},
            {".bmp",  "image/bmp"},
            {".tiff", "image/tiff"},
            {".ico",  "image/x-icon"},
            // add video mime types
            {".avi",  "video/avi"},
            {".mpeg", "video/mpeg"},
            {".mpg",  "video/mpeg"},
            {".mp4",  "video/mp4"},
            {".qt",   "video/quicktime"},
            // add audio mime types
            {".au",   "audio/basic"},
            {".midi", "audio/x-midi"},
            {".mp3",  "audio/mpeg"},
            {".ogg",  "audio/vorbis, application/ogg"},
            {".ra",   "audio/x-pn-realaudio, audio/vnd.rn-realaudio"},
            {".ram",  "audio/x-pn-realaudio, audio/vnd.rn-realaudio"},
            {".wav",  "audio/wav, audio/x-wav"},
            // Other common mime types
            {".json", "application/json"},
            {".js",   "application/javascript"},
            {".ttf",  "font/ttf"},
            {".xhtml","application/xhtml+xml"},
            {".xml",  "application/xml"}
        };

        if (ext != nullptr) {
            zcstring tmp(ext);
            auto it = mimetypes.find(tmp);
            if (it != mimetypes.end())
                return it->second;
        }

        return "text/plain";
    }
}