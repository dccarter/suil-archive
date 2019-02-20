//
// Created by dc on 20/11/18.
//

#ifndef SUIL_WIRE_H
#define SUIL_WIRE_H

#include <suil/varint.h>
#include <suil/blob.h>

namespace suil {

    struct OBuffer;
    struct String;

    struct Wire {

        inline bool push(const uint8_t e[], size_t es) {
            return forward(e, es) == es;
        }

        inline void pull(uint8_t e[], size_t es) {
            reverse(e, es);
        }

        Wire& operator<<(const VarInt& vi) {
            uint8_t sz{vi.length()};
            push(&sz, 1);
            auto tmp = vi.read<uint64_t>();
            push((uint8_t *)&tmp, sz);
            return Ego;
        }

        Wire& operator>>(VarInt& vi) {
            uint8_t sz{0};
            pull(&sz, 1);
            // actual value
            uint64_t tmp{0};
            Ego.pull((uint8_t *)&tmp, sz);
            vi.write(tmp);
            return Ego;
        }

        Wire& operator<<(const Data& d) {
            VarInt sz(d.size());
            Ego << sz;
            push(d.cdata(), d.size());
            return Ego;
        }

        Wire& operator>>(Data& d) {
            VarInt tmp(0);
            Ego >> tmp;
            auto sz = tmp.read<uint64_t>();
            if (sz) {
                // get access to serialization buffer
                d = Data(Ego.reverse(sz), sz, false);
            }
            return Ego;
        }

        template <size_t N>
        Wire& operator<<(const Blob<N>& b) {
            VarInt sz(b.size());
            Ego << sz;
            push(&b.cbin(), b.size());
            return Ego;
        }

        template <size_t N>
        Wire& operator>>(Blob<N>& b) {
            VarInt tmp(0);
            Ego >> tmp;
            auto sz = tmp.read<uint64_t>();
            if (sz) {
                // get access to serialization buffer
                b.copy(Ego.reverse(sz), sz);
            }
            return Ego;
        }

        template <typename T>
        inline Wire& operator<<(const T val) {
            if constexpr (std::is_arithmetic<T>::value) {
                uint64_t tmp{0};
                memcpy(&tmp, &val, sizeof(T));
                tmp = htole64((uint64_t) tmp);
                forward((uint8_t *) &tmp, sizeof(val));
                return Ego;
            }
            else {
                val.toWire(Ego);
                return Ego;
            }
        };

        template <typename T>
        inline Wire& operator>>(T& val) {
            if constexpr (std::is_arithmetic<T>::value) {
                uint64_t tmp{0};
                reverse((uint8_t *) &tmp, sizeof(val));
                tmp = le64toh(tmp);
                memcpy(&val, &tmp, sizeof(T));
                return Ego;
            }
            else {
                val = T::fromWire(Ego);
                return Ego;
            }
        };

        template <typename... T>
        Wire& operator<<(const iod::sio<T...>& o) {
            iod::foreach2(o) |
            [&](auto &m) {
                if (!m.attributes().has(var(unwire)) || Ego.always) {
                    /* use given metadata to to set options */
                    Ego << m.symbol().member_access(o);
                }
            };
            return Ego;
        }

        template <typename... T>
        Wire& operator>>(iod::sio<T...>& o) {
            iod::foreach2(o) |
            [&](auto &m) {
                if (!m.attributes().has(var(unwire)) || Ego.always) {
                    /* use given metadata to to set options */
                    Ego >> m.symbol().member_access(o);
                }
            };
            return Ego;
        }

        template <typename T>
        Wire& operator<<(const std::vector<T>& v) {
            VarInt sz{v.size()};
            Ego << sz;
            for (auto& e: v) {
                Ego << e;
            }
            return Ego;
        }

        template <typename T>
        Wire& operator>>(std::vector<T>& v) {
            VarInt sz{0};
            Ego >> sz;
            uint64_t entries{sz.read<uint64_t>()};
            for (int i = 0; i < entries; i++) {
                T entry{};
                Ego >> entry;
                v.emplace_back(std::move(entry));
            }

            return Ego;
        }

        template <typename... T>
        Wire& operator<<(const std::vector<iod::sio<T...>>& v) {
            VarInt sz{v.size()};
            Ego << sz;
            for (const iod::sio<T...>& e: v) {
                Ego << e;
            }

            return Ego;
        }

        template <typename... T>
        Wire& operator>>(std::vector<iod::sio<T...>>& v) {
            VarInt sz{0};
            Ego >> sz;
            uint64_t entries{sz.read<uint64_t>()};
            for (int i = 0; i < entries; i++) {
                iod::sio<T...> entry;
                Ego >> entry;
                v.emplace_back(std::move(entry));
            }

            return Ego;

        }

        inline Wire& operator<<(const char* str) {
            return (Ego << Data(str, strlen(str), false));
        }

        inline Wire& operator<<(const std::string& str) {
            return (Ego << Data(str.data(), str.size(), false));
        }

