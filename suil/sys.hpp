//
// Created by dc on 31/05/17.
//

#ifndef SUIL_SYS_HPP
#define SUIL_SYS_HPP

#include <string.h>
#include <stdint.h>
#include <time.h>

#include <unordered_map>

#include "mem.hpp"
#include "symbols.h"

#include "boost/utility/string_view.hpp"
#include "iod/json.hh"
#include "iod/parse_command_line.hh"
#include "libmill/libmill.h"

#define errno_s strerror(errno)

namespace suil {

    typedef uint8_t spid_t;
    extern const spid_t& spid;

    namespace version {
        extern const uint16_t  MAJOR;
        extern const uint16_t  MINOR;
        extern const uint16_t  PATCH;
        extern const uint16_t  BUILD;
        extern const char*     TAG;
        extern const char*     STRING;
        extern const char*     SWNAME;
    };

    typedef decltype(iod::D(
        prop(vmajor, uint64_t),
        prop(vminor, uint64_t),
        prop(vpatch, uint64_t),
        prop(vbuild, uint64_t),
        prop(vtag, std::string),
        prop(vdate, std::string),
        prop(vtime, std::string)
    )) version_t;
    extern const version_t& ver_json;

    struct datetime {
        datetime(time_t);
        datetime();
        datetime(const char *http_time);
        datetime(const char *fmt, const char *str);
        const char* str(char *out, size_t sz, const char *fmt) {
            if (!out || !sz || !fmt) {
                return nullptr;
            }
            (void) strftime(out, sz, fmt, &tm_);
            return out;
        }

        const char* operator()() {
            static char buf[64] = {0};
            return str(buf, sizeof(buf), LOG_FMT);
        }

        const char* operator()(const char *fmt) {
            static char buf[64] = {0};
            return str(buf, sizeof(buf), fmt);
        }

        const char* operator()(char *buf, size_t sz, const char *fmt) {
            return str(buf, sz, fmt);
        }

        inline operator const char *() {
            return asctime(&tm_);
        }

        inline operator const tm&() const {
            return tm_;
        }

        inline operator time_t() {
            if (t_ == 0) {
                t_ = timegm(&tm_);
            }
            return t_;
        }

    private:
        struct tm       tm_{};
        time_t          t_{0};

    public:
        static constexpr char *HTTP_FMT = (char *) "%a, %d %b %Y %T GMT";
        static constexpr char *LOG_FMT  = (char *) "%Y-%m-%d %H:%M:%S";
    };

    /**
     * This is a type which basically does nothing, its void
     * and doesn't have any significant meaning. It can be passed
     * around as a template parameter where the parameter value is
     * insignificant
     */
    struct __Void {
        template <typename... __A>
        __Void(__A...) {}
        operator bool() const { return false; }
        template <typename... __A>
        void operator()(__A...){}
        template <typename __A>
        bool operator==(const __A& a) const { return this == &a; }
        template <typename __A>
        bool operator!=(const __A& a) const { return this != &a; }

        uint8_t byte[0];
    } __attribute((packed));

    /**
     * a global value of type __Void which can be passed around
     * to functions that accept template parameters and the parameter
     * is not really needed
     */
    extern __Void Void;

    struct null_t {};

    /**
     * string view as defined by
     * @see http://www.boost.org/doc/libs/1_62_0/boost/utility/string_view.hpp
     */
    using strview_t = boost::string_view;

    namespace utils {
        /**
         * compute the deadline time in milliseconds given a timeout
         * @param tout the timeout whose deadline will be computed
         * @return the computed deadline, if the given timeout is less
         * than 0, the deadline will be -1
         */
        static inline int64_t after(int64_t tout) {
            return tout < 0 ? -1 : mnow() + tout;
        }

        static inline int64_t tabs(int64_t add) {
            return after(add);
        }

        /**
         * Duplicate \param n_chars the given string into a newly allocated buffer_t.
         * @param src the string (or character buffer_t) to duplicate
         * @param n_chars the number of characters to copy
         * @return a buffer_t with \param n_chars copied from \param src if the copy
         * was successful, otherwise NULL will be returned
         *
         * @note the returned buffer_t should be freed using @see memory::free
         */
        char *strndup(const char *src, size_t n_chars);

