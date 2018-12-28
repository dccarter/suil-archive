//
// Created by dc on 09/11/18.
//

#ifndef SUIL_BASE_H
#define SUIL_BASE_H

#include <sys/param.h>
#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <sstream>
#include <string_view>

#include <libmill/libmill.h>

#include <suil/base.inc>
#include <suil/symbols.h>

/**
 * suil base unit for data size. 1 is 1_B
 *
 * @param u the value of bytes to represent
 * @return returns \param u
 */
inline unsigned long long operator ""_B(unsigned long long u) { return u; }

/**
 * converts a value specified in kilobytes to bytes.
 * @param u the value in kilobytes
 * @return the size converted to bytes
 *
 * @example 20_Kb
 */
inline unsigned long long operator ""_Kb(unsigned long long u) { return u * 1000_B; }

/**
 * converts a value specified in megabytes to bytes.
 * @param u the value in megabytes
 * @return the size converted to bytes
 *
 * @example 20_Mb
 */
inline unsigned long long operator ""_Mb(unsigned long long u) { return u * 1000_Kb; }

/**
 * gets time units in microseconds and converts to milliseconds
 * @param f time in microseconds
 * @return given microseconds converted to milliseconds
 */
inline double operator ""_us(unsigned long long f) { return f/1000; }

/**
 * suil's base time unit is milliseconds.
 * @param m the number of milliseconds to represent
 * @return \param m
 */
inline signed long long operator ""_ms(unsigned long long m) { return m; }

/**
 * gets time units in seconds and converts to milliseconds
 * @param s time in seconds
 * @return given seconds converted to milliseconds
 */
inline signed long long operator ""_sec(unsigned long long s) { return s * 1000_ms; }

/**
 * gets time units in minutes and converts to milliseconds
 * @param s time in minutes
 * @return given minutes converted to milliseconds
 */
inline unsigned operator ""_min(unsigned long long s) { return (unsigned) (s * 60_sec); }

/**
 * gets time units in hours and converts to milliseconds
 * @param f time in hours
 * @return given hours converted to milliseconds
 */
inline unsigned operator ""_hr(unsigned long long s) { return (unsigned) (s * 60_min); }

#ifndef suil_ut
#define suil_ut
#endif

/**
 * some personal ego
 */
#define Ego (*this)

/**
 * macro to to retrieve current error in string representation
 */
#define errno_s strerror(errno)

/**
 * symbol variable with name v
 * @param v the name of the symbol
 */
#define var(v) s::_##v
/**
 * access a symbol, \see sym
 */
#define sym(v) var(v)
/**
 * set an option, assign value \param v to variable \param o
 * @param o the variable to assign value to
 * @param v the value to assign to the variable
 */
#define opt(o, v) var(o) = v
/**
 * utility for using symbols for events
 * @param ev the name of the event to access
 */
//#define on(ev) s::_on_##ev

#define prop(name, tp) var(name)  = tp()
/**
 * utility macro to return the size of literal c-string
 * @param ch the literal c-string
 *
 * \example sizeof("Hello") == 5
 */
#define sizeofcstr(ch)   (sizeof(ch)-1)

#define meta struct

#define jrpc    struct
#define srpc    struct
#define service struct


namespace suil {
    /**
     * @def typedef to an std::string_view
     */
    using strview = std::string_view;

    struct OBuffer;
    struct String;

    struct hasher {

        inline size_t operator()(const std::string& key) const {
            return std::hash<std::string>()(key);
        }

        inline size_t operator()(const strview& key) const {
            return std::hash<strview>()(key);
        }

        size_t operator()(const OBuffer& b) const;

        size_t operator()(const String& s) const;

        size_t hash(const char *ptr, size_t len) const;
    };

    /**
     * macro to add a utility function to a class/struct
     * creating shared pointers
     */
#define sptr(type)                          \
public:                                     \
    using Ptr = std::shared_ptr< type >;    \
    template <typename... Args>             \
    inline static Ptr mkshared(Args... args) {          \
        return std::make_shared< type >(    \
            std::forward<Args>(args)...);   \
    }

    /**
* get the current time in milliseconds
* @return the current time in milli seconds
*/
    inline int64_t now() { return mnow(); }

    /**
     * change the given file descriptor to either non-blocking or
     * blocking
     * @param fd the file descriptor whose properties must be changed
     * @param on true when non-blocking property is on, false otherwise
     */
    int nonblocking(int fd, bool on = true);

    /**
     * Structure to hold an ip address
     */
    struct IPAddress {

