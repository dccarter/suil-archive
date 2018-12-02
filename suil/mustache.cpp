//
// Created by dc on 27/11/18.
//

#include "file.h"
#include "mustache.h"

namespace suil {

    Mustache::Mustache(suil::Mustache &&o) noexcept
        : mFragments(std::move(o.mFragments)),
          mActions(std::move(o.mActions)),
          mBody(o.mBody)
    {}

    Mustache& Mustache::operator=(suil::Mustache &&o) noexcept{
        if (this != &o) {
            mFragments = std::move(o.mFragments);
            mActions = std::move(o.mActions);
            mBody = std::move(o.mBody);
        }
        return Ego;
    }

    void Mustache::parser_ReadTag(Parser& p, BlockPositions& blocks) {
        // read parser tag
        p.eatSpace();
        auto tok = p.peek();

        auto __toLastIndex = [](Parser& pp, const char* chs = "_") -> std::pair<size_t,size_t> {
            pp.eatSpace();
            size_t s = pp.pos, e = pp.pos;
            char n{pp.next()};
            if (!isalpha(n) && n != '_') {
                // implementation supports tags that starts with alpha/_
                throw Exception::create("tags can only start with letters or _, at position '",
                        s, ", ^^^", pp.substr(s-3, MIN(10, pp.size-s)));
            }

            while(!pp.eof() && (isalnum(n = pp.peek()) || strchr(chs, n) != nullptr))
                pp.next();
            e = pp.pos;
            pp.eatSpace();
            if (pp.next() != '}' || pp.next() != '}') {
                // error expected '}}'
                throw Exception::create("unclosed block close tag at position '",
                                        s-2, "', ^^^", pp.substr(s, pp.pos));
            }
            if (e == s) {
                // error empty block
                throw Exception::create("empty tags not supported at position '", s-2, "' ^^^",
                            pp.substr(s-2, pp.pos));
            }

            return std::make_pair(s, e);
        };

        switch (tok) {
            case '#': {
                p.next();
                auto[start, last] = __toLastIndex(p);
                blocks.emplace_back(mActions.size());
                mActions.emplace_back(OpenBlock, start, last, 0);
                p.frag = p.pos;
                break;
            }
            case '/': {
                if (blocks.empty()) {
                    // unexpected close tag
                    throw Exception::create("unexpected close tag at position '",
                            p.pos, "', ^^^", p.substr(p.pos-1, MIN(10, p.size-p.pos)));
                }
                auto openTag = p.tagName(mActions[blocks.back()]);
                p.next();
                auto [start, last] = __toLastIndex(p);
                Action action{CloseBlock, start, last, blocks.back()};
                auto closeTag = p.tagName(action);
                if (openTag != closeTag) {
                    // unexpected close tag,
                    throw Exception::create("unexpected block close tag at position '",
                                            start, "', got '", closeTag, "', expecting '",
                                            openTag, "'");
                }
                mActions.push_back(action);
                blocks.pop_back();
                p.frag = p.pos;
                break;
            }
            case '^': /* ELSE */{
                p.next();
                auto [start, last] = __toLastIndex(p);
                blocks.emplace_back(mActions.size());
                mActions.emplace_back(ElseBlock, start, last, 0);
                p.frag = p.pos;
                break;
            }
            case '!': /* DO NOTHING */ {
                p.next();
                size_t start{p.pos}, end{p.pos};
                while (!p.eof() && p.peek() != '}' && p.peek(1) != '}')
                    p.next();
                if (p.next() != '}' && p.peek() != '}') {
                    // comment not closed
                    throw Exception::create("ignore tag not closed at position '",
                            start, "', ^^^", p.substr(start, MIN(20, p.pos-start)));
                }
                mActions.emplace_back(Ignore, start, p.pos, 0);
                p.next();p.next();
                p.frag = p.pos;
                break;
            }

            case '>': /* PARTIAL */ {
                p.next();
                auto [start, last] = __toLastIndex(p, "/._");
                mActions.emplace_back(Partial, start, last, 0);
                p.frag = p.pos;
                break;
            }

            case '{': /* UN ESCAPE */ {
                p.next();
                auto [start, last] = __toLastIndex(p);
                tok = p.next();
                if (tok != '}') {
                    // un-matched un-escape tag
                    throw Exception::create("unmatched un-escape tag at position '",
                            start, "', ^^^", p.substr(start-2, p.pos));
                }
                mActions.emplace_back(UnEscapeTag, start, last, 0);
                p.frag = p.pos;
                break;
            }

            case '&': /* UN ESCAPE */ {
                p.next();
                auto [start, last] = __toLastIndex(p);
                mActions.emplace_back(UnEscapeTag, start, last, 0);
                p.frag = p.pos;
                break;
            }

            case '=': /* CHANGE DELIMITER */ {
                throw Exception::create(
                        "changing delimiter currently not supported, at positon '", p.pos, "'");
                break;
            }

            default: /* NORMAL TAG */{
                auto [start, last] = __toLastIndex(p);
                mActions.emplace_back(Tag, start, last, 0);
                p.frag = p.pos;
                break;
            }
        }
    }