        /**
         * Duplicate the given string into a new string
         * @param src the string to copy to a new buffer_t
         * @return a copy of the given string in a separate buffer_t or NULL
         * if the duplication failed
         *
         * @note the returned buffer_t should be freed using @see memory::free
         */
        static inline char *strdup(const char *str) {
            return utils::strndup(str, strlen(str));
        }
    }

    template <typename _Free>
    struct zcstr;

    /**
     * A dynamic auto-grow buffer which can be used for
     * for various memory manipulation
     */
    struct buffer_t {
        buffer_t(uint32_t init_size);
        buffer_t()
            : buffer_t(0)
        {}

        buffer_t(buffer_t&&);
        buffer_t&operator=(buffer_t&& other);
        ~buffer_t();

        void append(const void *data, uint32_t);

        inline void append(unsigned char c) {
            append(&c, sizeof(unsigned char));
        }

        inline void append(unsigned short c) {
            append(&c, sizeof(unsigned short));
        }

        inline void append(unsigned int c) {
            append(&c, sizeof(unsigned int));
        }

        inline void append(uint64_t c) {
            append(&c, sizeof(uint64_t));
        }

        inline void append(char c) {
            append(&c, sizeof(char));
        }

        inline void append(short c) {
            append(&c, sizeof(short));
        }

        void append(int c) {
            append(&c, sizeof(int));
        }

        void append(int64_t c) {
            append(&c, sizeof(int64_t));
        }

        void append(float c) {
            append(&c, sizeof(float));
        }

        void append(double c) {
            append(&c, sizeof(double));
        }

        void append(time_t, const char* fmt);

        inline void append(const char* str) {
            append(str, (uint32_t) strlen(str));
        }

        inline void append(const buffer_t& other) {
            append(other.data(), (uint32_t) other.size());
        }
        void appendf(const char *fmt, ...);

        void appendnf(uint32_t hint, const char *fmt,...);

        void appendv(const char *fmt, va_list args);

        void appendnv(uint32_t hint, const char *fmt, va_list args);

        void reset(size_t size, bool keep = false);

        void seek(off_t off);

        void bseek(off_t off);

        void reserve(size_t size);

        void clear();

        char* release();

        operator char*();

        operator const char*() const {
            return data();
        }

        operator std::string() const {
            return std::string((char*)data_, offset_);
        }

        operator strview_t();

        operator bool() {
            return (offset_ != 0) &&
                   (data_ != nullptr) &&
                   (size_ != 0);
        }

        size_t size() const {
            return offset_;
        }

        char *data() const {
            if (data_)
                return (char *) data_;
            return (char *) data_;
        }

        size_t capacity() const {
            return size_;
        }

        operator const void*() {
            return data_;
        }

        buffer_t&operator+=(const char *str) {
            append(str);
            return *this;
        }

        buffer_t&operator+=(const std::string& str) {
            append(str.c_str());
            return *this;
        }

        buffer_t&operator+=(const strview_t& sv) {
            append(sv.data(), sv.size());
            return *this;
        }

        buffer_t& operator<<(unsigned char u) {
            appendf("%hhu", u);
            return *this;
        }

        buffer_t& operator<<(unsigned short u) {
            appendf("%hu", u);
            return *this;
        }

        buffer_t& operator<<(unsigned int u) {
            appendf("%u", u);
            return *this;
        }

        buffer_t& operator<<(unsigned long ul) {
            appendf("%lu", ul);
            return *this;
        }

        buffer_t& operator<<(unsigned long long ull) {
            appendf("%llu", ull);
            return *this;
        }

        buffer_t& operator<<(char i) {
            appendf("%c", i);
            return *this;
        }

        buffer_t& operator<<(short i) {
            appendf("%hd", i);
            return *this;
        }

        buffer_t& operator<<(int i) {
            appendf("%d", i);
            return *this;
        }

        buffer_t& operator<<(long l) {
            appendf("%ld", l);
            return *this;
        }

        buffer_t& operator<<(long long ll) {
            appendf("%lld", ll);
            return *this;
        }

        buffer_t& operator<<(double d) {
            appendf("%f", d);
            return *this;
        }

        buffer_t& operator<<(const char *str) {
            append(str);
            return *this;
        }

        buffer_t& operator<<(char *str) {
            append(str);
            return *this;
        }

        template <typename _Free>
        buffer_t& operator<<(const zcstr<_Free>& s);

        template <typename _Free>
        buffer_t& operator<<(zcstr<_Free>& s);

        buffer_t& operator<<(const std::string &str) {
            append(str.c_str());
            return *this;
        }

