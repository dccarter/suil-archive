//
// Created by dc on 25/12/18.
//

#ifndef SUIL_PARSER_H
#define SUIL_PARSER_H

#include "program.h"
#include "mpc.h"

namespace suil::scc {

    struct Parser {

        Parser();

        Parser(const Parser&) = delete;

        Parser&operator=(const Parser&) = delete;

        Parser(Parser&&) noexcept;

        Parser&operator=(Parser&&) noexcept;

        bool load(const char*grammarFile = nullptr);

        void repl();

        ProgramFile parseFile(const char* filename);

        ProgramFile parseString(const suil::String&& str);

        ~Parser();
    private:
        ProgramFile buildProgramFile(mpc_ast_t *root);
        void build_Includes(ProgramFile& out, mpc_ast_t *ast);
        void build_Symbols(ProgramFile& out, mpc_ast_t *ast);
        void build_Namespace(ProgramFile& out, mpc_ast_t *ast);
        void build_MetaType(ProgramFile& out, mpc_ast_t *ast);
        void build_RpcType(ProgramFile& out, mpc_ast_t *ast);
        void build_Attribs(std::vector<Attribute>& out, mpc_ast_t *ast);
        void build_Parameters(std::vector<Parameter>& out, mpc_ast_t* ast);
        std::string build_Scoped(Scoped& out, mpc_ast_t *ast);
        inline std::string build_Scoped(mpc_ast_t *ast) {
            Scoped sp{};
            return build_Scoped(sp, ast);
        }
        std::string build_Generic(mpc_ast_t* ast);

        Field  build_Field(mpc_ast_t *ast);
        Method build_Method(mpc_ast_t *ast);

    private:
#define __PARSER_DECLARE(id) mpc_parser_t * p##id
#include "parser.inc.h"
#undef __PARSER_DECLARE
    };
}

#endif //SUIL_PARSER_H
