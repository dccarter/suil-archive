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

    bool load(bool si) {
        static bool loaded{false};
        if (loaded) return false;
        //signal(SIGPIPE, SIG_IGN);
        if (si) {
            // display version only if explicitly requested
            console::println("");
            console::println("Powered by suil C++1y web framework");
            console::printred("v" SUIL_VERSION_STRING "\n");
            console::printblue("http://suil.suilteam.com\n");
            console::println("---------------------------------------\n");
        }
        memory::init();
        loaded = true;

        return loaded;
    }

    namespace __internal {

        void chwdir(const std::string& to) {
            if (!utils::fs::isdir(to.c_str())) {
                throw SuilError::create("working directory: '", to, "' does not exist");
            }

            if (::chdir(to.c_str())) {
                throw SuilError::create("setting working dir to('", to, "') failed: ", errno_s);
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
                serror("Creating Session ID for daemon failed: %s", errno_s);
                closelog();
                exit(EXIT_FAILURE);
            }
            sinfo("Created Session ID %d for suil daemon", sid);

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

    Datetime::Datetime(time_t t)
        : t_(t)
    {
        gmtime_r(&t_, &tm_);
    }

    Datetime::Datetime()
        : Datetime(time(nullptr))
    {}

    Datetime::Datetime(const char *fmt, const char *str)
    {
        const char *tmp = HTTP_FMT;
        if (fmt) {
            tmp = fmt;
        }
        strptime(str, tmp, &tm_);
    }

    Datetime::Datetime(const char *http_time)
        : Datetime(HTTP_FMT, http_time)
    {}

    zbuffer::zbuffer(size_t init_size)
        : data_{nullptr},
          size_((uint32_t) init_size),
          offset_(0)
    {
        if (size_)
            grow(init_size);
    }

    zbuffer::zbuffer(zbuffer && other)
        : data_(other.data_),
          size_(other.size_),
          offset_(other.offset_)
    {
        other.size_ = 0;
        other.offset_ = 0;
        other.data_ = nullptr;
    }

    zbuffer& zbuffer::operator=(zbuffer &&other) {
        size_ = other.size_;
        data_ = other.data_;
        offset_ = other.offset_;
        other.data_ = nullptr;
        other.offset_ = other.size_ = 0;

        return *this;
    }

    zbuffer::~zbuffer() {
        if (data_ != nullptr) {
            memory::free(data_);
            data_ = nullptr;
            size_ = 0;
            offset_ = 0;
        }
    }

    void zbuffer::appendf(const char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        appendv(fmt, args);
        va_end(args);
    }

    void zbuffer::appendv(const char *fmt, va_list args) {
        char	sb[2048];
        int ret;
        ret = vsnprintf(sb, sizeof(sb), fmt, args);
        if (ret == -1) {
            throw std::runtime_error(
                    "zbuffer::appendv(): error: " + std::string(errno_s));
        }
        append(sb, (uint32_t) ret);
    }

    void zbuffer::appendnf(uint32_t hint, const char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        appendnv(hint, fmt, args);
        va_end(args);
    }

    void zbuffer::appendnv(uint32_t hint, const char *fmt, va_list args) {
        if ((size_-offset_) < hint)
            grow(hint);

        int ret;
        assert(data_ && hint);
        ret = vsnprintf((char *)(data_+offset_), (size_-offset_), fmt, args);
        if (ret == -1 || (ret + offset_) > size_) {
            throw std::runtime_error(
                    "zbuffer::appendnv(): error: " + std::string(errno_s));
        }
        offset_ += ret;
    }

    void zbuffer::append(const void *data, size_t len) {
        if ((size_-offset_) < len) {
            grow(len);
        }
        if (len)
            assert(data_);
        memcpy((data_+offset_), data, len);
        offset_ += len;
    }

    void zbuffer::append(time_t t, const char *fmt) {
        // reserve 64 bytes
        reserve(64);
        const char *pfmt = fmt? fmt : Datetime::HTTP_FMT;
        ssize_t sz = strftime((char*)(data_+offset_), 64, pfmt, localtime(&t));
        if (sz < 0) {
            strace("formatting time failed: %s", errno_s);
            return;
        }
        offset_ += sz;
    }

    void zbuffer::grow(uint32_t add) {
        // check if the current zbuffer fits
        data_ = (uint8_t *)memory::realloc(data_,(size_+add+1));
        if (data_ == nullptr)
            throw std::runtime_error(
                    "zbuffer::grow(): error: " + std::string(errno_s));
        // change the size of the memory
        size_ = (uint32_t) memory::fits(data_, (size_+add+1));
    }

    void zbuffer::reset(size_t size, bool keep) {
        offset_ = 0;
        if (!keep || (size_ < size)) {
            size = (size < 8) ? 8 : size;
            grow((uint32_t) size);
        }
    }

    void zbuffer::seek(off_t off = 0) {
        off_t to = offset_ + off;
        if (off <= 0 || to > size_) {
            offset_ = 0;
        }
        else {
            offset_ = (uint32_t) to;
        }
    }

    void zbuffer::bseek(off_t off) {
        if (off < size_ && off >= 0) {
            offset_ = (uint32_t) off;
        }
    }

    void zbuffer::reserve(size_t size) {
        size_t  remaining = size_ - offset_;
        if (size > remaining) {
            grow((uint32_t) (size-remaining));
        }
    }

    char* zbuffer::release() {
        if (data_) {
            char *raw = (char *) (*this);
            data_ = nullptr;
            clear();
            return raw;
        }
        return (char *)"";
    }

    void zbuffer::clear() {
        if (data_) {
            memory::free(data_);
            data_ = nullptr;
        }
        offset_ = 0;
        size_ = 0;
    }

    zbuffer::operator char*() {
        if (data_ && data_[offset_] != '\0') {
            data_[offset_] = '\0';
        }
        return data();
    }

    zbuffer::operator strview() {
        if (data_ && offset_) {
            return boost::string_view((const char *) data_, offset_);
        }
        return strview();
    }

    File::File(mfile fd)
        : fd(fd)
    {}

    File::File(int fd, bool own)
        : fd(mfcreate(fd, own))
    {
        if (Ego.fd == nullptr) {
            throw SuilError::create("creating file for descriptor '", fd, "' failed: ",
                                    errno_s);
        }
    }

    File::File(const char *pname, int flags, mode_t mode)
        : File(mfopen(pname, flags, mode))
    {
        if (fd == nullptr) {
            throw SuilError::create("opening file '", pname, "' failed: ",
                                     errno_s);
        }
    }

    void File::close() {
        if (fd != nullptr) {
            flush(500);
            mfclose(fd);
            fd = nullptr;
        }
    }

    void File::flush(int64_t timeout) {
        assert(fd);
        mfflush(fd, utils::after(timeout));
        if (errno) {
            printf("flushing failed: %s", errno_s);
        }
    }

    off_t File::seek(off_t off) {
        assert(fd);
        return mfseek(fd, off);
    }

    bool File::eof() {
        assert(fd);
        return mfeof(fd) != 0;
    }

    off_t File::tell() {
        assert(fd);
        return mftell(fd);
    }

    bool File::read(void *buf, size_t &len, int64_t timeout) {
        bool status{true};
        assert(fd);
        size_t ret = mfread(fd, buf, len, utils::after(timeout));
        if (errno) {
            strace("%p: File read error: %s", fd, errno_s);
            status = false;
        }
        len = ret;
        return status;
    }


    size_t File::write(const void *buf, size_t len, int64_t timeout) {
        assert(fd);
        size_t ret = mfwrite(fd, buf, len, utils::after(timeout));
        if (errno) {
            strace("%p: File write error: %s", fd, errno_s);
        }
        return ret;
    }

    File::~File() {
        close();
    }

    FileLogger::FileLogger(const std::string dir, const std::string prefix)
        : dst(nullptr)
    {
        open(dir, prefix);
    }

    void FileLogger::open(const std::string &dir, const std::string& prefix) {
        if (!utils::fs::exists(dir.c_str())) {
            sdebug("creating file logger directory: %s", dir.c_str());
            utils::fs::mkdir(dir.c_str(), true, 0777);
        }
        zcstring tmp{utils::catstr(dir, "/", prefix, "-", Datetime()("%Y%m%d_%H%M%S"), ".log")};
        dst = std::move(File(tmp.data(), O_WRONLY|O_APPEND|O_CREAT, 0666));
    }

    void FileLogger::log(const char *data, size_t sz, log::level) {
        dst.write(data, sz, 1500);
        dst.flush(1500);
    }

    Syslog::Syslog(const char *name) {
        openlog("suil", LOG_PID|LOG_CONS, LOG_USER);
    }

    void Syslog::close() {
        closelog();
    }

    void Syslog::log(const char *msg, size_t, log::level l) {
        int prio{LOG_INFO};
        switch(l) {
            case log::level::DEBUG:
            case log::level::TRACE:
                prio = LOG_DEBUG; break;
            case log::level::NOTICE:
                prio = LOG_NOTICE; break;
            case log::level::ERROR:
                prio = LOG_ERR; break;
            case log::level::WARNING:
                prio = LOG_WARNING; break;
            case log::level::CRITICAL:
                prio = LOG_CRIT; break;
            default:
                break;
        }
        syslog(prio, msg);
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
        zbuffer b((uint32_t) (size/4)*3);
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
        l = strtoll(str.data(), &ep, base);
        if (errno != 0 || str.data() == ep || !matchany(*ep, '.', '\0')) {
            strace("strtoll error: (str = %p, ep = %p), *ep = %02X, errno = %d",
                    str(), ep, *ep, errno);
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
        char *pos = nullptr, *end = src.data() + src.size();
        while ((pos = strchr(src.data(), what)) && 0 != after--);
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
        char		*ap = NULL, *ptr = str.data(), *eptr = ptr + str.size();
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
        size_t	len = str.size();
        char		*s, *p, *e;

        zbuffer b(str.size());
        void *tmp = b;
        p = (char *)tmp;
        s = str.data();
        e = str.data() + (str.size()-1);
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

    size_t utils::hexstr(const uint8_t *in, size_t ilen, char *out, size_t olen) {
        if (in == nullptr || out == nullptr || olen < (ilen<<1))
            return 0;

        size_t rc = 0;
        for (size_t i = 0; i < ilen; i++) {
            out[rc++] = i2c((uint8_t) (0x0F&(in[i]>>4)));
            out[rc++] = i2c((uint8_t) (0x0F&in[i]));
        }
        return rc;
    }

    zcstring utils::hexstr(const uint8_t *buf, size_t len) {
        if (buf == nullptr)
            return zcstring{};
        size_t rc = 2+(len<<1);
        char *OUT = (char *)memory::alloc(rc);

        zcstring tmp{nullptr};
        rc = hexstr(buf, len, OUT, rc);
        if (rc) {
            OUT[rc] = '\0';
            tmp = zcstring(OUT, (size_t)rc, true);
        }
        return std::move(tmp);
    }

    void utils::bytes(const zcstring &str, uint8_t *out, size_t olen) {
        size_t size = str.size()>>1;
        if (out == nullptr || olen < size) {
            SuilError::create("utils::bytes - output buffer invalid");
        }

        int i{0};
        char v;

        const char *p = str.data();
        for (i; i < size; i++) {
            out[i] = (uint8_t) (suil::c2i(*p++) << 4 | suil::c2i(*p++));
        }
    }

    zcstring utils::urlencode(const zcstring &str) {
        uint8_t *buf((uint8_t *) memory::alloc(str.size()*3));
        const char *src = str.data(), *end = src + str.size();
        uint8_t *dst = buf;
        uint8_t c;
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

    char *__urldecode(const char *src, const int src_len, char *out, int& out_sz)
    {
#define IS_HEX_CHAR(ch) \
        ((ch >= '0' && ch <= '9') || \
         (ch >= 'a' && ch <= 'f') || \
         (ch >= 'A' && ch <= 'F'))

#define HEX_VALUE(ch, value) \
        if (ch >= '0' && ch <= '9') \
        { \
            value = ch - '0'; \
        } \
        else if (ch >= 'a' && ch <= 'f') \
        { \
            value = ch - 'a' + 10; \
        } \
        else \
        { \
            value = ch - 'A' + 10; \
        }

        const unsigned char *start;
        const unsigned char *end;
        char *dest;
        unsigned char c_high;
        unsigned char c_low;
        int v_high;
        int v_low;

        dest = out;
        start = (unsigned char *)src;
        end = (unsigned char *)src + src_len;
        while (start < end)
        {
            if (*start == '%' && start + 2 < end)
            {
                c_high = *(start + 1);
                c_low  = *(start + 2);

                if (IS_HEX_CHAR(c_high) && IS_HEX_CHAR(c_low))
                {
                    HEX_VALUE(c_high, v_high);
                    HEX_VALUE(c_low, v_low);
                    *dest++ = (v_high << 4) | v_low;
                    start += 3;
                }
                else
                {
                    *dest++ = *start;
                    start++;
                }
            }
            else if (*start == '+')
            {
                *dest++ = ' ';
                start++;
            }
            else
            {
                *dest++ = *start;
                start++;
            }
        }

        out_sz = (int) (dest - out);
        return dest;
    }

    zcstring utils::urldecode(const char *src, size_t len)
    {
        char out[1024];
        int  size{1023};
        (void)__urldecode(src, (int)len, out, size);
        return zcstring{out, (size_t)size, false}.dup();
    }

    void utils::randbytes(uint8_t out[], size_t size) {
        RAND_bytes(out, (int) size);
    }

    zcstring utils::randbytes(size_t size) {
        uint8_t buf[size];
        RAND_bytes(buf, (int) size);
        return hexstr(buf, size);
    }

    zcstring utils::md5(const uint8_t *data, size_t len) {
        if (data == nullptr)
            return zcstring{};

        uint8_t RAW[MD5_DIGEST_LENGTH];
        MD5(data, len, RAW);
        return std::move(hexstr(RAW, MD5_DIGEST_LENGTH));
    }

    zcstring utils::shaHMAC256(zcstring &secret, const uint8_t *data, size_t len, bool b64) {
        if (data == nullptr)
            return zcstring{};

        uint8_t *result = HMAC(EVP_sha256(), secret.data(), secret.size(),
                                  data, len, nullptr, nullptr);
        if (b64) {
            return base64::encode(result, SHA256_DIGEST_LENGTH);
        }
        else {
            return std::move(hexstr(result, SHA256_DIGEST_LENGTH));
        }
    }

    zcstring utils::sha256(const uint8_t *data, size_t len, bool b64) {
        if (data == nullptr)
            return zcstring{nullptr};

        uint8_t *result = SHA256(data, len, nullptr);
        if (b64) {
            return base64::encode(hexstr(result, SHA256_DIGEST_LENGTH));
        }
        else {
            return hexstr(result, SHA256_DIGEST_LENGTH);
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
        zcstring tmp = zcstring{path}.dup();
        if (!tmp) {
            /* creating directory failed */
            throw SuilError::create("mkdir '", path, "' failed: ",  errno_s);
        }

        if (recursive)
            status = _mkpath(tmp.data(), mode);
        else
            status = _mkdir(tmp.data(), mode);

        if (!status) {
            /* creating directory failed */
            throw SuilError::create("mkdir '", path, "' failed: ",  errno_s);
        }
    }

    static void _forall(const char *base, zcstring& path, std::function<bool(const zcstring&, bool)> h, bool recursive, bool pod) {
        zcstring cdir(utils::catstr(base, "/", path));
        DIR *d = opendir(cdir.data());

        if (d == nullptr) {
            /* openining directory failed */
            throw SuilError::create("opendir('", path, "') failed: ", errno_s);
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
            throw SuilError::create("remove '", path, "' failed: ", errno_s);
        }
    }

    static inline void _rmdir(const char *path) {
        utils::fs::forall(path,
        [&](const zcstring& name, bool d) -> bool {
            zcstring tmp = utils::catstr(path, "/", name);
            __remove(tmp.data());
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
            throw SuilError::create("file '", path, "' does not exist");
        }

        struct stat st;
        if (::stat(path, &st)) {
            /* getting file information failed */
            throw SuilError::create("stat('", path, "') failed: ", errno_s);
        }

        if (st.st_size > 8188) {
            /* size too large to be read by this API */
            throw SuilError::create("file '", path, "' to large (",
                                     st.st_size, " bytes) to be read by fs::read_all");
        }

        int fd = open(path, O_RDONLY);
        if (fd < 0) {
            /* opening file failed */
            throw SuilError::create("opening file '", path, "' failed: ", errno_s);
        }

        zbuffer b((uint32_t) st.st_size);
        char *data = b.data();
        ssize_t nread = 0, rc = 0;
        do {
            rc = ::read(fd, &data[nread], (size_t)(st.st_size - nread));
            if (rc < 0) {
                /* reading file failed */
                throw SuilError::create("reading '", path, "' failed: ", errno_s);
            }

            nread += rc;
        } while (nread < st.st_size);
        b.seek(nread);

        return zcstring(b);
    }

    void utils::fs::append(const char *path, const void *data, size_t sz, bool async) {
        File f(path, O_WRONLY|O_CREAT|O_APPEND, 0666);
        f.write(data, sz, 1500);
        f.close();
    }

    zcstring utils::uuidstr(unsigned char *id) {
        static uuid_t UUID;
        if (id == nullptr) {
            id = uuid(UUID);
        }
        size_t olen{(size_t)(10+(sizeof(uuid_t)<1))};
        char out[olen];
        int i{0}, rc{0};
        for(i; i<4; i++) {
            out[rc++] = i2c((uint8_t) (0x0F&(UUID[i]>>4)));
            out[rc++] = i2c((uint8_t) (0x0F& UUID[i]));
        }
        out[rc++] = '-';

        for (i; i < 10; i++) {
            out[rc++] = i2c((uint8_t) (0x0F&(UUID[i]>>4)));
            out[rc++] = i2c((uint8_t) (0x0F& UUID[i]));
            if (i&0x1) {
                out[rc++] = '-';
            }
        }

        for(i; i<sizeof(uuid_t); i++) {
            out[rc++] = i2c((uint8_t) (0x0F&(UUID[i]>>4)));
            out[rc++] = i2c((uint8_t) (0x0F& UUID[i]));
        }

        return zcstring{out, (size_t) rc, false}.dup();
    }

    const char *utils::mimetype(const zcstring filename) {
        const char *ext = strrchr(filename.data(), '.');
        static zmap<const char*> mimetypes = {
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