    void Mustache::parse()
    {
        if (mBody.empty()) {
            // cannot parse empty body
            idebug("cannot parse an empty mustache template");
            return;
        }

        struct Parser p(mBody);
        BlockPositions blocks;
        while (!p.eof()) {
            // Parse until end
            char tok = p.next(), next = p.peek();
            if (tok == '{' && next == '{') {
                // opening fragment
                mFragments.emplace_back(p.frag, p.pos-1);
                p.next();
                // read body tag
                parser_ReadTag(p, blocks);
            }
        }

        if (!blocks.empty()) {
            // un-terminated blocks
            std::stringstream ss;
            ss << "template contains non-terminated blocks";
            for (auto &b : blocks)
                ss << ", '" << p.tagName(mActions[b]) << "'";
            throw Exception::create(ss.str());
        }

        if (p.pos != p.frag) {
            // append last fragment
            mFragments.emplace_back(p.frag, p.pos);
        }

        if (!mActions.empty()) {
            for (int i = 0; i < mActions.size() - 1; i++) {
                const auto &b = mActions[i];
                if (b.tag == ElseBlock)
                    if (mActions[i + 1].tag != CloseBlock)
                        throw Exception::create("inverted section cannot have tags {",
                                                tag_View(b), " ", b.first, "}");

            }
        }
    }

    void Mustache::dump()
    {
        OBuffer ob{mBody.size()*2};
        for (size_t i = 0; i < mFragments.size(); i++) {
            auto Frag = mFragments[i];
            auto frag = frag_String(Frag);
            ob << "FRAG: ";
            if (!frag.empty())
                ob << frag << "\n";
            else
                ob << "{" << Frag.first << ", " << Frag.second << "}\n";
            if (i < mActions.size()) {
                // print action
                auto act = tag_String(mActions[i]);
                ob << "\tACTION: " << act << "\n";
            }
        }

        write(STDOUT_FILENO, ob.data(), ob.size());
        fflush(stdout);
    }

    void Mustache::escape(suil::OBuffer &out, const suil::String &in)
    {
        out.reserve(in.size()*2);
        for (auto c : in) {
            // escape every character of input
            switch (c) {
                case '&' : out << "&amp;"; break;
                case '<' : out << "&lt;"; break;
                case '>' : out << "&gt;"; break;
                case '"' : out << "&quot;"; break;
                case '\'' : out << "&#39;"; break;
                case '/' : out << "&#x2f;"; break;
                default:
                    out.append(&c, 1);
            }
        }
    }

    void Mustache::render_Fragment(suil::OBuffer &out, const suil::Mustache::Fragment &frag)
    {
        if (!frag_Empty(frag)) {
            // only render non-empty fragments
            out << frag_String(frag);
        }
    }

