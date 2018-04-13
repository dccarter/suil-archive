//
// Created by dc on 31/05/17.
//

#ifndef SUIL_SYS_HPP
#define SUIL_SYS_HPP

#include <string.h>
#include <stdint.h>
#include <time.h>
#include <endian.h>
#include <fcntl.h>
#include <libgen.h>
#include <uuid/uuid.h>
#include <unordered_map>
#include <sys/param.h>

#include <set>
#include <regex>

#include <boost/utility/string_view.hpp>
#include <iod/json.hh>
#include <iod/parse_command_line.hh>
#include <libmill/libmill.h>

#include <suil/mem.hpp>
#include <suil/log.hpp>
#include <netinet/in.h>

#define errno_s strerror(errno)

namespace suil {

    typedef uint8_t spid_t;
    extern const spid_t& spid;

    struct zcstring;
    template <size_t N>
    struct Blob;

#define Ego (*this)

    namespace __internal {
        auto remove_members_with_attribute = [](const auto& o, const auto& a)
        {
            typedef std::decay_t<decltype(a)> A;
            return iod::foreach2(o) | [&] (auto& m) {
                typedef typename std::decay_t<decltype(m)>::attributes_type attrs;
                return iod::static_if<!iod::has_symbol<attrs,A>::value>(
                        [&](){ return m; },
                        [&](){}
                );
            };
        };

        auto extract_members_with_attribute = [](const auto& o, const auto& a)
        {
            typedef std::decay_t<decltype(a)> A;
            return iod::foreach2(o) | [&] (auto& m) {
                typedef typename std::decay_t<decltype(m)>::attributes_type attrs;
                return iod::static_if<iod::has_symbol<attrs,A>::value>(
                        [&](){ return m; },
                        [&](){}
                );
            };
        };

        template<typename T>
        using remove_unwired =
        decltype(remove_members_with_attribute(std::declval<T>(), sym(unwire)));

        void chwdir(const std::string& to);
        void daemonize(const std::string& wdir);
    }