        static constexpr unsigned IP_ADDR_MAXSTRLEN = IPADDR_IPV4;

        enum : uint8_t {
            IP_ADDR_IPV4      = IPADDR_IPV4,
            IP_ADDR_IPV6      = IPADDR_IPV4,
            IP_ADDR_PREF_IPV4 = IPADDR_IPV4,
            IP_ADDR_PREF_IPV6 = IPADDR_IPV4
        };

        /**
         * convert a local human readable IP address or interface name into an IP address
         * data structure
         * @param name the human readable IP address or the interface name
         * @param port the port
         * @param mode the desired kind of the IP address
         * @return the converted IP address in IPAdress object
         */
        static IPAddress fromLocal(const char *name, int port, int mode) {
            IPAddress addr;
            addr.m_ip = iplocal(name, port, mode);
            return addr;
        }

        /**
         * Converts an IP address in human-readable format, or a name of a remote host into an IPAdress
         * object
         * @param name IP address in human readable or remote host name
         * @param port the port
         * @param mode the desired kind of IP address
         * @param deadline the deadline for querying the DNS for the IP address
         * @return IPAddress object representing the given human readable format
         */
        static IPAddress fromRemote(const char *name,int port, int mode, int64_t deadline) {
            IPAddress addr;
            addr.m_ip = ipremote(name, port, mode, deadline);
            return addr;
        }

        /**
         * converts an IPAddress object into human readable format
         * @return a string representation of the IPAddress object
         */
        inline const std::string& toString() {
            if (m_ipStr.empty()) {
                char tmp[IPADDR_MAXSTRLEN];
                ipaddrstr(m_ip, tmp);
                m_ipStr = std::string(tmp);
            }
            return m_ipStr;
        }

        /**
         * eq compare with other IPAddress object
         * @param other the object to compare against
         * @return true if the objects are equal, false otherwise
         */
        inline bool operator==(const IPAddress& other) {
            if (this == &other) return true;
            return memcmp(m_ip.data, other.m_ip.data, sizeof(ipaddr)) == 0;
        }

        /**
         * neq compare with other IPAddress object
         * @param other the object to compare against
         * @return true if the objects are not equal, false otherwise
         */
        inline bool operator!=(const IPAddress& other) { return !(*this == other); }

        /**
         * lt compare with other IPAddress object
         * @param other the object to compare against
         * @return true if object on (this) left is less than one on right (\param other)
         */
        inline bool operator<(const IPAddress& other) {
            if (this == &other) return false;
            return memcmp(m_ip.data, other.m_ip.data, sizeof(ipaddr)) < 0;
        }

        /**
         * gt compare with other IPAddress object
         * @param other the object to compare against
         * @return true if object on (this) left is greater than one on right (\param other)
         */
        inline bool operator>(const IPAddress& other) {
            if (this == &other) return false;
            return memcmp(m_ip.data, other.m_ip.data, sizeof(ipaddr)) > 0;
        }

        /**
         * lte compare with other IPAddress object
         * @param other the object to compare against
         * @return true if object on (this) left is less than or equal to one on right (\param other)
         */
        inline bool operator<=(const IPAddress& other) {
            if (this == &other) return true;
            return memcmp(m_ip.data, other.m_ip.data, sizeof(ipaddr)) <= 0;
        }

        /**
         * gte compare with other IPAddress object
         * @param other the object to compare against
         * @return true if object on (this) left is greater than or equal to one on right (\param other)
         */
        inline bool operator>=(const IPAddress& other) {
            if (this == &other) return true;
            return memcmp(m_ip.data, other.m_ip.data, sizeof(ipaddr)) >= 0;
        }

    private:

        ipaddr m_ip{};
        std::string m_ipStr{""};
    };

    /**
     * General framework exception which can be used
     * to throw exception represented by error codes
     */
    struct Exception : std::exception {

        static constexpr int IndexOutOfBounds           = 10;
        static constexpr int OutOfRange                 = 11;
        static constexpr int UnsupportedOperation       = 12;
        static constexpr int MemoryAllocationFailure    = 13;
        static constexpr int AccessViolation            = 14;
        static constexpr int InvalidArguments           = 15;
        static constexpr int KeyNotFound                = 16;

        /**
         * Create a new exception
         * @param code the exception code
         * @param msg the exception message
         */
        Exception(std::string&& msg = "", int code = 0)
                : std::exception(),
                  Msg{std::move(msg)},
                  Code{code}
        {}

        Exception(const Exception&) = default;
        Exception&operator=(const Exception&) = default;