    bool Mustache::render_Tag(suil::OBuffer &out, const Action& act, const suil::json::Object &ctx, int& index)
    {
        auto name{tag_String(act)};

        switch (act.tag) {
            case Ignore:
                /* ignore comments*/
                break;
            case Partial: {
                /* load a and render template */
                Mustache& m = MustacheCache::get().load(tag_String(act));
                m.render(out, ctx);
                break;
            }
            case UnEscapeTag:
            case Tag: {
                /* render normal tag */
                auto val = ctx[name.peek()];
                switch(val.type()) {
                    /* render base on type */
                    case JsonTag::JSON_BOOL: {
                        /* render boolean */
                        out << (((bool) val)? "true" : "false");
                        break;
                    }
                    case JsonTag::JSON_NUMBER: {
                        /* render number */
                        auto d = (double) val;
                        if (d != (int) d)
                            out << d;
                        else
                            out << (int) d;
                        break;
                    }
                    case JsonTag::JSON_STRING: {
                        /* render string */
                        if (act.tag == UnEscapeTag)
                            out << (String) val;
                        else
                            escape(out, val);
                        break;
                    }
                    default:
                        /* rendering failure */
                        throw Exception::create(
                                "rendering template tag {", tag_View(act), "@", act.pos, "}");
                }
                break;
            }

            case ElseBlock: {
                /* render Else Block */
                auto val = ctx[name.peek()];
                if (val.empty()) {
                    /* rendered only when */
                    render_Fragment(out, mFragments[++index]);
                }
                else {
                    /* skip fragment */
                    index++;
                }
                break;
            }

            case OpenBlock: {
                /* render an open block, easy, recursive */
                auto val = ctx[name.peek()];
                auto id = index;
                if (val.empty()) {
                    /* empty value, ignore block */
                    for (; id < mActions.size(); id++)
                        if (tag_String(mActions[id]) == name && mActions[id].tag == CloseBlock)
                            break;
                }
                else if (val.isArray()) {
                    /* render block multiple times */
                    for (const auto [_, e] : val) {
                        /* recursively render, retaining the index */
                        id = index + 1;
                        render_Internal(out, e, id, name);
                    }
                }
                else {
                    /* render block once */
                    render_Internal(out, val, id, name);
                }
                index = id;
                break;
            }

            case CloseBlock:
                /* closing block, do nothing */
                break;

            default:
                /* UNREACHABLE*/
                throw Exception::create("unknown tag type: ", act.tag);
        }
    }

    void Mustache::render_Internal(
            suil::OBuffer &out, const suil::json::Object &ctx, int &index, const suil::String &stop)
    {
        for (;index < mFragments.size(); index++) {
            /* enumerate all fragments and render */
            render_Fragment(out, mFragments[index]);
            if (index < mActions.size()) {
                /* a fragment is always followed by a tag */
                auto act = mActions[index];
                if (!stop.empty() && tag_String(act) == stop)
                    break;
                render_Tag(out, act, ctx, index);
            }
        }
    }

    void Mustache::render(suil::OBuffer &ob, const suil::json::Object &ctx)
    {
        int index{0};
        String stop{nullptr};
        render_Internal(ob, ctx, index, stop);
    }

    String Mustache::render(const suil::json::Object &ctx) {
        OBuffer ob{mBody.size()+(mBody.size()>>1)};
        Ego.render(ob, ctx);
        return String{ob};
    }

    Mustache Mustache::fromString(suil::String &&str)
    {
        Mustache m(std::move(str));
        m.parse();
        return std::move(m);
    }

    Mustache Mustache::fromFile(const char *path)
    {
        return Mustache::fromString(utils::fs::readall(path));
    }

    MustacheCache MustacheCache::sCache{};

    MustacheCache::CacheEntry::CacheEntry(CacheEntry &&o) noexcept
        : name(std::move(o.name)),
          path(std::move(o.path)),
          tmpl(std::move(o.tmpl))
    {}

    MustacheCache::CacheEntry& MustacheCache::CacheEntry::operator=(CacheEntry &&o) noexcept
    {
        if (this != &o) {
            name = std::move(o.name);
            path = std::move(o.path);
            tmpl = std::move(o.tmpl);
        }
        return Ego;
    }

    Mustache& MustacheCache::CacheEntry::reload()
    {
        if (!utils::fs::exists(path())) {
            // template required to exist
            throw Exception::create("attempt to reload template '",
                                    (strview) path(), "' which does not exist");
        }

        struct stat st{};
        stat(path.data(), &st);
        auto mod    = (time_t) st.st_mtim.tv_sec;
        if (mod != lastMod) {
            // template has been modified, reload
            lastMod = mod;
            tmpl = Mustache::fromFile(path());
        }
        return tmpl;
    }