        buffer_t& operator<<(const strview_t& sv) {
            append(sv.data(), sv.size());
            return *this;
        }

        buffer_t&operator<<(const buffer_t& other) {
            append(other.data_, other.offset_);
            return *this;
        }

        template <typename _TJson>
        buffer_t&operator<<(const _TJson& json) {
            std::string str = iod::json_encode(json);
            append(str.c_str(), str.size());
            return *this;
        }

        uint8_t& operator[](size_t index) {
            if (index <= offset_) {
                return data_[index];
            }
            throw std::runtime_error("index out of bounds");
        }

    private:
        void grow(uint32_t);
        uint8_t         *data_;
        uint32_t        size_;
        uint32_t        offset_{0};
    };
    // each buffer_t overhead is 16 bytes
    static_assert(sizeof(buffer_t) <= 16);

    struct zcstr_free {
        void operator()(char *str) {
            memory::free(str);
        }
    };

    /**
     * this is basically just like a string view but with added
     * functionality, it is a zero copy string. if the string is given
     * the buffer, it will own the buffer and is responsible for deallocating
     * the memory after. Should be used wisely
     * @tparam __Free the callback that will be used to free the memory
     */
    template <typename __Free = zcstr_free>
    struct zcstr {
        union {
            char *str;
            const char *cstr;
        };

        uint32_t len{0};
        uint8_t  own{0};
        size_t   hash{0};

        zcstr()
            : str(nullptr),
              len(0),
              own(0)
        {}

        zcstr(const char *str)
            : cstr(str),
              len((uint32_t)(str? strlen(str): 0)),
              own(0)
        {}

        zcstr(const char *str, size_t len, bool own = true)
            : cstr(str),
              len((uint32_t)len),
              own((uint8_t)(own? 1 : 0))
        {}

        zcstr(buffer_t& b, bool own = true)
        {
            len = (uint32_t ) b.size();
            this->own = (uint8_t) (own? 1 : 0);
            if (this->own) {
                str = b.release();
            }
            else {
                str = b.data();
            }
        }

        zcstr(zcstr&& s)
            : str(s.str),
              len(s.len),
              own(s.own)
        {
            s.str = nullptr;
            s.len = 0;
            s.own = 0;
        }

        zcstr& operator=(zcstr&& s) {
            str = s.str;
            len = s.len;
            own = s.own;

            s.str = nullptr;
            s.len = s.own = 0;

            return *this;
        }

        zcstr(const zcstr& s)
            : str(s.str),
              len(s.len),
              own(0),
              hash(s.hash)
        {}

        zcstr operator=(const zcstr& s) {
            str  = s.str;
            len  = s.len;
            own  = 0;
            hash = s.hash;
        }

        zcstr copy() const {
            zcstr zc(utils::strndup(str, len), len, true);
            return std::move(zc);
        }

        const zcstr peek() const {
            // this will return a copy of the string but as
            // just a reference or simple not owner
            return zcstr(cstr, len, false);
        }

        bool empty() const {
            return str == nullptr || len == 0;
        }

        operator bool() const {
            return !empty();
        }

        operator strview_t() const {
            return strview_t(str, len);
        }

        bool operator==(const zcstr& s) const {
            if (str != nullptr && s.str != nullptr) {
                return ((str == s.str) && (len == s.len)) ||
                       (strncmp(str, s.str, std::min(len, s.len)) == 0);
            }
            return str == s.str;
        }

        bool operator!=(const zcstr& s) const {
            return !(*this == s);
        }

        ~zcstr() {
            if (str && own) {
                __Free()(str);
            }
            str = nullptr;
            own = 0;
        }
    };

    using zcstring = zcstr<>;

    struct hasher {

        inline void hash_combine(size_t& seed, const char c) const {
            seed ^= (size_t)c + 0x9e3779b9 + (seed<<6) + (seed>>2);
        }

        inline size_t operator()(const std::string& key) const
        {
            return hash(key.c_str(), key.size());
        }

        inline size_t operator()(const buffer_t& b) const {
            return hash(b.data(), b.size());
        }

        template <typename _F>
        inline size_t operator()(const zcstr<_F>& s) const {
            zcstr<_F>& ss = (zcstr<_F>&) s;
            if (ss.hash == 0) {
                ss.hash = hash(s.cstr, s.len);
            }
            return s.hash;
        }

