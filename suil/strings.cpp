//
// Created by dc on 1/5/18.
//
#include "sys.hpp"

namespace suil {

    zcstring::zcstring()
        : _str(nullptr),
          _len(0),
          _own(0)
    {}

    zcstring::zcstring(const char *str)
        : _cstr(str),
          _len((uint32_t) (str ? strlen(str) : 0)),
          _own(0)
    {}

    zcstring::zcstring(const strview_t str, bool own)
        : _cstr(str.data()),
          _len((uint32_t) (str.size())),
          _own((uint8_t) (own ? 1 : 0))
    {}

    zcstring::zcstring(const std::string &str, bool own)
        : _cstr(own ? utils::strndup(str.data(), str.size()) : str.data()),
          _len((uint32_t) (str.size())),
          _own((uint8_t) (own ? 1 : 0))
    {}

    zcstring::zcstring(const char *str, size_t len, bool own)
        : _cstr(str),
          _len((uint32_t) len),
          _own((uint8_t) (own ? 1 : 0))
    {}

    zcstring::zcstring(char c, size_t n)
        : _str((char *) memory::alloc(n+1))
    {
        memset(_str, c, n);
        _str[n] = '\0';
    }

    zcstring::zcstring(zbuffer &b, bool own) {
        _len = (uint32_t) b.size();
        Ego._own = (uint8_t) ((own && (b.data() != nullptr)) ? 1 : 0);
        if (Ego._own) {
            _str = b.release();
        } else {
            _str = b.data();
        }
    }

    zcstring::zcstring(zcstring &&s) noexcept
            : _str(s._str),
              _len(s._len),
              _own(s._own),
              _hash(s._hash) {
        s._str = nullptr;
        s._len = 0;
        s._own = 0;
        s._hash = 0;
    }

    zcstring &zcstring::operator=(zcstring &&s) noexcept {
        _str = s._str;
        _len = s._len;
        _own = s._own;
        _hash = s._hash;

        s._str = nullptr;
        s._len = s._own = 0;
        s._hash = 0;

        return *this;
    }

    zcstring::zcstring(const zcstring &s)
            : _str(s._own ? utils::strndup(s._str, s._len) : s._str),
              _len(s._len),
              _own(s._own),
              _hash(s._hash) {}

    zcstring& zcstring::operator=(const zcstring &s) {
        _str = s._own ? utils::strndup(s._str, s._len) : s._str;
        _len = s._len;
        _own = s._own;
        _hash = s._hash;
        return *this;
    }

    zcstring zcstring::dup() const {
        if (_str == nullptr || _len == 0)
            return nullptr;
        return std::move(zcstring(utils::strndup(_str, _len), _len, true));
    }

    zcstring zcstring::peek() const {
        // this will return a dup of the string but as
        // just a reference or simple not owner
        return std::move(zcstring(_cstr, _len, false));
    }

    inline void zcstring::toupper() {
        for (int i = 0; i < _len; i++) {
            _str[i] = (char) ::toupper(_str[i]);
        }
    }

    inline void zcstring::tolower() {
        for (int i = 0; i < _len; i++) {
            _str[i] = (char) ::tolower(_str[i]);
        }
    }

    bool zcstring::empty() const {
        return _str == nullptr || _len == 0;
    }

    bool zcstring::operator==(const zcstring &s) const {
        if (_str != nullptr && s._str != nullptr) {
            return (_len == s._len) && ((_str == s._str) ||
                                       (strncmp(_str, s._str, _len) == 0));
        }
        return _str == s._str;
    }

    const char *zcstring::c_str(const char *nil) const {
        if (_cstr == nullptr || _len == 0)
            return nil;
        return _cstr;
    }

    void zcstring::decjv(iod::jdecit &it, zcstring &out) {
        // WARN!!! Copying data might not be efficiet but
        // necessary for decoding objects that are going outta
        // memory.
        it.eat('"');
        out = std::move(zcstring(it.start(), it.size(), false).dup());
        it.eat('"');
    }

    size_t zcstring::hash() const {
        if (_hash == 0) {
            hasher()(Ego);
        }
        return _hash;
    }

    void zcstring::out(wire &w) const {
        varint sz(Ego._len);
        w << sz;
        if (!w.push((uint8_t *) Ego._str, Ego._len)) {
            suil_error::create("pushing zcstring failed");
        }
    }

    void zcstring::in(wire &w) {
        zbuffer b;
        w >> b;
        Ego = std::move(zcstring(b));
    }

    zcstring::~zcstring() {
        if (_str && _own) {
            memory::free(_str);
        }
        _str = nullptr;
        _own = 0;
    }

    inline void hash_combine(size_t& seed, const char c) {
        seed ^= (size_t)c + 0x9e3779b9 + (seed<<6) + (seed>>2);
    }

    size_t hasher::operator()(const zcstring& s) const {
        zcstring& ss = (zcstring &) s;
        if (ss._hash == 0) {
            ss._hash = hash(s._cstr, s._len);
        }
        return s._hash;
    }

    inline size_t hasher::hash(const char *ptr, size_t len) const {
        std::size_t seed = 0;
        for(size_t i = 0; i < len; i++)
        {
            hash_combine(seed, ::toupper(ptr[i]));
        }
        return seed;
    }
}