        inline Wire& operator>>(std::string& str) {
            // automatically decode reverse
            Data rv;
            Ego >> rv;
            if (!rv.empty()) {
                // this will copy out the string
                str = std::string((char *)rv.data(), rv.size());
            }

            return Ego;
        }

        inline Wire& operator()(bool always) {
            Ego.always = always;
            return Ego;
        }

        virtual bool move(size_t size) = 0;

        virtual Data raw() const = 0;

        bool isFilterOn() const { return !always; }

    protected suil_ut:
        virtual size_t   forward(const uint8_t e[], size_t es) = 0;
        virtual void     reverse(uint8_t e[], size_t es) = 0;
        virtual const uint8_t *reverse(size_t es) = 0;
        bool           always{false};
    };

    struct Breadboard : Wire {

        Breadboard(uint8_t* buf, size_t sz)
                : sink(buf),
                  M(sz)
        {}

        size_t forward(const uint8_t e[], size_t es) override {
            if ((M-T) < es) {
                throw Exception::create("Breadboard buffer out of memory, requested: ", es,
                                        " available: ", (M-T));
            }
            memcpy(&sink[T], e, es);
            T += es;
            return es;
        }

        void reverse(uint8_t e[], size_t es) override {
            if (es <= (T-H)) {
                memcpy(e, &sink[H], es);
                H += es;
            }
            else
                throw  Exception::create("Breadboard out of read buffer bytes, requested: ", es,
                        " available: ", (T-H));
        }

        const uint8_t* reverse(size_t es) override {
            if (es <= (T-H)) {
                size_t tmp{H};
                H += es;
                return &sink[tmp];
            }
            else
                throw  Exception::create("Breadboard out of read buffer bytes, requested: ", es,
                            " available: ", (T-H));
        }

        virtual uint8_t *rd() {
            return &sink[H];
        }

        bool move(size_t size) override {
            if (Ego.size() >= size) {
                Ego.H += size;
                return true;
            }
            return false;
        }

        inline void reset() {
            H = T = 0;
        }

        ~Breadboard() {
            reset();
        }

        inline size_t size() const {
            return T - H;
        }

        Data raw() const {
            return Data{&sink[H], size(), false};
        };

        void toHexStr(OBuffer& ob) const;

        bool fromHexStr(String& str);

    protected suil_ut:
        friend
        Wire&operator<<(Wire& w, const Breadboard& bb) {
            if (&w == &bb) {
                // cannot serialize to self
                throw Exception::create("serialization loop detected, cannot serialize to self");
            }
            return (w << bb.raw());
        }

        uint8_t     *sink;
        size_t       H{0};
        size_t       T{0};
        size_t       M{0};
    };

    struct Heapboard : Breadboard {
        Heapboard(size_t size);

        Heapboard(const uint8_t *buf = nullptr, size_t size = 0);

        explicit Heapboard(const Data& data)
            : Heapboard(data.cdata(), data.size())
        {}

        Heapboard(const Heapboard& hb)
            : Heapboard()
        {
            if (hb.data != nullptr)
                Ego.copyfrom(hb.data, hb.M);
        }

        Heapboard&operator=(const Heapboard& hb) {
            if (hb.data != nullptr)
                Ego.copyfrom(hb.data, hb.M);
            return Ego;
        }

        Heapboard(Heapboard&& hb);

        Heapboard& operator=(Heapboard&& hb);

        void copyfrom(const uint8_t* data, size_t sz);

        bool seal();

        inline void clear() {
            if (Ego.own && Ego.data) {
                free(Ego.data);
                Ego.H = Ego.M = Ego.T;
                Ego.own = false;
            }
            Ego.sink = Ego.data = nullptr;
        }

        virtual ~Heapboard() {
            clear();
        }

        Data release();

    private suil_ut:
        union {
            uint8_t *data{nullptr};
            const uint8_t* _cdata;
        };
        bool      own{false};
    };

    template <size_t N=8192>
    struct Stackboard: Breadboard {
        static_assert(((N<=8192) && ((N&(N-1))==0)), "Requested Breadboard size greater than 8912");
        Stackboard()
            : Breadboard(data, N)
        {}

    private suil_ut:
        uint8_t   data[N];
    };

    template <typename Mt>
    inline void metaFromWire(Mt& o, suil::Wire& w) {
        iod::foreach(Mt::Meta) |
        [&](auto &m) {
            if (!m.attributes().has(var(unwire)) || w.isFilterOn()) {
                /* use given metadata to to set options */
                w >> m.symbol().member_access(o);
            }
        };
    }

    template <typename Mt>
    inline void metaToWire(const Mt& o, suil::Wire& w) {
        iod::foreach(Mt::Meta) |
        [&](auto &m) {
            if (!m.attributes().has(var(unwire)) || w.isFilterOn()) {
                /* use given metadata to to set options */
                w << m.symbol().member_access(o);
            }
        };
    }
}

#endif //SUIL_WIRE_H