        inline size_t hash(const char *ptr, size_t len) const {
            std::size_t seed = 0;
            for(size_t i = 0; i < len; i++)
            {
                hash_combine(seed, ::toupper(ptr[i]));
            }
            return seed;
        }
    };

    struct network_buffer {
        using destory_t = std::function<void(void*)>;

        network_buffer&operator=(const network_buffer&) = delete;
        network_buffer(const network_buffer&) = delete;

        network_buffer(void *d, size_t size, size_t off = 0, destory_t dctor = nullptr)
            : data(d),
              offset(off),
              len(size),
              dctor(dctor)
        {}

        network_buffer()
            : network_buffer(nullptr, 0)
        {}

        network_buffer(network_buffer&& other)
            : data(other.data),
              offset(other.offset),
              len(other.len),
              dctor(other.dctor)
        {
            other.data = nullptr;
            other.offset = other.len = 0;
            other.dctor = nullptr;
        }

        network_buffer&operator=(network_buffer&& other)
        {
            data = other.data;
            offset = other.offset;
            len = other.len = 0;
            dctor = std::move(other.dctor);

            other.data = nullptr;
            other.offset = other.len = 0;
            other.dctor = nullptr;
        }

        inline size_t size() const {
            return len;
        }

        inline void* get() const {
            return ((char *)data + offset);
        }

        inline operator void*() const {
            return get();
        }

        operator bool() const {
            return data != nullptr;
        }

        void destory() {
            if (data != nullptr) {
                if (dctor) {
                    dctor(data);
                }
                else {
                    memory::free(data);
                }
                data = nullptr;
            }
        }

        inline ~network_buffer() {
            destory();
        }

    private:
        void     *data{nullptr};
        size_t   offset{0};
        size_t   len{0};
        destory_t dctor{nullptr};
    };

    struct base64 {
        static zcstr<> encode(const uint8_t *, size_t);

        static buffer_t decode(const uint8_t* in, size_t len);

        static buffer_t decode(const char* in) {
            return decode((const uint8_t *)in, strlen(in));
        }
        static buffer_t decode(strview_t& sv) {
            return std::move(decode((const uint8_t*)sv.data(), sv.size()));
        }
        static buffer_t decode(const zcstr<>& zc) {
            return std::move(decode((const uint8_t *)zc.cstr, zc.len));
        }
    };


    struct strmap_eq {
        inline bool operator()(const std::string& l, const std::string& r) const
        {
            return std::equal(l.begin(), l.end(), r.begin(), r.end());
        }
    };

    struct zcstrmap_eq {
        template <typename _F>
        inline bool operator()(const zcstr<_F>& l, const zcstr<_F>& r) const
        {
            return l == r;
        }
    };

    struct zcstrmap_case_eq {
        template <typename _F>
        inline bool operator()(const zcstr<_F>& l, const zcstr<_F>& r) const
        {
            if (l.str != nullptr) {
                return ((l.str == r.str) && (l.len == r.len)) ||
                       (strncmp(l.str, r.str, std::min(l.len, r.len)) == 0);
            }
            return l.str == r.str;
        }
    };

    template <typename _Free>
    inline buffer_t& buffer_t::operator<<(zcstr<_Free> &s) {
        append(s.str, s.len);
        return *this;
    }

    template <typename _Free>
    inline buffer_t& buffer_t::operator<<(const zcstr<_Free> &s) {
        append(s.str, s.len);
        return *this;
    }

    /**
     * The asynchronous type encapsulates libmill's channel. It can be
     * used to wait for async operations to complete or to exchange data
     * between co routines
     * @tparam __R the result type
     * @tparam __N the channel buffer specifies how much data can be buffered
     * into the underlying channel without the reader reading them.
     */
    template <typename __R, int __N = 0>
    struct async_t {
        /**
         * Creates an asynchronous channel
         * @tparam __Args variadic for terminal value of the
         * channel
         * @param args these arguments are passed to the constructor
         * of the result type to create a value that the channel will
         * listen to an use as a termination command
         */
        template<typename... __Args>
        async_t(__Args... args)
                : ch(chmake(__R, __N)),
                  term(args...)
        {}

        /**
         * Creates an empty channel, basically a null and
         * not useful channel
         */
        async_t(__Void&)
            : ch(nullptr)
        {}

