//
// Created by dc on 14/12/17.
//

#ifndef SUIL_WIRE_HPP
#define SUIL_WIRE_HPP

#include <suil/sys.hpp>

namespace suil {

    Wire& Wire::operator<<(const char *str) {
        zcstring tmp{str};
        Ego << tmp;
        return Ego;
    }

    Wire& Wire::operator<<(const std::string &str) {
        zcstring tmp{str.data(), str.size(), false};
        Ego << tmp;
        return Ego;
    }

    Wire& Wire::operator>>(std::string &str) {
        varint v{0};
        Ego >> v;
        uint64_t tmp{v.read<uint64_t>()};
        if (tmp > 0) {
            str.resize(tmp, '\0');
            if (!reverse((uint8_t *)str.data(), tmp)) {
                SuilError::create("pulling string from wire failed");
            }
        }

        return Ego;
    }

    template <typename __T>
    Wire& Wire::operator<<(const std::vector<__T> &vec) {
        varint sz{vec.size()};
        Ego << sz;
        for (auto& e: vec) {
            Ego << e;
        }

        return Ego;
    }

    template <typename... __T>
    Wire& Wire::operator<<(const std::vector<iod::sio<__T...>> &vec) {
        varint sz{vec.size()};
        Ego << sz;
        for (const iod::sio<__T...>& e: vec) {
            Ego << e;
        }

        return Ego;
    }

    template <typename... __T>
    Wire& Wire::operator>>(std::vector<iod::sio<__T...>> &vec) {
        varint sz{0};
        Ego >> sz;
        uint64_t entries{sz.read<uint64_t>()};
        for (int i = 0; i < entries; i++) {
            iod::sio<__T...> entry;
            Ego >> entry;
            vec.emplace_back(std::move(entry));
        }

        return Ego;
    }

    template <typename __T>
    Wire& Wire::operator>>(std::vector<__T> &vec) {
        varint sz{0};
        Ego >> sz;
        uint64_t entries{sz.read<uint64_t>()};
        for (int i = 0; i < entries; i++) {
            __T entry{};
            Ego >> entry;
            vec.emplace_back(std::move(entry));
        }

        return Ego;
    }

    template <typename... __T>
    Wire& Wire::operator<<(const iod::sio<__T...> &o) {
        //typedef __internal::remove_unwired<decltype(o)> wireble;
        iod::foreach2(o) |
        [&](auto &m) {
            if (!m.attributes().has(var(unwire)) || Ego.always) {
                /* use given metadata to to set options */
                Ego << m.symbol().member_access(o);
            }
        };
        return Ego;
    }

    template <typename... __T>
    Wire& Wire::operator>>(iod::sio<__T...> &o) {
        //typedef __internal::remove_unwired<decltype(o)> wireble;
        iod::foreach2(o) |
        [&](auto &m) {
            if (!m.attributes().has(var(unwire)) || Ego.always) {
                /* use given metadata to to set options */
                Ego >> m.symbol().member_access(o);
            }
        };
        return Ego;
    }

    using rawbuffer = const std::pair<const uint8_t*, size_t>;

    struct breadboard : Wire {

        breadboard(uint8_t* buf, size_t sz)
            : sink(buf),
              M(sz)
        {}

        virtual size_t forward(const uint8_t e[], size_t es) {
            if ((M-T) < es) {
                throw SuilError::create("breadboard buffer out of memory, requested: ", es,
                            " available: ", (M-T));
            }
            memcpy(&sink[T], e, es);
            T += es;
            return es;
        }

        virtual size_t reverse(uint8_t e[], size_t es) {
            size_t m{MIN((T-H), es)};
            if (m) {
                memcpy(e, &sink[H], m);
                H += m;
            }
            return m;
        }

        virtual uint8_t *rd() {
            return &sink[H];
        }

