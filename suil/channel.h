//
// Created by dc on 12/11/18.
//

#ifndef SUIL_ASYNC_H
#define SUIL_ASYNC_H

#include <libmill/libmill.h>

#include <suil/base.h>

namespace suil {

    /**
     * This is a type which basically does nothing, its void
     * and doesn't have any significant meaning. It can be passed
     * around as a template parameter where the parameter value is
     * insignificant
     */
    struct Void_t {
        template <typename... __A>
        Void_t(__A...) {}
        operator bool() const { return false; }
        template <typename... __A>
        void operator()(__A...){}
        template <typename __A>
        bool operator==(const __A& a) const { return this == &a; }
        template <typename __A>
        bool operator!=(const __A& a) const { return this != &a; }

        uint8_t byte[0];
    } __attribute((packed));
    static_assert(sizeof(Void_t)==0, "Size of Void_t must be 0");

    /**
     * a global value of type __Void which can be passed around
     * to functions that accept template parameters and the parameter
     * is not really needed
     */
    extern Void_t Void;

    /**
     * The asynchronous type encapsulates libmill's channel. It can be
     * used to wait for async operations to complete or to exchange data
     * between co routines
     * @tparam R the result type
     * @tparam N the channel buffer specifies how much data can be buffered
     * into the underlying channel without the reader reading them.
     */
    template <typename R, int N = 0>
    struct Channel {
        /**
         * Creates an asynchronous channel
         * @tparam Args variadic for terminal value of the
         * channel
         * @param args these arguments are passed to the constructor
         * of the result type to create a value that the channel will
         * listen to an use as a termination command
         */
        template<typename... Args>
        Channel(Args... args)
        : ch(chmake(R, N)),
                term(args...)
        {}

        /**
         * Creates an empty channel, basically a null and
         * not useful channel
         */
        Channel(Void_t & )
        : ch(nullptr)
        {}

        /**
         * move constructor for an async, these must never be copied
         * @param as the async to move
         */
        Channel(Channel && as)
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
        Channel &operator=(Channel &&async) {
            term = async.term;
            ch = async.ch;
            waitn = async.waitn;
            ddline = async.ddline;
            async.ch = nullptr;
        }

        /**
         * write a value to the channel
         * @param res the value to write to the channel
         * @return the async being written to
         */
        Channel &operator+=(const R res) {
            if (ch != nullptr)
                chs(ch, R, res);
            return *this;
        };

        /**
         * write a value to the channel
         * @param res the value to write to the channel
         * @return the async being written to
         */
        Channel &operator<<(const R res) {
            ;
            if (ch != nullptr)
                chs(ch, R, res);
            return *this;
        }

        /**
         * notify the waiter/channel receive that sending is completed
         */
        void operator!() {
            chdone(ch, R, term);
        }

        /**
         * when called before the wait command, it will redirect the channel
         * to receive n entries. The command is only ever useful when followed
         * by a wait command
         * @param n the number of time the channel will wait
         * @return self
         */
        Channel &operator()(int n) {
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
        template<typename F>
        bool operator|(F f) {
            int64_t dd = ddline;
            ddline = -1;
            // cache and clear the waitn flag
            int rxd = waitn;
            waitn = -1;

            if (ch == nullptr) {
                return false;
            }

            while (ch && (rxd < 0 || rxd > 0)) {
                R res;
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
        bool operator>>(R &res) {
            waitn = -1;
            int64_t dd = ddline;
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
        R operator()() {
            R r;
            (*this) >> r;
            return r;
        }

        /**
         * set a timeout on the channel
         * @param timeout the timeout the next read should
         * wait for data on the channel before giving up
         * @return self
         */
        Channel &operator[](int64_t timeout) {
            if (this->ddline <= 0 && timeout > 0)
                this->ddline = mnow() + timeout;
            return *this;
        }

        /**
         * the destructor will close and destroy the underlying channel
         */
        ~Channel()
        {
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

    private suil_ut:

        inline bool chrx(int64_t dd, R &res) {
            bool rc = true;
            if (dd <= 0) {
                res = chr(ch, R);
            } else {
                choose{
                        chin(ch, R, tmp) :
                        res = tmp;
                        deadline(dd):
                        res = term;
                        rc  = false;
                        chend
                }
            }
            return rc;
        }
        chan    ch{nullptr};
        R       term;
        int     waitn{-1};
        int64_t ddline{-1};
    };
}

#endif //SUIL_ASYNC_H