    Mustache& MustacheCache::load(const suil::String &&name)
    {
        auto it = Ego.mCached.find(name);
        if (it != Ego.mCached.end()) {
            // found on cache, reload cache if possible
            return it->second.reload();
        }
        // create new cache entry
        CacheEntry entry{name.dup(), utils::catstr(Ego.root, "/", name)};
        entry.reload();
        auto tmp = Ego.mCached.emplace(entry.name.peek(), std::move(entry));
        return tmp.first->second.reload();
    }
}

#ifdef unit_test
#include <catch/catch.hpp>

using namespace suil;

TEST_CASE("Mustache tests", "[template][mustache]")
{
    using Expected = std::pair<Mustache::ActionType, const char*>;
    auto __CheckTags = [](const Mustache& m, const std::vector<Expected> expected) {
        /* same size */
        REQUIRE((m.mActions.size() == expected.size()));
        int i{0};
        for (auto& act: m.mActions) {
            // verify each action
            const auto& exp = expected[i++];
            REQUIRE(act.tag == exp.first);
            REQUIRE((m.tag_String(act) == String{exp.second}));
        }
    };

    auto __CheckFrags = [](const Mustache& m, std::vector<const char*> frags) {
        /* same size */
        REQUIRE((m.mFragments.size() == frags.size()));
        int i{0};
        for (auto& frag: m.mFragments) {
            // verify each action
            const auto& exp = frags[i++];
            REQUIRE((m.frag_String(frag) == String{exp}));
        }
    };

    SECTION("Parsing mustache templates") {
        /* tests parsing a valid mustache template */
        auto __CheckParse = [&__CheckFrags, &__CheckTags](
                const char* input, std::vector<const char*> frags, const std::vector<Expected> tags) {
            Mustache m;
            REQUIRE_NOTHROW((m = Mustache::fromString(String{input})));
            __CheckTags(m, tags);
            __CheckFrags(m, frags);
        };

        WHEN("Parsing valid templates") {
            // check normal tag
            __CheckParse("Hello World",                 /* not tag */
                         {"Hello World"}, {});
            __CheckParse("{{name}}",                    /* tag without fragments */
                         {nullptr}, {{Mustache::Tag, "name"}});
            __CheckParse("{{name}}{{lastname}}",        /* tags without fragments */
                         {nullptr, nullptr}, {{Mustache::Tag, "name"},
                                              {Mustache::Tag, "lastname"}});
            __CheckParse("{{    world   }}",
                         {nullptr}, {{Mustache::Tag, "world"}});
            __CheckParse("Hello {{world}}",
                         {"Hello "}, {{Mustache::Tag, "world"}});
            __CheckParse("Hello {{world}}, its awesome",
                         {"Hello ", ", its awesome"}, {{Mustache::Tag, "world"}});
            __CheckParse("Hello {{name}}, welcome to {{world}} of {{planet}}",
                         {"Hello ", ", welcome to ", " of "},
                         {{Mustache::Tag, "name"},
                          {Mustache::Tag, "world"},
                          {Mustache::Tag, "planet"}});

            // check un-escape tag
            __CheckParse("{{{name}}}",                   /* tag without fragments */
                         {nullptr}, {{Mustache::UnEscapeTag, "name"}});
            __CheckParse("{{&name}}",                    /* tag without fragments */
                         {nullptr}, {{Mustache::UnEscapeTag, "name"}});

            // check OpenBlock tag
            __CheckParse("{{#name}}{{/name}}",           /* empty block */
                         {nullptr, nullptr},
                         {{Mustache::OpenBlock,  "name"},
                          {Mustache::CloseBlock, "name"}});
            __CheckParse("{{#name}}Carter{{/name}}",     /* fragment between block */
                         {nullptr, "Carter"},
                         {{Mustache::OpenBlock,  "name"},
                          {Mustache::CloseBlock, "name"}});
            __CheckParse("{{#name}} frag1 {{tag1}} frag2 {{tag2}} frag3 {{/name}}",     /* fragment between block */
                         {nullptr, " frag1 ", " frag2 ", " frag3 "},
                         {{Mustache::OpenBlock,  "name"},
                          {Mustache::Tag,        "tag1"},
                          {Mustache::Tag,        "tag2"},
                          {Mustache::CloseBlock, "name"}});

            // check ElseBlock tag
            __CheckParse("{{^name}}{{/name}}",           /* empty block */
                         {nullptr, nullptr},
                         {{Mustache::ElseBlock,  "name"},
                          {Mustache::CloseBlock, "name"}});
            // check ElseBlock tag
            __CheckParse("{{ ^name}}{{\t/name\t}}",           /* empty block */
                         {nullptr, nullptr},
                         {{Mustache::ElseBlock,  "name"},
                          {Mustache::CloseBlock, "name"}});

            __CheckParse("{{^name}}Carter{{/name}}",     /* fragment between block */
                         {nullptr, "Carter"},
                         {{Mustache::ElseBlock,  "name"},
                          {Mustache::CloseBlock, "name"}});
            __CheckParse("{{#name}} frag1 {{tag1}} frag2 {{tag2}} frag3 {{/name}}",     /* fragment between block */
                         {nullptr, " frag1 ", " frag2 ", " frag3 "},
                         {{Mustache::OpenBlock,  "name"},
                          {Mustache::Tag,        "tag1"},
                          {Mustache::Tag,        "tag2"},
                          {Mustache::CloseBlock, "name"}});

            // check Ignore tag
            __CheckParse("{{!name}}{{!name}}",           /* Ignored block */
                         {nullptr, nullptr},
                         {{Mustache::Ignore, "name"},
                          {Mustache::Ignore, "name"}});
            __CheckParse("{{!//comment?\n/* yes*/}}{{!name}}",           /* can have any characters, except delimiter */
                         {nullptr, nullptr},
                         {{Mustache::Ignore, "//comment?\n/* yes*/"},
                          {Mustache::Ignore, "name"}});


            // check Partial tag
            __CheckParse("{{> name}}{{\t>\tname}}",           /* Partial */
                         {nullptr, nullptr},
                         {{Mustache::Partial,  "name"},
                          {Mustache::Partial, "name"}});
            // check Partial tag
            __CheckParse("{{> index.html}}{{>includes/_header.html}}",           /* file names */
                         {nullptr, nullptr},
                         {{Mustache::Partial,  "index.html"},
                          {Mustache::Partial, "includes/_header.html"}});
        }

        WHEN("Parsing invalid templates") {
            // test parsing of invalid templates
            REQUIRE_THROWS(Mustache::fromString("{{tag}"));   /* invalid closing tag */
            REQUIRE_THROWS(Mustache::fromString("{{2tag}}")); /* tags must start with letters or '_' */
            REQUIRE_THROWS(Mustache::fromString("{{_tagPrefix Postfix}}")); /* there can be no spaces between tag names */
            REQUIRE_THROWS(Mustache::fromString("{{tag%&}}")); /* characters, numbers and underscore only supported in tag name */
            REQUIRE_THROWS(Mustache::fromString("{{/tag}}"));  /* floating block close tag not supported  */
            REQUIRE_THROWS(Mustache::fromString("{{#tag}}"));  /* unclosed block tag to supported */
            REQUIRE_THROWS(Mustache::fromString("{{^tag}}"));  /* unclosed block tag to supported */
            /* inner blocks must be closed before outter blocks */
            REQUIRE_THROWS(Mustache::fromString("{{#tag}}{{^tag2}}{{/tag}}{{/tag2}}"));
            /* all blocks must be closed */
            REQUIRE_THROWS(Mustache::fromString("{{#tag}}{{^tag2}}{{/tag2}}{{tag}}"));
            /* currently chnage delimiter block is not supported */
            REQUIRE_THROWS(Mustache::fromString("{{=<% %>=}}"));
        }
    }

    SECTION("Rendering templates") {
        /* Test rendering mutache templates */
        WHEN("Rendering with valid context") {
            /* rendering normal tags */
            Mustache m = Mustache::fromString("Hello {{name}}");
            auto rr = m.render(json::Object(json::Obj, "name", "Carter"));
            REQUIRE(rr == String{"Hello Carter"});
            m = Mustache::fromString("Hello {{name}}, welcome to {{planet}}!");
            rr = m.render(json::Object(json::Obj,
                                       "name", "Carter",
                                       "planet", "Mars"));
            REQUIRE(rr == String{"Hello Carter, welcome to Mars!"});
            /* different variable types */
            m = Mustache::fromString("Test variable types {{type}}={{val}}");
            rr = m.render(json::Object(json::Obj,
                    "type", "int", "value", 6));
            REQUIRE(rr == String{"Test variable types int=6"});
            rr = m.render(json::Object(json::Obj,
                                       "type", "float", "value", 5.66));
            REQUIRE(rr == String{"Test variable types float=5.660000"});
            rr = m.render(json::Object(json::Obj,
                                       "type", "bool", "value", true));
            REQUIRE(rr == String{"Test variable types bool=true"});
            rr = m.render(json::Object(json::Obj,
                                       "type", "string", "value", "World"));
            REQUIRE(rr == String{"Test variable types string=World"});
            /* template referencing same key */
            m = Mustache::fromString("{{name}} is {{name}}, {{name}}'s age is {{age}}, {{age}} hah!");
            rr = m.render(json::Object(json::Obj,
                                       "name", "Carter", "age", 45));
            REQUIRE(rr == String{"Carter is Carter, Carter's age is 45, 45 hah!"});

            /* Parsing else block */
            m = Mustache::fromString("Say {{^cond}}Hello{{/cond}}");
            rr = m.render(json::Object(json::Obj, "cond", false));
            REQUIRE(rr == String{"Say Hello"});
            rr = m.render(json::Object(json::Obj, "cond", true));
            REQUIRE(rr == String{"Say "});
            m = Mustache::fromString("Your name is {{^cond}}Carter{{/cond}}");
            rr = m.render(json::Object(json::Obj,"cond", true));
            REQUIRE(rr == String{"Your name is "});
            rr = m.render(json::Object(json::Obj));
            REQUIRE(rr == String{"Your name is Carter"});
            rr = m.render(json::Object(json::Obj, "cond", nullptr));
            REQUIRE(rr == String{"Your name is Carter"});

            /* blocks */
            m = Mustache::fromString("{{#data}}empty{{/data}}");
            rr = m.render(json::Object(json::Obj, "data", true));
            REQUIRE(rr == "empty");
            rr = m.render(json::Object(json::Obj, "data", false));
            REQUIRE(rr == "");
            rr = m.render(json::Object(json::Obj, "data", 5));
            REQUIRE(rr == "empty");
            rr = m.render(json::Object(json::Obj, "data", json::Object(json::Arr)));
            REQUIRE(rr == "");
            rr = m.render(json::Object(json::Obj,
                    "data", json::Object(json::Arr, 0)));
            REQUIRE(rr == "empty");
            rr = m.render(json::Object(json::Obj,
                                       "data", json::Object(json::Arr, 0, true)));
            REQUIRE(rr == "emptyempty");
            rr = m.render(json::Object(json::Obj,
                                       "data", json::Object(json::Arr, 0, "true", true, false)));
            REQUIRE(rr == "emptyemptyemptyempty");

            /* more blocks */
            m = Mustache::fromString("{{#numbers}}The number is {{number}}!\n{{/numbers}}");
            rr = m.render(json::Object(json::Obj, "numbers", json::Object(json::Arr,
                            json::Object(json::Obj, "number", 1)
                    )));
            REQUIRE(rr == "The number is 1!\n");
            rr = m.render(json::Object(json::Obj, "numbers", json::Object(json::Arr,
                                json::Object(json::Obj, "number", 1),
                                json::Object(json::Obj, "number", 2),
                                json::Object(json::Obj, "number", 3),
                                json::Object(json::Obj, "number", 4))));
            REQUIRE(rr == "The number is 1!\nThe number is 2!\nThe number is 3!\nThe number is 4!\n");
            m = Mustache::fromString("{{#users}}Name: {{name}}, Email: {{email}}\n{{/users}}");
            rr = m.render(json::Object(json::Obj, "users", json::Object(json::Arr,
                              json::Object(json::Obj, "name",   "Holly", "email", "holly@gmail.com"),
                              json::Object(json::Obj, "name",   "Molly", "email", "molly@gmail.com"))));
            REQUIRE(rr == "Name: Holly, Email: holly@gmail.com\nName: Molly, Email: molly@gmail.com\n");
        }

    }
}

#endif