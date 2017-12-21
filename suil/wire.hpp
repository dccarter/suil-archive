//
// Created by dc on 14/12/17.
//

#ifndef SUIL_WIRE_HPP
#define SUIL_WIRE_HPP

#include <suil/sys.hpp>

namespace suil {

    using rawbuffer = const std::pair<const uint8_t*, size_t>;

    struct breadboard : wire {

        breadboard(uint8_t* buf, size_t sz)
            : sink(buf),
              M(sz)
        {}

        virtual size_t forward(const uint8_t e[], size_t es) {
            size_t m{MIN((M-T), es)};
            if (m) {
                memcpy(&sink[T], e, m);
                T += m;
            }
            return m;
        }

        virtual size_t reverse(uint8_t e[], size_t es) {
            size_t m{MIN((T-H), es)};
            if (m) {
                memcpy(e, &sink[H], m);
                H += m;
            }
            return m;
        }

        inline void reset() {
            H = T = 0;
        }

        ~breadboard() {
            reset();
        }

        inline size_t size() const {
            return T - H;
        }

        rawbuffer raw() const {
            return {&sink[H], size()};
        };

        zcstring hexstr() const {
            auto tmp = Ego.raw();
            return utils::hexstr(tmp.first, tmp.second);
        }

        bool fromhexstr(zcstring& str) {
            if ((str.len>>1) > M-T)
                return false;
            utils::bytes(str, &sink[T], (M-T));
            T += (str.len>>1);
            return true;
        }

    protected:
        uint8_t     *sink;
        size_t       H{0};
        size_t       T{0};
        size_t       M{0};
    };

    struct heapboard : breadboard {
        heapboard(size_t size)
            : breadboard(data, size),
              data((uint8_t *)memory::alloc(size)),
              own{true}
        {
            Ego.sink = data;
            Ego.M    = size;
        }

        heapboard(uint8_t *buf = nullptr, size_t size = 0)
            : data(buf),
              breadboard(data, size)
        {
            // push some data into it;
            Ego.sink = data;
            Ego.M    = M;
            Ego.T    = size;
            Ego.H    = 0;
        }

        heapboard(const heapboard&) = delete;
        heapboard(heapboard&&) = delete;
        heapboard&operator=(const heapboard&) = delete;
        heapboard&operator=(heapboard&&) = delete;

        void copyfrom(const uint8_t* data, size_t sz) {
            if (data && sz>0) {
                clear();
                data = (uint8_t *)memory::alloc(sz);
                M    = sz;
                own  = true;
                memcpy(Ego.data, data, sz);
            }
        }

        inline void clear() {
            if (own && data) {
                memory::free(data);
                data = nullptr;
            }
        }

        ~heapboard() {
            clear();
        }

    private:
        uint8_t   *data{nullptr};
        bool      own{false};
    };

    template <size_t N=8192>
    struct stackboard: breadboard {
        static_assert(((N<=8192) && (N&(N-1))==0), "Requested breadboard size greater than 8912");
        stackboard()
            : breadboard(data, N)
        {}

    private:
        uint8_t   data[N];
    };
}

#endif //SUIL_WIRE_HPP