    bool load(bool si = true);
    template <typename... __A>
    void init(__A... args) {
        static bool initialized{false};
        auto opts = iod::D(args...);
        bool showinfo = opts.get(var(printinfo), true);
        suil::load(showinfo);
        if (!initialized) {

            std::string wdir = opts.get(var(wdir), "");

            if (opts.get(var(background), false)) {
                // put the process to background
                __internal::daemonize(wdir);
            } else if (!wdir.empty()) {
                // change directory in current process
                __internal::chwdir(wdir);
            }
        }
        initialized = true;
    }

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
        prop(vtag,   std::string),
        prop(vdate,  std::string),
        prop(vtime,  std::string)
    )) Version;
    extern const Version& ver_json;

    struct Datetime {
        Datetime(time_t);
        Datetime();
        Datetime(const char *http_time);
        Datetime(const char *fmt, const char *str);
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

    struct SuilError: public std::runtime_error {
        template <typename... __A>
        inline static SuilError create(__A... args) {
            std::stringstream ss;
            if (sizeof...(__A) == 0) {
                ss << "SuilError: " << errno_s;
            } else {
                msg(ss, args...);
            }
            return SuilError(ss.str());
        }

        inline static const char* getmsg(std::exception_ptr ex) {
            try {
                if (ex) {
                    std::rethrow_exception(ex);
                }
            }
            catch (const std::exception& e) {
                return e.what();
            }
            return "";
        }

    private:
        SuilError(std::string str)
            : std::runtime_error(str)
        {}

        inline static void msg(std::stringstream&) {
        }

        template <typename __A>
        inline static void msg(std::stringstream& ss, __A& a) {
            ss << a;
        }

        inline static void msg(std::stringstream& ss, suil::zcstring& a);

        template <typename __A, typename... __O>
        inline static void msg(std::stringstream& ss, __A& a, __O&... args) {
            msg(ss, a);
            msg(ss, args...);
        }
    };

    struct Wire;

    struct Data {
    private:
        union {
            uint8_t       *_data;
            const uint8_t *_cdata;
        };
        struct {
#ifdef BIG_ENDIAN
            uint32_t _own:  8;
            uint32_t _size: 26;
#else
            uint32_t _size: 26;
            uint32_t _own:  8;
#endif
        };
    public:

        Data()
            : Data(nullptr, 0)
        {}

        Data(uint8_t *data, size_t size, bool own = true)
            : _cdata(data),
              _size((uint32_t)(size)),
              _own(own?1:0)
        {}

        Data(const Data& d)
                : Data()
        {
            if (d._size) {
                Ego._data = (uint8_t *) memory::alloc(d._size);
                Ego._size = d._size;
                memcpy(Ego._data, d._data, d._size);
                Ego._own  = 1;
            }
        }

        Data& operator=(const Data& d) {
            Ego.clear();
            if (d._size) {
                Ego._data = (uint8_t *) memory::alloc(d._size);
                Ego._size = d._size;
                memcpy(Ego._data, d._data, d._size);
                Ego._own  = 1;
            }

            return Ego;
        }

        Data(Data&& d)
                : Data(d._data, d._size, (d._own!=0))
        {
            d._data = nullptr;
            d._size = 0;
            d._own  = 0;
        }

        Data& operator=(Data&& d) {
            Ego.clear();
            Ego._data = d._data;
            Ego._size = d._size;
            Ego._own  = d._own;
            d._data = nullptr;
            d._size = 0;
            d._own  = 0;
        }

        inline Data peek() {
            Data d{Ego._data, Ego._size, false};
            return std::move(d);
        }

        inline bool empty() {
            return Ego._size == 0;
        }

        inline size_t size() const {
            return Ego._size;
        }

        inline uint8_t* data() {
            return Ego._data;
        }

        inline const uint8_t* cdata() const {
            return Ego._cdata;
        }

        inline bool operator==(const Data& other) {
            return (Ego._size == other._size) &&
                   Ego._size &&
                   (memcmp(Ego._data, other._data, Ego._size) == 0);
        }

        inline bool operator!=(const Data& other) {

        }

        void clear() {
            if (Ego._own && Ego._data) {
                memory::free(Ego._data);
                Ego._data = nullptr;
                Ego._size = 0;
                Ego._own  = 0;
            }
        }

        ~Data() {
            Ego.clear();
        }

        void in(Wire& w);
        void out(Wire& w) const;

    } __attribute__((aligned(1)));

    template <typename Type>
    struct Wrapper {
        Wrapper(Type& w)
            : wrapped(std::move(w))
        {}

        Wrapper(Type&& w)
            : wrapped(std::move(w))
        {}

        Wrapper()
        {}

        operator Type&() {
            return wrapped;
        }
        Type&operator()() {
            return wrapped;
        }
        Type&self() { return wrapped; }
    private:
        Type wrapped{};
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
    using strview = boost::string_view;

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
         * Duplicate \param n_chars the given string into a newly allocated zbuffer.
         * @param src the string (or character zbuffer) to duplicate
         * @param n_chars the number of characters to copy
         * @return a zbuffer with \param n_chars copied from \param src if the copy
         * was successful, otherwise NULL will be returned
         *
         * @note the returned zbuffer should be freed using @see memory::free
         */
        char *strndup(const char *src, size_t n_chars);

        /**
         * Duplicate the given string into a new string
         * @param src the string to copy to a new zbuffer
         * @return a copy of the given string in a separate zbuffer or NULL
         * if the duplication failed
         *
         * @note the returned zbuffer should be freed using @see memory::free
         */
        static inline char *strdup(const char *str) {
            return utils::strndup(str, strlen(str));
        }
    }

    struct current {
    };

    struct Wire {

        inline bool push(const uint8_t e[], size_t es) {
            return forward(e, es) == es;
        }

        inline bool pull(uint8_t e[], size_t es) {
            return reverse(e, es) == es;
        }

        template <typename __T, typename std::enable_if<std::is_arithmetic<__T>::value>::type* = nullptr>
        inline Wire& operator<<(const __T val) {
            uint64_t tmp = htole64((uint64_t) val);
            forward((uint8_t *)&tmp, sizeof(val));
            return Ego;
        };

        template <typename __T, typename std::enable_if<std::is_arithmetic<__T>::value>::type* = nullptr>
        inline Wire& operator>>(__T& val) {
            uint64_t tmp{0};
            reverse((uint8_t *)&tmp, sizeof(val));
            val = (__T) le64toh(tmp);
            return Ego;
        };

        template <typename __T, typename std::enable_if<!std::is_arithmetic<__T>::value>::type* = nullptr>
        inline Wire& operator<<(const __T& curr) {
            curr.out(Ego);
            return Ego;
        };

        template <typename __T, typename std::enable_if<!std::is_arithmetic<__T>::value>::type* = nullptr>
        inline Wire& operator>>(__T& curr) {
            curr.in(Ego);
            return Ego;
        };

        template <typename... __T>
        inline Wire& operator<<(const iod::sio<__T...>& o);

        template <typename... __T>
        inline Wire& operator>>(iod::sio<__T...>& o);

        template <typename __T>
        inline Wire& operator<<(const Wrapper<__T>& o) {
            // serialize wrapped type
            Ego << o();
            return Ego;
        }

        template <typename __T>
        inline Wire& operator>>(Wrapper<__T>& o) {
            // deserialize wrapped type
            Ego >> o();
            return Ego;
        }

        template <typename __T>
        inline Wire& operator<<(const std::vector<__T>& curr);

        template <typename __T>
        inline Wire& operator>>(std::vector<__T>& curr);

        template <typename... __T>
        inline Wire& operator<<(const std::vector<iod::sio<__T...>>& curr);

        template <typename... __T>
        inline Wire& operator>>(std::vector<iod::sio<__T...>>& curr);

        inline Wire& operator<<(const char* str);

        inline Wire& operator<<(const std::string& str);

        inline Wire& operator>>(std::string& str);

        inline Wire& operator()(bool always) {
            Ego.always = always;
            return Ego;
        }

        virtual uint8_t *rd() = 0;
        virtual bool    move(size_t size) = 0;

    protected:
        virtual size_t forward(const uint8_t e[], size_t es) = 0;
        virtual size_t reverse(uint8_t e[], size_t es) = 0;
        bool           always{false};
    };

    /**
     * A dynamic auto-grow buffer which can be used for
     * for various memory manipulation
     */
    struct zbuffer {

        struct fmter {
            fmter(size_t reqsz)
                : reqsz{reqsz}
            {}

            virtual size_t fmt(char *raw) { return 0; };

        private:
            friend struct zbuffer;
            size_t reqsz{0};
        };

        zbuffer(size_t init_size);
        zbuffer()
            : zbuffer(0)
        {}

        zbuffer(zbuffer&&);

        zbuffer&operator=(zbuffer&& other);
        ~zbuffer();

        void append(const void *data, size_t);

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

        inline void append(const zbuffer& other) {
            append(other.data(), (uint32_t) other.size());
        }

        template<typename __T, typename std::enable_if<std::is_arithmetic<__T>::value>::type* = nullptr>
        inline size_t hex(__T  v, bool filled = false) {
            size_t tmp = this->size();
            char fmt[10];
            if (filled)
                sprintf(fmt, "%%0%dx",(int)sizeof(__T)*2);
            else
                sprintf(fmt, "%%x");
            appendnf(8, fmt, v);
            return this->size() - tmp;
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

        operator strview();

        operator bool() {
            return (offset_ != 0) &&
                   (data_ != nullptr) &&
                   (size_ != 0);
        }

        inline size_t size() const {
            return offset_;
        }

        inline bool empty() const {
            return offset_ == 0;
        }

        char *data() const {
            if (data_)
                return (char *) data_;
            return nullptr;
        }

        size_t capacity() const {
            return size_-offset_;
        }

        operator const void*() {
            return data_;
        }

        zbuffer&operator+=(const char *str) {
            append(str);
            return *this;
        }

        zbuffer&operator+=(const std::string& str) {
            append(str.c_str());
            return *this;
        }

        zbuffer&operator+=(const strview& sv) {
            append(sv.data(), sv.size());
            return *this;
        }

        zbuffer& operator<<(const bool u) {
            appendf("%d", u);
            return *this;
        }

        zbuffer& operator<<(unsigned char u) {
            appendf("%hhu", u);
            return *this;
        }

        zbuffer& operator<<(unsigned short u) {
            appendf("%hu", u);
            return *this;
        }

        zbuffer& operator<<(unsigned int u) {
            appendf("%u", u);
            return *this;
        }

        zbuffer& operator<<(unsigned long ul) {
            appendf("%lu", ul);
            return *this;
        }

        zbuffer& operator<<(unsigned long long ull) {
            appendf("%llu", ull);
            return *this;
        }

        zbuffer& operator<<(char i) {
            appendf("%c", i);
            return *this;
        }

        zbuffer& operator<<(short i) {
            appendf("%hd", i);
            return *this;
        }

        zbuffer& operator<<(int i) {
            appendf("%d", i);
            return *this;
        }

        zbuffer& operator<<(long l) {
            appendf("%ld", l);
            return *this;
        }

        zbuffer& operator<<(long long ll) {
            appendf("%lld", ll);
            return *this;
        }

        zbuffer& operator<<(double d) {
            appendf("%f", d);
            return *this;
        }

        zbuffer& operator<<(float d) {
            appendf("%f", d);
            return *this;
        }

        zbuffer& operator<<(const char *str) {
            append(str);
            return *this;
        }

        zbuffer& operator<<(char *str) {
            append(str);
            return *this;
        }

        template <typename __T, typename std::enable_if<std::is_base_of<suil::zbuffer::fmter, __T>::value>::type* = nullptr>
        zbuffer& operator<<(__T fmt) {
            reserve(fmt.reqsz);
            size_t tmp = fmt.fmt(&data()[offset_]);
            if (tmp > fmt.reqsz) tmp = fmt.reqsz;
            offset_ += tmp;
            return *this;
        }

        zbuffer& operator<<(const zcstring& s);

        zbuffer& operator<<(zcstring& s);

        template <size_t N>
        zbuffer& operator<<(const Blob<N>& s);

        template <size_t N>
        zbuffer& operator<<(Blob<N>& s);

        zbuffer& operator<<(const std::string &str) {
            append(str.c_str());
            return *this;
        }

        zbuffer& operator<<(const strview& sv) {
            append(sv.data(), sv.size());
            return *this;
        }

        zbuffer&operator<<(const zbuffer& other) {
            append(other.data_, other.offset_);
            return *this;
        }

        template <typename __T, typename std::enable_if<!std::is_base_of<suil::zbuffer::fmter, __T>::value>::type* = nullptr>
        zbuffer&operator<<(const __T& data) {
            data.serialize(*this);
            return *this;
        }

        uint8_t& operator[](size_t index) {
            if (index <= offset_) {
                return data_[index];
            }
            throw std::runtime_error("index out of bounds");
        }

        void in(Wire& w);
        void out(Wire& w) const;

    private:
        void grow(uint32_t);
        uint8_t         *data_{nullptr};
        uint32_t        size_{0};
        uint32_t        offset_{0};
    };
    // each zbuffer overhead is 16 bytes
    static_assert(sizeof(zbuffer) <= 16);

    template <typename __T>
    struct __fmtnum : zbuffer::fmter {
        __fmtnum(const char* fs, __T t)
            : zbuffer::fmter(32),
              val(t),
              fs(fs)
        {}

        virtual size_t fmt(char *raw) {
            return (size_t) snprintf(raw, 32, fs, val);
        }

        __T val;
        const char *fs{"%x"};
    };

    template <typename __T, typename std::enable_if<std::is_arithmetic<__T>::value>::type * = nullptr>
    static inline __fmtnum<__T> fmtnum(const char* fs, __T t) {
        return __fmtnum<__T>(fs, t);
    };

    struct fmtbool : zbuffer::fmter {
        fmtbool(bool b)
            : zbuffer::fmter(6),
              val(b)
        {}

        virtual size_t fmt(char *raw) {
            return (size_t) snprintf(raw, 6, "%s", (val? "True" : "False"));
        }
        bool val{false};
    };

    struct zcstring : iod::jsonvalue {

        zcstring();

        explicit zcstring(char c, size_t n);

        zcstring(const char *str);

        explicit zcstring(const strview str, bool own = 0);

        explicit zcstring(const std::string& str, bool own = 0);

        explicit zcstring(const char *str, size_t len, bool own = true);

        zcstring(zbuffer& b, bool own = true);

        zcstring(zcstring&& s) noexcept;

        zcstring& operator=(zcstring&& s) noexcept;

        zcstring(const zcstring& s);

        zcstring& operator=(const zcstring& s);

        zcstring dup() const;

        zcstring peek() const;

        void toupper();

        void tolower();

        bool empty() const;

        inline operator bool() const {
            return !empty();
        }

        operator strview() const {
            return strview(_str, _len);
        }

        bool operator==(const zcstring& s) const;

        bool operator!=(const zcstring& s) const {
            return !(Ego == s);
        }

        size_t find(const char ch) const;

        zcstring substr(size_t from, size_t nchars = 0) const {
            ssize_t fits = (ssize_t) (Ego._len - from);
            if (fits >= (ssize_t) nchars) {
                return zcstring{&Ego._cstr[from], nchars, false}.dup();
            }
            return zcstring{};
        }

        inline int compare(const char* s) const {
            return strncmp(_str, s, MIN(strlen(s), _len));
        }

        inline int compare(const zcstring& s) const {
            return strncmp(_str, s._str, MIN(s._len, _len));
        }

        inline bool operator>(const zcstring& s) const {
            return Ego.compare(s) > 0;
        }

        inline bool operator>=(const zcstring& s) const {
            return Ego.compare(s) >= 0;
        }

        inline bool operator<(const zcstring& s) const {
            return Ego.compare(s) < 0;
        }

        inline bool operator<=(const zcstring& s) const {
            return Ego.compare(s) <= 0;
        }

        inline const char*operator~() const {
            return  Ego.c_str();
        }

        inline const char* operator()() const {
            return Ego.c_str();
        }

        inline const char* data() const {
            return Ego._cstr;
        }

        inline char* data() {
            return Ego._str;
        }

        inline size_t size() const {
            return Ego._len;
        }

        size_t hash() const;

        template <typename T>
        zcstring& operator+=(const T&);

        const char* c_str(const char* nil = "") const;

        template <typename __T>
        explicit inline operator __T() const;

        template <typename S>
        void encjv(S& ss) const {
            if (!empty()) {
                suil::strview tmp(_cstr, _len);
                ss << '"' << tmp << '"';
            }
            else {
                ss << "\"\"";
            }
        }

        static void decjv(iod::jdecit& it, zcstring& out);

        void in(Wire& w);
        void out(Wire& w) const;

        ~zcstring();

#ifndef SUIL_TESTING
    private:
#endif
        friend struct hasher;
        union {
            char *_str;
            const char *_cstr;
        };

        uint32_t _len{0};
        uint8_t  _own{0};
        size_t   _hash{0};
    };

    inline void SuilError::msg(std::stringstream& ss, suil::zcstring& a) {
        ss.write(a.data(), a.size());
    }

    struct hasher {

        inline size_t operator()(const std::string& key) const {
            return hash(key.c_str(), key.size());
        }

        inline size_t operator()(const zbuffer& b) const {
            return hash(b.data(), b.size());
        }

        size_t operator()(const zcstring& s) const;

        size_t hash(const char *ptr, size_t len) const;
    };

    struct NetworkBuffer {
        using destory_t = std::function<void(void*)>;

        NetworkBuffer&operator=(const NetworkBuffer&) = delete;
        NetworkBuffer(const NetworkBuffer&) = delete;

        NetworkBuffer(void *d, size_t size, size_t off = 0, destory_t dctor = nullptr)
            : data(d),
              offset(off),
              len(size),
              dctor(dctor)
        {}

        NetworkBuffer()
            : NetworkBuffer(nullptr, 0)
        {}

        NetworkBuffer(NetworkBuffer&& other)
            : data(other.data),
              offset(other.offset),
              len(other.size()),
              dctor(other.dctor)
        {
            other.data  = nullptr;
            other.offset = 0;
            other.dctor  = nullptr;
        }

        NetworkBuffer&operator=(NetworkBuffer&& other)
        {
            data = other.data;
            offset = other.offset;
            len = other.size();
            dctor = std::move(other.dctor);

            other.data  = nullptr;
            other.offset = 0;
            other.dctor  = nullptr;
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

        inline ~NetworkBuffer() {
            destory();
        }

    private:
        void     *data{nullptr};
        size_t   offset{0};
        size_t   len{0};
        destory_t dctor{nullptr};
    };

    namespace base64 {
        zcstring encode(const uint8_t *, size_t);

        static zcstring encode(const zcstring& str) {
            return encode((const uint8_t *) str.data(), str.size());
        }

        static zcstring encode(const std::string& str) {
            return encode((const uint8_t *) str.data(), str.size());
        }

        zcstring decode(const uint8_t* in, size_t len);

        static zcstring decode(const char* in) {
            return decode((const uint8_t *)in, strlen(in));
        }

        static zcstring decode(strview& sv) {
            return std::move(decode((const uint8_t*)sv.data(), sv.size()));
        }
        static zcstring decode(const zcstring& zc) {
            return std::move(decode((const uint8_t *)zc.data(), zc.size()));
        }
    };


    struct strmap_eq {
        inline bool operator()(const std::string& l, const std::string& r) const
        {
            return std::equal(l.begin(), l.end(), r.begin(), r.end());
        }
    };

    struct zcstrmap_eq {
        inline bool operator()(const zcstring& l, const zcstring& r) const
        {
            return l == r;
        }
    };

    struct zcstring_cmp {
        inline bool operator()(const zcstring& l, const zcstring& r) const
        {
            return l < r;
        }
    };

    struct zcstrmap_case_eq {
        inline bool operator()(const zcstring& l, const zcstring& r) const
        {
            if (l.data() != nullptr) {
                return ((l.data() == r.data()) && (l.size() == r.size())) ||
                       (strncasecmp(l.data(), r.data(), std::min(l.size(), r.size())) == 0);
            }
            return l.data() == r.data();
        }
    };

    inline zbuffer& zbuffer::operator<<(zcstring &s) {
        append(s.data(), s.size());
        return *this;
    }

    inline zbuffer& zbuffer::operator<<(const zcstring& s) {
        append(s.data(), s.size());
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
    struct Async {
        /**
         * Creates an asynchronous channel
         * @tparam __Args variadic for terminal value of the
         * channel
         * @param args these arguments are passed to the constructor
         * of the result type to create a value that the channel will
         * listen to an use as a termination command
         */
        template<typename... __Args>
        Async(__Args... args)
                : ch(chmake(__R, __N)),
                  term(args...)
        {}

        /**
         * Creates an empty channel, basically a null and
         * not useful channel
         */
        Async(__Void&)
            : ch(nullptr)
        {}

        /**
         * move constructor for an async, these must never be copied
         * @param as the async to move
         */
        Async(Async &&as)
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
        Async& operator=(Async&& async) {
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
        Async &operator+=(const __R res) {
            if (ch != nullptr)
                chs(ch, __R, res);
            return *this;
        };

        /**
         * write a value to the channel
         * @param res the value to write to the channel
         * @return the async being written to
         */
        Async &operator<<(const __R res) {
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
        Async &operator()(int n) {
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
        Async &operator[](int64_t timeout) {
            if (this->ddline <= 0 && timeout > 0)
                this->ddline = mnow() + timeout;
            return *this;
        }

        /**
         * the destructor will close and destroy the underlying channel
         */
        ~Async() {
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

    namespace __internal {
        struct defer_ctx {
            defer_ctx(std::function<void()> fn)
                    : dctor(fn) {}

            defer_ctx(const defer_ctx &) = delete;

            defer_ctx &operator=(const defer_ctx &) = delete;

            defer_ctx(defer_ctx &&) = delete;

            defer_ctx &operator=(defer_ctx &&) = delete;

            ~defer_ctx() {
                if (dctor && !once) {
                    once = true;
                    dctor();
                }
            }

        private:
            std::function<void()> dctor;
            bool once{false};
        };

        template <typename __R>
        struct scoped_res {
            scoped_res(__R& res)
                : res(res)
            {}
            scoped_res(const scoped_res&) = delete;
            scoped_res(scoped_res&&) = delete;
            scoped_res&operator=(scoped_res&) = delete;
            scoped_res&operator=(const scoped_res&) = delete;
            ~scoped_res() {
                res.close();
            }
            __R& res;
        };
    }

// function call to end of block
#define defer(n, x) suil::__internal::defer_ctx __##n {[&]() { x ; } }
#define scoped(n, x) auto& n = x ; suil::__internal::scoped_res<decltype( n )> _##n { n }

    struct File {
        File(mfile);
        File(const char *, int, mode_t);
        explicit File(int fd, bool own = false);

        File(File&) = delete;
        File&operator=(File&) = delete;

        File(File&& f)
            : fd(f.fd)
        {
            f.fd = nullptr;
        }

        File&& operator=(File&& f) {
            fd = f.fd;
            f.fd = nullptr;
        }

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

        bool operator==(const File& other) {
            return (this == &other)  ||
                   ( fd == other.fd) ||
                   (fd != nullptr && other.fd != nullptr);
        }

        bool operator!=(const File& other) {
            return !(*this == other);
        }

        File& operator<<(strview& sv) {
            size_t nwr = write(sv.data(), sv.size(), -1);
            if (nwr != sv.size()) {
                throw std::runtime_error("writing failed to file failed");
            }
            return *this;
        }

        File& operator<<(zcstring& str) {
            size_t nwr = write(str.data(), str.size(), -1);
            if (nwr != str.size()) {
                throw std::runtime_error("writing failed to file failed");
            }
            return *this;
        }

        File& operator<<(zbuffer& b) {
            size_t nwr = write(b.data(), b.size(), -1);
            if (nwr != b.size()) {
                throw std::runtime_error("writing failed to file failed");
            }
            return *this;
        }

        virtual ~File();

    protected:
        mfile           fd;
    };

    struct FileLogger {

        FileLogger(const std::string dir, const std::string prefix);
        FileLogger()
            : dst(nullptr)
        {}

        virtual void log(const char *, size_t, log::level);

        inline void close() {
            dst.close();
        }

        void open(const std::string& str, const std::string& prefix);

        inline ~FileLogger() {
            close();
        }

    private:
        File dst;
    };

    struct Syslog {

        Syslog(const char *name = "suil");

        virtual void log(const char *, size_t, log::level);

        void close();

        inline ~Syslog() {
            Ego.close();
        }
    };

    template <typename __T>
    using zmap = std::unordered_map<zcstring, __T, hasher, zcstrmap_case_eq>;
    template <typename _T>
    using hmap = std::unordered_map<std::string, _T, hasher, strmap_eq>;

    template <typename Cmp = zcstring_cmp>
    using Set = std::set<suil::zcstring, Cmp>;

    namespace utils {
        template <typename Map, typename V>
        void mapvalues(const Map& mp, std::vector<V>& out, zcstring prefix = nullptr) {
            std::vector<zcstring> keys;
            for (auto& kv: mp) {
                bool cap{true};
                if (!prefix.empty()) {
                    cap = (prefix.size() <= kv.first.len) &&
                            memcmp(prefix.data(), kv.first.cstr, prefix.size());
                }
                if (cap) {
                    keys.emplace_back(&kv.second);
                }
            }
        }

        template <typename Map>
        void mapkeys(const Map& mp, Set<>& out, zcstring prefix = nullptr) {
            for (auto& kv: mp) {
                bool cap{true};
                if (!prefix.empty()) {
                    cap = (prefix.size() <= kv.first.size()) &&
                          memcmp(prefix.data(), kv.first.data(), prefix.size());
                }
                if (cap) {
                    out.insert(kv.first.peek());
                }
            }
        }

        namespace __internal {

            template <typename __T>
            inline void catstr(zbuffer &out, const __T& a) {
                out << a;
            }

            template<typename __T1, typename __T2, typename... __A>
            inline void catstr(zbuffer &out, const __T1& a, const __T2& b, __A&... args) {
                catstr(out, a);
                catstr(out, b, args...);
            }

            inline bool strmatchany(const char *orig, const char *o) {
                return strcmp(orig, o) == 0;
            }

            template <typename... __A>
            inline bool strmatchany(const char *l, const char *r, __A&... args) {
                return strmatchany(l, r) || strmatchany(l, args...);
            }

            template <typename __T, typename... __A>
            inline bool matchany(const __T& l, const __T& r) {
                return l == r;
            }

            template <typename __T, typename... __A>
            inline bool matchany(const __T& l, const __T& r, __A... args) {
                return matchany(l, r) || matchany(l, args...);
            }
        }

        template <typename __T1, typename __T2, typename... __A>
        zcstring catstr(const __T1& a, const __T2& b, __A... args) {
            zbuffer out(32);
            __internal::catstr(out, a, b, args...);
            return zcstring(out);
        }

        template <typename... __A>
        bool strmatchany(const char* l, const char* r, __A... args) {
            return __internal::strmatchany(l, r, args...);
        }

        template <typename __T, typename... __A>
        inline bool matchany(const __T l, const __T r, __A... args) {
            return __internal::matchany(l, r, args...);
        };

        inline unsigned char* uuid(uuid_t id) {
            if (uuid_generate_time_safe(id)) {
                nullptr;
            }

            return id;
        }

        inline bool parseuuid(const zcstring& str, uuid_t& out) {
            if (!str.empty() && uuid_parse(str.data(), out)) {
                return true;
            }
            return false;
        }

        inline bool uuidvalid(const zcstring& str) {
            uuid_t tmp;
            return parseuuid(str, tmp);
        }

        zcstring uuidstr(uuid_t uuid = nullptr);

        namespace fs {

            inline zcstring realpath(const char *path) {
                char base[PATH_MAX];
                if (::realpath(path, base) == nullptr) {
                    if (errno != EACCES && errno != ENOENT)
                        return zcstring();
                }

                return std::move(zcstring{base}.dup());
            }

            inline size_t size(const char *path) {
                struct stat st;
                if (stat(path, &st) == 0) {
                    return (size_t) st.st_size;
                }
                throw SuilError::create("file '", path, "' does not exist");
            }

            inline void touch(const char *path, mode_t mode=0777) {
                if (::open(path, O_CREAT|O_TRUNC|O_WRONLY, mode) < 0) {
                    throw SuilError::create("touching file '", path, "' failed: ", errno_s);
                }
            }

            inline bool  exists(const char *path) {
                return access(path, F_OK) != -1;
            }

            inline bool isdir(const char *path) {
                struct stat st{};
                return (stat(path, &st) == 0) && (S_ISDIR(st.st_mode));
            }

            void mkdir(const char *path, bool recursive = false, mode_t mode = 0777);

            inline void mkdir(const char *base, const std::vector<const char*> paths, bool recursive = false, mode_t mode = 0777) {
                for (auto& p : paths) {
                    if (p[0] == '/') {
                        mkdir(p, recursive, mode);
                    }
                    else {
                        zcstring tmp = catstr(base, "/", p);
                        mkdir(tmp.data(), recursive, mode);
                    }
                }
            }

            inline void mkdir(const std::vector<const char*> paths, bool recursive = false, mode_t mode = 0777) {
                char base[PATH_MAX];
                getcwd(base, PATH_MAX);
                mkdir(base, std::move(paths), recursive, mode);
            }

            void remove(const char *path, bool recursive = false, bool contents = false);

            inline void remove(const char *base, const std::vector<const char*> paths, bool recursive = false) {
                for (auto& p : paths) {
                    zcstring tmp = p[0] == '/'? utils::catstr(base, p) : utils::catstr(base, "/", p);
                    remove(tmp.data(), recursive);
                }
            }

            inline void remove(const std::vector<const char*>&& paths, bool recursive = false) {
                char base[PATH_MAX];
                getcwd(base, PATH_MAX);
                remove(base, std::move(paths), recursive);
            }

            void forall(const char *path, std::function<bool(const zcstring&, bool)> h, bool recursive = false, bool pod = false);

            std::vector<zcstring> ls(const char *path, bool recursive = false);

            zcstring readall(const char* path, bool async = false);

            void append(const char *path, const void *data, size_t sz, bool async = true);

            inline void append(const char *path, const zbuffer& b, bool async = true) {
                append(path, b.data(), b.size(), async);
            }

            inline void append(const char *path, const std::string& s, bool async = true) {
                append(path, s.data(), s.size(), async);
            }

            inline void append(const char *path, const strview& s, bool async = true) {
                append(path, s.data(), s.size(), async);
            }

            inline void append(const char *path, const zcstring& s, bool async = true) {
                append(path, s.data(), s.size(), async);
            }

            inline void clear(const char *path) {
                if ((::truncate(path, 0) < 0) && errno != EEXIST) {
                    throw SuilError::create("clearing file '", path, "' failed: ", errno_s);
                }
            }

            template <typename __T>
            inline void append(const char* path, const __T d, bool async = true) {
                zbuffer b(15);
                b << d;
                append(path, b, async);
            }
        }

        /**
         * @brief Converts a string to a decimal number. Floating point
         * numbers are not supported
         * @param str the string to convert to a number
         * @param base the numbering base to the string is in
         * @param min expected minimum value
         * @param max expected maximum value
         * @return a number converted from given string
         */
        int64_t strtonum(const zcstring& str, int base, long long min, long long max);

        zcstring find(zcstring& src, char what, size_t after = 0);

        const std::vector<char*> strsplit(zcstring&, const char *delim);

        zcstring strstrip(zcstring& str, char strip = ' ', bool ends = false);

        inline zcstring strtrim(zcstring& str, char what = ' ') {
            return strstrip(str, what, true);
        }

        void *memfind(void *src, size_t slen, const void *needle, size_t len);

        template<typename __T>
        typename std::enable_if<std::is_integral<__T>::value, __T>::type
        to_number(const zcstring& str) {
            __T tmp = (__T)utils::strtonum(str, 10, INT64_MIN, INT64_MAX);
            return tmp;
        }

        template<typename __T>
        typename std::enable_if<std::is_floating_point<__T>::value, __T>::type
        to_number(const zcstring& str) {
            double f;
            char *end;
            f = strtod(str.data(), &end);
            if (errno || *end != '\0')  {
                throw std::runtime_error(errno_s);
            }
            return (__T) f;
        }

        template<typename __T>
        inline typename std::enable_if<std::is_arithmetic<__T>::value, zcstring>::type
        tozcstr(__T v) {
            auto tmp = std::to_string(v);
            return zcstring(tmp.c_str(), tmp.size(), false).dup();
        }

        inline zcstring tozcstr(const zcstring& str) {
            return std::move(str.peek());
        }

        inline zcstring tozcstr(const char *str) {
            return zcstring{str}.dup();
        }

        inline zcstring tozcstr(const std::string& str) {
            return zcstring(str.c_str(), str.size(), false).dup();
        }

        template <typename __T>
        inline __T read(void *buf) {
            __T t;
            memcpy(&t, buf, sizeof(__T));
            return t;
        }

        template <typename __T>
        inline void write(void *buf, __T v) {
            memcpy(buf, &v, sizeof(__T));
        }

        template <typename __T>
        inline typename std::enable_if<std::is_arithmetic<__T>::value, void>::type
        cast(const zcstring& data, __T& to) {
            to = utils::to_number<__T>(data);
        }

        inline void cast(const zcstring& data, bool& to) {
            to = utils::to_number<int>(data) != 0;
        }

        inline void cast(const zcstring& data, const char*& to) {
            to = data();
        }


        inline void cast(const zcstring& data, std::string& to) {
            to = std::move(std::string(data.data(), data.size()));
        }

        inline void cast(const zcstring& data, zcstring& to) {
            to = std::move(zcstring(data.data(), data.size(), false));
        }

        template <typename __T>
        inline __T env(const char *name, __T def = __T{}) {
            const char *v = std::getenv(name);
            if (v != nullptr) {
                cast(zcstring(v), def);
            }
            return def;
        }

        inline const char* env(const char *name, const char *def = nullptr) {
            const char *v = std::getenv(name);
            if (v != nullptr) {
                def = v;
            }
            return def;
        }

        inline zcstring env(const char *name, zcstring def = zcstring{}) {
            const char *v = std::getenv(name);
            if (v != nullptr) {
                def = std::move(zcstring{v}.dup());
            }

            return std::move(def);
        }

        zcstring urlencode(const zcstring& str);
        static inline zcstring urlencode(const char* str) {
            zcstring tmp(str);
            return urlencode(tmp);
        }

        zcstring urldecode(const char *src, size_t len);
        inline zcstring urldecode(const zcstring& str) {
            return urldecode(str.data(), str.size());
        }

        void     randbytes(uint8_t *out, size_t size);

        zcstring randbytes(size_t size);

        size_t   hexstr(const uint8_t *, size_t, char *out, size_t len);

        zcstring hexstr(const uint8_t *, size_t);

        void bytes(const zcstring &str, uint8_t *out, size_t olen);

        zcstring shaHMAC256(zcstring &, const uint8_t *, size_t, bool b64 = false);

        static inline zcstring shaHMAC256(zcstring &secret, zcstring &msg, bool b64 = false) {
            return utils::shaHMAC256(secret, (const uint8_t *) msg.data(), msg.size(), b64);
        }

        static inline zcstring shaHMAC256(zcstring &secret, zbuffer &msg, bool b64 = false) {
            return shaHMAC256(secret, (const uint8_t *) msg.data(), msg.size(), b64);
        }

        zcstring md5(const uint8_t *, size_t);

        static inline zcstring md5(const char *str) {
            return utils::md5((const uint8_t *) str, strlen(str));
        }

        static inline zcstring md5(const zcstring &zc) {
            return utils::md5((const uint8_t *) zc.data(), zc.size());
        }

        static inline zcstring md5(zbuffer &b) {
            return utils::md5((const uint8_t *) b.data(), b.size());
        }

        zcstring sha256(const uint8_t *data, size_t len, bool b64 = false);

        static inline zcstring sha256(const zcstring &data, bool b64 = false) {
            return utils::sha256((const uint8_t *) data.data(), data.size(), b64);
        }

        static inline zcstring sha256(const zbuffer &data, bool b64 = false) {
            return utils::sha256((const uint8_t *) data.data(), data.size(), b64);
        }

        template<typename __C, typename __Opts>
        inline void apply_options(__C& o, __Opts& opts) {
            /* the target object here is also an sio*/
            if (opts.size()) {
                iod::foreach(opts) |
                [&](auto &m) {
                    /* use given metadata to to set options */
                    m.symbol().member_access(o) = m.value();
                };
            }
        }

        template<typename __C, typename... __Opts>
        inline void apply_config(__C& obj, __Opts... opts) {
            auto options = iod::D(opts...);
            utils::apply_options(obj, options);
        }

        template<typename __C>
        inline void apply_config(__C& /* unsused parameter*/) {
        }

        const char *mimetype(const zcstring filename);

        inline int clz(uint64_t num) {
            if (num) {
                return  __builtin_clz(num);
            }
            return 0;
        }

        inline int ctz(uint64_t num) {
            if (num) {
                return  __builtin_ctz(num);
            }
            return sizeof(uint64_t);
        }

        inline uint64_t np2(uint64_t num) {
            return (uint64_t)(1<<(sizeof(num)-utils::ctz(num)));
        }

        inline void closepipe(int p[2]) {
            close(p[0]);
            close(p[1]);
        }


        namespace regex {
            inline bool match(std::regex& reg, const char *data, size_t len = 0) {
                return std::regex_match(std::string(data, len), reg);
            }

            inline bool match(const char *rstr, const char *data, size_t len = 0) {
                if (len == 0) len = strlen(data);
                std::regex reg(rstr);
                return match(reg, data, len);
            }

            inline bool match(const char *rstr, const zcstring& data) {
                std::regex reg(rstr);
                return match(reg, data(), data.size());
            }

            inline bool match(const char *rstr, const zbuffer& data) {
                std::regex reg(rstr);
                return match(reg, data.data(), data.size());
            }
        }

#define exmsg() SuilError::getmsg(std::current_exception())

    }

    template <typename __T>
    inline zcstring::operator __T() const {
        __T to;
        utils::cast(*this, to);
        return to;
    };

    inline uint8_t c2i(const char c) {
        if (c >= '0' && c <= '9') {
            return (uint8_t) (c - '0');
        } else if (c >= 'a' && c <= 'f') {
            return (uint8_t) (c - 'W');
        } else if (c >= 'A' && c <= 'F') {
            return (uint8_t) (c - '7');
        }
        throw SuilError::create("invalid hex number");
    };

    inline char i2c(uint8_t c, bool caps = false) {

        if (c <= 0x9) {
            return c + '0';
        }
        else if (c <= 0xF) {
            if (caps)
                return c + '7';
            else
                return c + 'W';
        }
        throw SuilError::create("invalid hex number");
    };


    template <size_t N>
    struct Blob: iod::jsonvalue, std::array<uint8_t, N> {
        Blob()
            : Blob(true)
        {}

        Blob(bool zero)
        {
            if (zero) memset(Ego.begin(),0, N);
        }

        Blob(std::initializer_list<uint8_t> l)
            : std::array<uint8_t, N>()
        {
            if (l.size() > N)
                throw std::out_of_range("the size of the int list cannot be greater than array size");
            memcpy(Ego.begin(), l.begin(), l.size());
        }

        inline zcstring base64() const {
            return base64::encode(Ego.begin(), N);
        }

        inline zcstring hexstr() const {
            return utils::hexstr(Ego.begin(), N);
        }

        inline void fromhex(const zcstring& hex) {
            utils::bytes(hex, Ego.begin(), MIN(hex.size()/2, N));
        }

        template <typename S>
        void encjv(S& ss) const {
            ss << '"';
            const uint8_t *p = (uint8_t *) Ego.begin();
            for (int i=0; i < N; i++) {
                ss << i2c(p[i]>>4) << i2c((uint8_t) (p[i]&0xF));
            }
            ss << '"';
        }

        template<size_t NN>
        size_t copyfrom(const Blob<NN>& blob) {
            size_t n = MIN(N, NN);
            memcpy(Ego.begin(), blob.begin(), n);
            return n;
        }

        template <size_t S=0>
        bool copy(const char* str) {
            size_t len{strlen(str)};
            if ((S+len) > N)
                return false;
            memcpy(&Ego.begin()[S], str, len);
            return true;
        };

        template <size_t S, size_t NN>
        bool copy(const Blob<NN> bb) {
            if ((S+bb.size())>N)
                return false;
            memcpy(&Ego.begin()[S], bb.begin(), bb.size());
            return true;
        };

        template <size_t S = 0, size_t E = N>
        suil::Blob<E-S> slice() const {
            static_assert(((E>S)&&(E<=N)), "invalid slicing indices");

            suil::Blob<E-S> blob;
            uint8_t *p = (uint8_t *) Ego.begin();
            memcpy(blob.begin(), &p[S], E);
            return std::move(blob);
        };

        template <size_t S=0, size_t E = N>
        void szero() {
            static_assert(((E>S)&&(E<=N)), "invalid zeroing indices");
            memset(&Ego.begin()[S], 0, E-S);
        }

        void zero(size_t s=0, size_t e = N) {
            if ((e > s) && (e <= N)) {
                memset(&Ego.begin()[s], 0, e-s);
            }
        }

        static void decjv(iod::jdecit& jit, Blob<N>& ba) {
            uint8_t *bap = (uint8_t *) ba.begin();
            char c, c1;
            int i{0};
            jit.eat('"');
            for (i; i < N; i++) {
                if ((c = jit.next('"')) == '\0') break;
                c1 = jit.next('"');
                bap[i] = (uint8_t) ((c2i(c)<<4) | c2i(c1));
            }
            jit.eat('"');
        }

        template <size_t S=0, size_t C=N>
        inline bool isnill() const {
            static_assert((C<=N)&&((C+S)<=N), "null check range ins invalid");
            size_t offset{S};
            for(; offset < S+C; offset++)
                if (Ego.begin()[offset] != 0)
                    return false;
            return true;
        }

        template <size_t I=0>
        inline uint8_t& bin() {
            static_assert(I<N, "index should be less than size");
            return Ego.begin()[I];
        }

        template <size_t I=0>
        inline const uint8_t& cbin() const {
            static_assert(I<N, "index should be less than size");
            return Ego.begin()[I];
        }

        void in(Wire& w);
        void out(Wire& w) const;

    } __attribute__((aligned(1)));

    template <typename T>
    zcstring& zcstring::operator+=(const T &t) {
        zbuffer tmp{Ego._len+(sizeof(T)*2)};
        tmp << Ego << t;
        Ego = std::move(zcstring(t));
        return Ego;
    }

    // overhead is 0 byte
    static_assert(sizeof(Blob<1>) == 1);

    template <size_t N>
    inline zbuffer& zbuffer::operator<<(Blob<N> &bb) {
        Ego << bb.hexstr();
        return Ego;
    }

    template <size_t N>
    inline zbuffer& zbuffer::operator<<(const Blob<N> &bb) {
        Ego << bb.hexstr();
        return Ego;
    }

    struct varint : Blob<8> {
        varint(uint64_t v);

        varint();

        template <typename __T>
        __T read() const {
            return (__T) be64toh(*((uint64_t *) Ego.begin()));
        }

        template <typename __T>
        void write(__T v) {
            *((uint64_t *) Ego.begin()) = htobe64((uint64_t) v);
        }

        uint8_t *raw();

        uint8_t length() const;

        void in(Wire& w);

        void out(Wire& w) const;
    };

    template <size_t N>
    void Blob<N>::in(Wire &w) {
        if (!w.pull(Ego.begin(), Ego.size())) {
            SuilError::create("pulling blob failed");
        }
    }

    template <size_t N>
    void Blob<N>::out(Wire &w) const {
        if (!w.push(Ego.begin(), Ego.size())) {
            SuilError::create("pushing blob failed");
        }
    }

    template <typename Key, typename Value>
    struct KVPair {
        typedef decltype(iod::D(
                prop(key,     Key),
                prop(val,     Value)
        )) Type;
    };

    namespace utils {
        /**
         * Load key/value configuration from environment variables
         * @tparam Config The type of configuration to load
         * @param config The configuration object to load into
         * @param prefix the prefix to use when loading confuration
         */
        template <typename Config>
        static void envconfig(Config& config, const char *prefix = "") {
            iod::foreach2(config)|
            [&](auto& m) {
                zcstring key;
                if (prefix)
                    key = utils::catstr(prefix, m.symbol().name());
                else
                    key = zcstring{m.symbol().name()}.dup();
                // convert key to uppercase
                key.toupper();
                m.value() = utils::env(key(), m.value());
            };
        }
    }
}

inline suil::zcstring operator "" _zc(const char* str, size_t len) {
    if (len) {
        return suil::zcstring{str, len, false}.dup();
    }
    return nullptr;
}

#endif //SUIL_SYS_HPP