        /**
         * exceptoion move constructor
         * @param e the exception to move
         */
        Exception(Exception&& e) noexcept
                : Code(e.Code),
                  Msg(std::move(e.Msg))
        { e.Code = 0; }

        /**
         * exception move assignment operator
         * @param e the exception to move
         * @return Exception
         */
        Exception&operator=(Exception&& e) noexcept {
            Code = e.Code;
            Msg  = std::move(e.Msg);
            e.Code = 0;
            return *this;
        }

        /**
         * create an exception with an exception message string streamed from
         * \arg args parameters
         * @tparam Args argument types deduced by compiler
         * @param code the exception code
         * @param args argument list to build up a string
         * @return and Exception object which can be thrown
         */
        template <typename... Args>
        static Exception create(int code, Args... args) {
            std::stringstream ss;
            create(ss, std::forward<Args>(args)...);
            return Exception(ss.str(), code);
        }

        /**
         * create an exception with an exception message string streamed from
         * \arg args parameters. The exception code defaults to 0
         * @tparam Args argument types deduced by compiler
         * @param args argument list to build up a string
         * @return and Exception object which can be thrown
         */
        template <typename... Args>
        static inline Exception create(Args... args) {
            return create(0, std::forward<Args>(args)...);
        }

        template <typename... Args>
        static inline Exception indexOutOfBounds(Args... args) {
            return create(IndexOutOfBounds, "IndexOutOfBounds: ",std::forward<Args>(args)...);
        }

        template <typename... Args>
        static inline Exception unsupportedOperation(Args... args) {
            return create(UnsupportedOperation, "UnsupportedOperation: ",std::forward<Args>(args)...);
        }

        template <typename... Args>
        static inline Exception accessViolation(Args... args) {
            return create(AccessViolation, "AccessViolation: ",std::forward<Args>(args)...);
        }

        template <typename... Args>
        static inline Exception allocationFailure(Args... args) {
            return create(MemoryAllocationFailure, "MemoryAllocationFailure: ",std::forward<Args>(args)...);
        }

        template <typename... Args>
        static inline Exception outOfRange(Args... args) {
            return create(OutOfRange, "OutOfRangeError: ",std::forward<Args>(args)...);
        }

        template <typename... Args>
        static inline Exception invalidArguments(Args... args) {
            return create(InvalidArguments, "InvalidArgumentsError: ",std::forward<Args>(args)...);
        }

        template <typename... Args>
        static inline Exception keyNotFound(Args... args) {
            return create(KeyNotFound, "KeyNotFoundError: ", std::forward<Args>(args)...);
        }

        /**
         * Construct exception from current exception
         * @return the current exception
         *
         * @note this function is only applicable on a catch exception
         */
        static Exception fromCurrent();

        /**
         * the exception code associated with current exception object
         * @property
         */
        int Code{0};

        /**
         * the exception message
         * @property
         */
        std::string Msg{""};

        /**
         * eq equality comparison operator
         * @param other the other exception to compare to
         * @return true if the exceptions have the same code
         */
        inline bool operator==(const Exception& other) {
            return Code == other.Code;
        }

        /**
         * neq equality comparison operator
         * @param other the other exception to compare to
         * @return true if the exceptions have different codes
         */
        inline bool operator!=(const Exception& other) {
            return Code == other.Code;
        }

        /**
         * get the exception message
         * @return
         */
        inline const char*operator()() {
            return this->what();
        }

        /**
         * returns the exception message in c-style string
         * @return c-style string exception message
         */
        virtual const char* what() const noexcept {
            return Msg.c_str();
        }

    private:
        template <typename Arg, typename... Args>
        static void create(std::stringstream& ss, Arg arg, Args... args) {
            ss << arg;
            if constexpr (sizeof...(args))
                create(ss, std::forward<Args>(args)...);
        }
    };


    /**
     * Date/Time formatting functions
     */
    struct [[Untested]] Datetime {
        /**
         * creates a new Datetime object from given timestamp
         * @param t the timestamp to create a date-time object from
         */
        Datetime(time_t t);

        /**
         * creates a Datetime object using the current time
         */
        Datetime();

        /**
         * creates a Datetime object from a string representing
         * the date and time formatted using HTTP standard
         * @param http_time the HTTP formatted string with date & time
         */
        Datetime(const char *http_time);

        /**
         * Creates a date from the given \param str which is a
         * date and time representation formatted using the given
         * \param fmt
         * @param fmt the expected format of the date/time string
         * @param str the date/time string that will be parsed into a Datetime object
         */
        Datetime(const char *fmt, const char *str);