        /**
         * move constructor for an async, these must never be copied
         * @param as the async to move
         */
        async_t(async_t &&as)
            : ch(as.ch),
              term(as.term),
              waitn(as.waitn),
              ddline(as.ddline)
        {
            as.ch = nullptr;
        }

        /**
         * moves one async to another
         * @param async the async to move
         * @return a copy of  the moved async
         */
        async_t& operator=(async_t&& async) {
            term  = async.term;
            ch    = async.ch;
            waitn = async.waitn;
            ddline = async.ddline;
            async.ch = nullptr;
        }

        /**
         * write a value to the channel
         * @param res the value to write to the channel
         * @return the async being written to
         */
        async_t &operator+=(const __R res) {
            if (ch != nullptr)
                chs(ch, __R, res);
            return *this;
        };

        /**
         * write a value to the channel
         * @param res the value to write to the channel
         * @return the async being written to
         */
        async_t &operator<<(const __R res) {
            ;
            if (ch != nullptr)
                chs(ch, __R, res);
            return *this;
        }

        /**
         * notify the waiter/channel receive that sending is completed
         */
        void operator!() {
            chdone(ch, __R, term);
        }

        /**
         * when called before the wait command, it will redirect the channel
         * to receive n entries. The command is only ever useful when followed
         * by a wait command
         * @param n the number of time the channel will wait
         * @return self
         */
        async_t &operator()(int n) {
            // force channel to wait for n calls
            waitn = n <= 0 ? -1 : n;
            return *this;
        }

        /**
         * wait command. this command takes a function (or Void for simply just waiting)
         * that will be executed when data is available on the channel. If prefixed by
         * by the ()(int n) command.
         * @tparam _F the type of function to invoke when there is data on the channel
         * @param f function to invoke when there is data on the channel
         * @return true when n entries have been received or the sender sent a termination
         * command, otherwise false on timeout
         *
         * @code
         * async_t<long> async(-1L);
         * ...
         * ...
         * ...
         * // wait 5 seconds for 5 writes onto the channel
         * // if 5 seconds elapses before these values can be written
         * // the wait will timeout
         * bool status = async[5000](5) | Void;
         * @endcode
         */
        template<typename _F>
        bool operator|(_F f) {
            int64_t  dd = ddline;
            ddline = -1;
            // cache and clear the waitn flag
            int rxd = waitn;
            waitn = -1;

            if (ch == nullptr) {
                return false;
            }

            while (ch && (rxd < 0 || rxd > 0)) {
                __R res;
                if (!chrx(dd, res)) {
                    return false;
                }

                if (res == term) {
                    return false;
                }

                f(true, res);
                rxd--;
            }

            return rxd == 0;
        }

        /**
         * reads one entry from the channel
         * @param res a reference to hold the read value
         * @return true if value was successfully read and is not
         * a terminal.
         *
         * @code
         * async_t<int> async(-1);
         * int val;
         * if (!(async>>val)) {
         *      // handler error
         * }
         * @endcode
         */
        bool operator>>(__R &res) {
            waitn = -1;
            int64_t  dd = ddline;
            ddline = -1;
            if (ch) {
                bool rc = chrx(dd, res);
                return (rc && res != term);
            }
            return false;
        }

        /**
         * reads one entry from the channel
         * @return the value read from the channel, which can be a terminal
         * and should be tested against the terminal
         *
         * @code
         * async_t<int> async(-1);
         * ...
         * ...
         * ...
         * int a = async();
         * if (a == async.TERM) {
         *      // handle termination
         * }
         * @endcode
         */
        __R operator()() {
            __R r;
            (*this) >> r;
            return r;
        }

        /**
         * set a timeout on the channel
         * @param timeout the timeout the next read should
         * wait for data on the channel before giving up
         * @return self
         */
        async_t &operator[](int64_t timeout) {
            if (this->ddline <= 0 && timeout > 0)
                this->ddline = mnow() + timeout;
            return *this;
        }

        /**
         * the destructor will close and destroy the underlying channel
         */
        ~async_t() {
            if (ch) {
                chclose(ch);
                ch = nullptr;
            }
        }

        /**
         * casts the channel to a bool. useful for checking the
         * validity of a channel
         * @return
         */
        operator bool() const {
            return ch != nullptr;
        }

    private:

