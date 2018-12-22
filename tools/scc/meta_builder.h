//
// Created by dc on 13/12/18.
//

#ifndef SUIL_META_PARSER_H
#define SUIL_META_PARSER_H

#include <string>
#include <vector>
#include "compiler.h"

namespace suil::scc {

    struct MetaAttribute {
        std::string              Name;
        std::vector<std::string> Parts;
        std::string toString() const;
    };

    struct MetaField {
        std::string                 Type;
        std::string                 Name;
        std::vector<MetaAttribute>  Attribs;
    };

    struct MetaType {
        std::string Name;
        std::vector<MetaAttribute>  Attribs;
        std::vector<MetaField>      Fields;
    };

    struct MetaAstBuilder : AstBuilder {
        MetaAstBuilder(ProgramBuilder& programBuilder, mpc_ast_trav_t **trav)
            : AstBuilder(programBuilder)
        { Ego.resetTrav(trav); }

        MetaType build();

    protected:
        virtual mpc_ast_t* build_Attributes(
                std::vector<MetaAttribute>& attribs);
        virtual mpc_ast_t* build_Fields(std::vector<MetaField>& fields);

        virtual mpc_ast_t* build_AttributeParts(std::vector<std::string>& parts);
    };

    struct MetaBuilder : ComponentBuilder {
        using Artifact = std::vector<MetaType>;

        MetaBuilder(ProgramBuilder& programBuilder)
            : ComponentBuilder(programBuilder)
        {}

        inline const Artifact& getArtifact() const {
            return Ego.artifact;
        }

        virtual mpc_ast_t* build(mpc_ast_trav_t** trav);

        void clear() override;

    private:
        Artifact        artifact;
    };

    struct MetaGenerator : ComponentGenerator {
        MetaGenerator(FileCompiler& compiler)
            : builder(compiler.getProgramBuilder())
        {
            compiler.registerGenerator("meta", this);
        }

        void appendHeaderContent(File &headerFile, int tab) override {}

        void appendCppContent(File &cppFile, int tab) override {}

        virtual ComponentBuilder &getBuilder() {
            return Ego.builder;
        }

    private:
        MetaBuilder builder;
    };
}
#endif //SUIL_META_PARSER_H