        virtual bool move(size_t size) {
            if (Ego.size() >= size) {
                Ego.H += size;
                return true;
            }
            return false;
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

        Data raw() const {
            return Data{&sink[H], size(), false};
        };

        zcstring hexstr() const {
            auto tmp = Ego.raw();
            return utils::hexstr(tmp.data(), tmp.size());
        }

        bool fromhexstr(zcstring& str) {
            if ((str.size()>>1) > M-T)
                return false;
            utils::bytes(str, &sink[T], (M-T));
            T += (str.size()>>1);
            return true;
        }

        void out(Wire& w) const;

    protected:
        uint8_t     *sink;
        size_t       H{0};
        size_t       T{0};
        size_t       M{0};
    };

    struct heapboard : breadboard {
        heapboard(size_t size)
            : data((uint8_t *)memory::alloc(size)),
              breadboard(data, size),
              own{true}
        {
            Ego.sink = Ego.data;
            Ego.M    = size;
        }

        heapboard(const uint8_t *buf = nullptr, size_t size = 0)
            : _cdata(buf),
              breadboard(data, size)
        {
            // push some data into it;
            Ego.sink = data;
            Ego.M    = size;
            Ego.T    = size;
            Ego.H    = 0;
        }

        explicit heapboard(const Data& data)
            : heapboard(data.cdata(), data.size())
        {}

        heapboard(const heapboard& hb)
            : heapboard()
        {
            if (hb.data != nullptr)
                Ego.copyfrom(hb.data, hb.M);
        }

        heapboard&operator=(const heapboard& hb) {
            if (hb.data != nullptr)
                Ego.copyfrom(hb.data, hb.M);
        }

        heapboard(heapboard&& hb)
            : heapboard(hb.data, hb.M)
        {
            Ego.own = hb.own;
            Ego.H = hb.H;
            Ego.T = hb.T;
            hb.own  = false;
            hb.data = hb.sink = nullptr;
            hb.M = hb.H = hb.T = 0;
        }

        heapboard& operator=(heapboard&& hb) {
            Ego.sink = Ego.data = hb.data;
            Ego.own = hb.own;
            Ego.H = hb.H;
            Ego.T = hb.T;
            hb.own  = false;
            hb.data = hb.sink = nullptr;
            hb.M = hb.H = hb.T = 0;
            return  Ego;
        }

        void copyfrom(const uint8_t* data, size_t sz) {
            if (data && sz>0) {
                clear();
                Ego.data = (uint8_t *)memory::alloc(sz);
                Ego.sink = Ego.data;
                Ego.M    = sz;
                Ego.own  = true;
                Ego.H    = 0;
                Ego.T    = sz;
                memcpy(Ego.data, data, sz);
            }
        }

        bool seal() {
            size_t tmp{size()};
            if (Ego.own && tmp) {
                /* 65 K maximum size
                 * if (size()) < 75% */
                if (tmp < (M-(M>>1))) {
                    void* td = memory::alloc(tmp);
                    size_t tm{M}, tt{T}, th{H};
                    memcpy(td, &Ego.data[H], tmp);
                    clear();
                    Ego.sink = Ego.data = (uint8_t *)td;
                    Ego.own = true;
                    Ego.M   = tm;
                    Ego.T   = tt;
                    Ego.H   = th;

                    return true;
                }
            }
            return false;
        }

        inline void clear() {
            if (Ego.own && Ego.data) {
                memory::free(Ego.data);
                Ego.H = Ego.M = Ego.T;
                Ego.own = false;
            }
            Ego.sink = Ego.data = nullptr;
        }

        ~heapboard() {
            clear();
        }

        void in(Wire& w);

        Data release() {
            if (Ego.size()) {
                // seal buffer
                Ego.seal();
                Data tmp{Ego.data, Ego.size(), Ego.own};
                Ego.own = false;
                Ego.clear();
                return std::move(tmp);
            }

            return Data{};
        }

    private:
        union {
            uint8_t *data{nullptr};
            const uint8_t* _cdata;
        };
        bool      own{false};
    };

    template <size_t N=8192>
    struct stackboard: breadboard {
        static_assert(((N<=8192) && ((N&(N-1))==0)), "Requested breadboard size greater than 8912");
        stackboard()
            : breadboard(data, N)
        {}

    private:
        uint8_t   data[N];
    };
}

#endif //SUIL_WIRE_HPP
