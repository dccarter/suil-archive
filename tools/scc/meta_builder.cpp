//
// Created by dc on 13/12/18.
//

#include <suil/base.h>
#include "meta_builder.h"

namespace suil::scc {

    std::string MetaAttribute::toString() const
    {
        std::stringstream ss;
        bool first{true};
        for (auto& p: Parts) {
            if (!first) {
                ss << "::";
            }
            first = false;
            ss << p;
        }

        return ss.str();
    }

    MetaType MetaAstBuilder::build()
    {
        MetaType metaType;
        // skip the meta|> tag & meta keyword
        auto it = mpc_ast_traverse_next(trav);

        if (strcmp(it->tag, "attribs|>") == 0) {
            /* found attributes, load the attributes */
            it = build_Attributes(metaType.Attribs);
        }

        // expect type name
        Ego.expect(it, "ident|regex");
        metaType.Name = std::string(it->contents);

        // expect opening brace
        it = mpc_ast_traverse_next(trav);
        Ego.expect(it, "char", "{");

        // expect field declarations
        it = mpc_ast_traverse_next(trav);
        Ego.expect(it, "fieldDcls|>");
        it = build_Fields(metaType.Fields);

        // should be done parsing now
        Ego.expect(it, "char", "}");
        it = mpc_ast_traverse_next(trav);
        Ego.expect(it, "char", ";");
        return metaType;
    }

    mpc_ast_t * MetaAstBuilder::build_Fields(std::vector<MetaField>& fields)
    {
        auto it = mpc_ast_traverse_next(trav);
        do {
            MetaField metaField;
            Ego.expect(it, "field|>");
            it = mpc_ast_traverse_next(trav);
            if (strcmp(it->tag, "attribs|>") == 0) {
                // got attributes
                it = build_Attributes(metaField.Attribs);
            }

            // expect type
            if (strcmp(it->tag, "type|>") == 0) {
                // scoped type
                it = build_Type(metaField.Type);
            }
            else {
                // simple type
                Ego.expect(it, "type|ident|regex");
                metaField.Type = std::string(it->contents);
                it = mpc_ast_traverse_next(trav);
            }

            // expect field name
            Ego.expect(it, "ident|regex");
            metaField.Name = std::string(it->contents);

            // expect semi-colon
            it = mpc_ast_traverse_next(trav);
            Ego.expect(it, "char", ";");

            // field parsed
            fields.emplace_back(std::move(metaField));

            // move to next lexeme
            it = mpc_ast_traverse_next(trav);
        } while (it != nullptr && strcmp(it->contents, "}") != 0);

        return it;
    }

    mpc_ast_t* MetaAstBuilder::build_Attributes(std::vector<MetaAttribute> &attribs)
    {
        auto it = mpc_ast_traverse_next(trav);
        Ego.expect(it, "string", "[[");
        do {
            it = mpc_ast_traverse_next(trav);

            MetaAttribute attribute{};
            if (strcmp(it->tag, "attrib|>") == 0) {
                // attribute has parts
                it = build_AttributeParts(attribute.Parts);
            }
            else {
                /* simple attribute */
                Ego.expect(it, "attrib|ident|regex");
                attribute.Parts.emplace_back(it->contents);
                it = mpc_ast_traverse_next(trav);
            }
            attribute.Name = attribute.Parts.back();
            programBuilder.addSymbol(attribute.Name);

            attribs.emplace_back(attribute);
            // until all consumed
        } while (it != nullptr && strcmp(it->contents, ",") == 0);

        Ego.expect(it, "string", "]]");
        it = mpc_ast_traverse_next(trav);
        return it;
    }

    mpc_ast_t* MetaAstBuilder::build_AttributeParts(std::vector<std::string> &parts)
    {
        mpc_ast_t *it{nullptr};

        do {
            it = mpc_ast_traverse_next(trav);
            // expect a part
            Ego.expect(it, "ident|regex");
            parts.emplace_back(it->contents);

            it = mpc_ast_traverse_next(trav);
            // take all parts of the attribute
        } while (it != nullptr && strcmp(it->contents, "::") == 0);


        return it;
    }

    mpc_ast_t* MetaBuilder::build(mpc_ast_trav_t** trav)
    {
        MetaAstBuilder astBuilder(programBuilder, trav);
        Ego.artifact.emplace_back(astBuilder.build());
        // consume closing comma
        return mpc_ast_traverse_next(trav);
    }

    void MetaBuilder::clear()
    {
        Ego.artifact.clear();
    }
}