        inline bool chrx(int64_t dd, __R& res) {
            bool rc = true;
            if (dd <= 0) {
                res = chr(ch, __R);
            }
            else {
                choose {
                        chin(ch, __R, tmp) :
                            res = tmp;
                        deadline(dd):
                            res = term;
                            rc  = false;
                    chend
                }
            }
            return rc;
        }
        chan ch;
        __R term;
        int waitn{-1};
        int64_t ddline{-1};
    };

    struct file_t {
        file_t(mfile);
        file_t(const char *, int, mode_t);
        virtual size_t write(const void*, size_t, int64_t);
        virtual bool   read(void*, size_t&, int64_t);
        virtual off_t  seek(off_t);
        virtual off_t  tell();
        virtual bool   eof();
        virtual void   flush(int64_t);
        virtual void   close();
        virtual bool   valid() {
            return fd != nullptr;
        }

        bool operator==(const file_t& other) {
            return (this == &other)  ||
                   ( fd == other.fd) ||
                   (fd != nullptr && other.fd != nullptr);
        }

        bool operator!=(const file_t& other) {
            return !(*this == other);
        }

        file_t& operator<<(strview_t& sv) {
            size_t nwr = write(sv.data(), sv.size(), -1);
            if (nwr != sv.size()) {
                throw std::runtime_error("writing failed to file failed");
            }
            return *this;
        }

        template <typename __F>
        file_t& operator<<(zcstr<__F>& str) {
            size_t nwr = write(str.str, str.len, -1);
            if (nwr != str.len) {
                throw std::runtime_error("writing failed to file failed");
            }
            return *this;
        }

        file_t& operator<<(buffer_t& b) {
            size_t nwr = write(b.data(), b.size(), -1);
            if (nwr != b.size()) {
                throw std::runtime_error("writing failed to file failed");
            }
            return *this;
        }

        virtual ~file_t();

    protected:
        mfile           fd;
    };

    namespace utils {

        int64_t strtonum(const zcstring& str, int base, long long min, long long max);

        const std::vector<char*> strsplit(zcstr<>&, const char *delim);

        zcstr<> strstrip(zcstr<>& str, char strip);

        void *memfind(void *src, size_t slen, const void *needle, size_t len);

        template<typename _T>
        inline _T to_number(const zcstring& str) {
            return (_T)utils::strtonum(str, 10, 0, UINT64_MAX);
        }

        template <typename _T>
        inline _T read(void *buf) {
            _T t;
            memcpy(&t, buf, sizeof(_T));
            return t;
        }

        template <typename _T>
        inline void write(void *buf, _T v) {
            memcpy(buf, &v, sizeof(_T));
        }

        zcstr<> md5Hash(const uint8_t*, size_t);

        static inline zcstr<> md5Hash(const char *str) {
            return std::move(md5Hash((const uint8_t *)str, strlen(str)));
        }

        static inline zcstr<> md5Hash(zcstr<>& zc) {
            return std::move(md5Hash((const uint8_t *) zc.cstr, zc.len));
        }

        static inline zcstr<> md5Hash(buffer_t& b) {
            return std::move(md5Hash((const uint8_t *) b.data(), b.size()));
        }

        template<typename __C, typename... __Opts>
        inline void apply_config(__C& obj, __Opts&... opts) {
            auto options = iod::D(opts...);
            if (options.size()) {
                iod::foreach(options) |
                [&] (auto& m) {
                    m.symbol().member_access(obj) = m.value();
                };
            }
        }

        template<typename __C>
        inline void apply_config(__C& /* unsused parameter*/) {
        }
    }

    template <typename __T>
    using zcstr_map_t = std::unordered_map<zcstring, __T, hasher, zcstrmap_case_eq>;
    template <typename _T>
    using hmap_t = std::unordered_map<std::string, _T, hasher, strmap_eq>;
}

namespace iod {
    // Decode \o from a json string \b.
    template<typename ...T>
    inline void json_decode(sio<T...> &o, const suil::buffer_t& b) {
        iod::stringview str(b.data(), b.size());
        json_decode(o, str);
    }

    // Decode \o from a json string \zc_str.
    template<typename ...T>
    inline void json_decode(sio<T...> &o, const suil::zcstring zc_str) {
        iod::stringview str(zc_str.str, zc_str.len);
        json_decode(o, str);
    }
}

#define var(v) s::_##v
#define sym(v) var(v)
#define opt(o, v) var(o) = v
#define on(ev) s::_on_##ev

#define sizeofcstr(ch)   (sizeof(ch)-1)
#endif //SUIL_SYS_HPP
