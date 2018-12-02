//
// Created by dc on 27/11/18.
//

#ifndef SUIL_MUSTACHE_H
#define SUIL_MUSTACHE_H

#include <suil/buffer.h>
#include <suil/zstring.h>
#include <suil/logging.h>
#include "json.h"

namespace suil {

    define_log_tag(MUSTACHE);

    struct Mustache : LOGGER(MUSTACHE) {
        Mustache() = default;

        Mustache(const Mustache&) = delete;

        Mustache& operator=(const Mustache&) = delete;

        Mustache(Mustache&&) noexcept;

        Mustache&operator=(Mustache&&) noexcept;

        static Mustache fromFile(const char *path);

        static Mustache fromString(String&& str);

        inline void render(OBuffer& ob, const json::Object&& ctx) {
            Ego.render(ob, ctx);
        }

        inline String render(const json::Object&& ctx) {
            return Ego.render(ctx);
        }

        inline String render(const json::Object& ctx);

        void render(OBuffer& ob, const json::Object& ctx);

    private suil_ut:
        sptr(Mustache);

        friend struct MustacheCache;
        Mustache(String&& str)
            : mBody(std::move(str))
        {};

        enum ActionType {
            Ignore,
            Tag,
            UnEscapeTag,
            OpenBlock,
            CloseBlock,
            ElseBlock,
            Partial
        };

        struct Action {
            ActionType  tag;
            size_t      first;
            size_t      last;
            size_t      pos;
            Action(ActionType t, size_t f, size_t l, size_t p = 0)
                    : tag(t), first(f), last(l), pos(p)
            {}
        };

        struct Parser {
            char    *data{nullptr};
            size_t   size{0};
            size_t   pos{0};
            size_t   frag{0};

            Parser(String& s)
                : data(s.data()),
                  size(s.size())
            {}

            inline bool eof() {
                return pos >= size;
            }

            inline char next() {
                return data[pos++];
            }

            inline char peek(int off = 0) {
                size_t tmp{pos+off};
                return (char) ((tmp >=size)? '\0' : data[tmp]);
            }

            inline void eatSpace() {
                while (!eof() && isspace(data[pos]))
                    pos++;
            }

            inline strview substr(size_t first, size_t last) {
                if (first < size)
                    return strview{&data[first], MIN(size-first, last-first)};
                return strview{"invalid substr"};
            }

            inline strview tagName(const Action& a) {
                return strview{&data[a.first], a.last-a.first};
            }
        };
        using Fragment = std::pair<size_t,size_t>;
        using BlockPositions = std::vector<size_t>;

        void escape(OBuffer& out, const String& in);

        void render_Fragment(OBuffer& out, const Fragment& frag);
        void render_Internal(OBuffer& out, const json::Object& ctx, int& index, const String& stop);
        bool render_Tag(OBuffer& out, const Action& tag, const json::Object& ctx, int& index);

        void parse();
        void dump();
        void parser_ReadTag(Parser& p, BlockPositions& blocks);
        inline bool frag_Empty(const Fragment& frag) const {
            return frag.second == frag.first;
        }

        inline bool tag_Empty(const Action& act) const {
            return act.last == act.first;
        }

        inline String frag_String(const Fragment& frag) const {
            return frag_Empty(frag)?
                nullptr : String{&mBody.data()[frag.first], frag.second-frag.first, false};
        }

        inline String tag_String(const Action& act) const {
            return tag_Empty(act)?
                nullptr : String{&mBody.data()[act.first], act.last-act.first, false};
        }

        inline strview tag_View(const Action& act) const {
            return tag_Empty(act)?
                   nullptr : strview{&mBody.data()[act.first], act.last-act.first};
        }

        std::vector<Action>   mActions;
        std::vector<Fragment> mFragments;
        String                mBody{};
    };

    struct MustacheCache {
        Mustache& load(const String&& name);
        template<typename... Opts>
        void setup(Opts... args) {
            auto opts = iod::D(std::forward<Opts>(args)...);
            if (opts.has(var(root))) {
                // changing root directory
                root_Change(opts.get(var(root)));
            }
        }

        static MustacheCache& get() { return sCache; }
    private:

        MustacheCache() = default;
        MustacheCache(const MustacheCache&) = delete;
        MustacheCache&operator=(const MustacheCache&) = delete;
        MustacheCache(MustacheCache&&) = delete;
        MustacheCache&operator=(MustacheCache&&) = delete;

        struct CacheEntry {
            CacheEntry(String&& name, String&& path)
                : name(std::move(name)),
                  path(std::move(path))
            {}

            Mustache& reload();

            CacheEntry(CacheEntry&& o) noexcept;

            CacheEntry& operator=(CacheEntry&& o) noexcept;

            CacheEntry(const CacheEntry&) = delete;
            CacheEntry& operator=(const CacheEntry&) = delete;

            String       name;
            String       path;
            Mustache     tmpl;
            time_t       lastMod{0};
        };

        void root_Change(const String& to);

        String root{"./res/templates"};
        Map<CacheEntry>      mCached{};
        static MustacheCache sCache;
    };
}
#endif //SUIL_MUSTACHE_H