        /**
         * get a string representation of the current Datetime object
         * @param out the output buffer to hold the Date/time
         * @param sz the size of the output buffer
         * @param fmt the desired output format of the date (see strtime)
         * @return a c-style string representing the current date/time object as request
         *
         * @note if \param out is null or \param sz is 0 or \param fmt is null
         * null will be returned
         */
        const char* str(char *out, size_t sz, const char *fmt);

        /**
         * format the string into a static buffer using suil
         * logging format
         * @return c-style string with current date/time object respresented
         * in logging format
         *
         * @note this API is not thread safe, if used in a multi-threading
         * environment, the API should be invoked under lock and the returned
         * buffer shouldn't escape the lock
         */
        const char* operator()() {
            static char buf[64] = {0};
            return str(buf, sizeof(buf), LOG_FMT);
        }


        /**
         * format the string into a static buffer using the given \param fmt
         *
         * @param fmt the desired output date/time format
         *
         * @return c-style string with current date/time object respresented
         * in logging format
         *
         * @note this API is not thread safe, if used in a multi-threading
         * environment, the API should be invoked under lock and the returned
         * buffer shouldn't escape the lock
         */
        const char* operator()(const char *fmt) {
            static char buf[64] = {0};
            return str(buf, sizeof(buf), fmt);
        }

        /**
         * get a string representation of the current Datetime object
         *
         * @param buf the output buffer to hold the Date/time
         * @param sz the size of the output buffer
         * @param fmt the desired output format of the date (see strtime)
         * @return a c-style string representing the current date/time object as request
         *
         * @note if \param buf is null or \param sz is 0 or \param fmt is null
         * null will be returned
         */
        const char* operator()(char *buf, size_t sz, const char *fmt) {
            return str(buf, sz, fmt);
        }

        /**
         * get current date/time asctime representation
         * @return
         *
         * @see https://linux.die.net/man/3/asctime
         */
        inline operator const char *() {
            return asctime(&m_tm);
        }

        /**
         * implicit cast to tm object reference
         * @return const reference to the tm object of this date/time object
         */
        inline operator const tm&() const {
            return m_tm;
        }

        /**
         * get the timestamp represented by this object
         * @return the timestamp equivalence of this object
         */
        operator time_t();

    private:
        struct tm       m_tm{};
        time_t          m_t{0};

    public:
        /**
         * @static
         * HTTP date string format
         */
        static constexpr char *HTTP_FMT = (char *) "%a, %d %b %Y %T GMT";
        /**
         * @static
         * suil date/time logging string format
         */
        static constexpr char *LOG_FMT  = (char *) "%Y-%m-%d %H:%M:%S";
    };

    struct Data {
    public:

        Data();

        Data(void *data, size_t size, bool own = true);

        Data(const void *data, size_t size, bool own = true);

        Data(const Data& d) noexcept;

        Data& operator=(const Data& d) noexcept;

        Data(Data&& d) noexcept;

        Data& operator=(Data&& d) noexcept;

        inline Data peek() {
            Data d{Ego.m_data, Ego.m_size, false};
            return std::move(d);
        }

        inline bool empty() {
            return Ego.m_size == 0;
        }

        inline size_t size() const {
            return Ego.m_size;
        }

        inline uint8_t* data() {
            return Ego.m_data;
        }

        inline const uint8_t* cdata() const {
            return Ego.m_cdata;
        }

        inline bool operator==(const Data& other) {
            return (Ego.m_size == other.m_size) &&
                   Ego.m_size &&
                   (memcmp(Ego.m_data, other.m_data, Ego.m_size) == 0);
        }

        inline bool operator!=(const Data& other) {
            return (Ego.m_size != other.m_size) ||
                   (Ego.m_size == 0) ||
                   (memcmp(Ego.m_data, other.m_data, Ego.m_size) != 0);
        }

        Data copy() const;
        void clear();

        ~Data() {
            Ego.clear();
        }
        
    private:
        union {
            uint8_t       *m_data{nullptr};
            const uint8_t *m_cdata;
        };
        bool     m_own{false};
        uint32_t m_size{0};
    } __attribute__((aligned(1)));

    namespace version {
        extern const uint8_t  MAJOR;
        extern const uint8_t  MINOR;
        extern const uint16_t  PATCH;
        extern const uint32_t  BUILD;
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
    extern const uint8_t spid;
}
#endif //SUIL_BASE_H
