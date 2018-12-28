//
// Created by dc on 25/12/18.
//

#include "suil/file.h"
#include "suil/console.h"

#include "parser.h"

namespace suil::scc {

    static const char *sc_Grammar =
R"(
ident       : /[a-zA-Z_][a-zA-Z0-9_]*/ | /[a-zA-Z_]/;
scoped      : <ident> ("::" <ident>)* ;
generic     : <scoped> '<' <generic>? (',' <generic>)* '>'
            | <scoped> ;
attribs     : "[[" <scoped> (',' <scoped>)* "]]" ;
field       : <attribs>? <generic> <ident> ;
fields      : (<field> ';')+ ;
param       : "const"? <generic> ("&&"|"&")? <ident> ;
params      : <param>? (',' <param>)* ;
method      : <attribs>? <generic> <ident> '(' <params>? ')' ;
methods     : (<method> ';')+ ;
meta        : "meta" <attribs>? <ident> '{' <fields> '}' ;
rpc         : ("srpc"|"jrpc"|"service")  <attribs>? <ident> '{' <methods> '}' ;
types       : ((<meta>|<rpc>) ';')+ ;
namespace   : "namespace" <scoped> '{' <types> '}' ;
symbol      : "symbol" '(' <ident> ')' ;
symbols     : <symbol>+ ;
include     : "#include" (('"' /(\\.|[^"])+/ '"') | ('<' /(\\.|[^>])+/ '>' )) ;
includes    : <include>+ ;
program     : <includes>? <symbols>? <namespace> ;
)";


    static inline void parserError(mpc_err_t* err)
    {
        // error parsing given grammar
        char *str = mpc_err_string(err);
        console::error("%s", str);
        free(str);
        mpc_err_delete(err);
    }

    Parser::Parser()
    {
#define __PARSER_INITIALIZE(id, name) p##id = mpc_new(name)
#include "parser.inc.h"
#undef  __PARSER_INITIALIZE
    }

#define __USE_PARSER(id) p##id

    Parser::~Parser()
    {
        if (__USE_PARSER(Program) != nullptr) {
            // cleanup parser
            mpc_cleanup(count(),
#include "parser.inc.h"
            );
#define __PARSER_ASSIGN(id) nullptr
#include "parser.inc.h"
#undef __PARSER_ASSIGN
        }
    }

    Parser::Parser(suil::scc::Parser &&other) noexcept
    {
#define __PARSER_ASSIGN(id) other. __USE_PARSER(id)
#include "parser.inc.h"
#undef __PARSER_ASSIGN
        other. __USE_PARSER(Program) = nullptr;
    }

    Parser& Parser::operator=(suil::scc::Parser &&other) noexcept
    {
        if (this != &other) {
#define __PARSER_ASSIGN(id) other. __USE_PARSER(id)
#include    "parser.inc.h"
#undef __PARSER_ASSIGN
            other. __USE_PARSER(Program) = nullptr;
        }
        return Ego;
    }

    bool Parser::load(const char *grammarFile)
    {
        mpc_err_t *err{nullptr};
        if (grammarFile && utils::fs::exists(grammarFile)) {
            // grammar file used, parse grammar file
            err = mpca_lang_contents(MPCA_LANG_DEFAULT,
                    grammarFile,
#include "parser.inc.h"
            );
        }
        else {
            // parse internal grammar
            err = mpca_lang(MPCA_LANG_DEFAULT,
                    sc_Grammar,
#include "parser.inc.h"
            );
        }

        if (err != nullptr) {
            // loading grammar failed
            parserError(err);
            return false;
        }

        return true;
    }

    void Parser::repl()
    {
        while (true) {
            char *line;
            size_t len{0};
            console::info("> ");
            if (getline(&line, &len, stdin) < 0) {
                // error reading input
                console::warn("error: failed to read input: %s", errno_s);
                break;
            }

            if (strncmp("exit", line, 4) == 0) {
                /* exit requested */
                free(line);
                break;
            }

            mpc_result_t r;
            if (mpc_parse("<stdin>", line, pProgram, &r)) {
                // parsed successfully
                mpc_ast_print((mpc_ast_t *) r.output);
                mpc_ast_delete((mpc_ast_t *) r.output);
            }
            else {
                // parsing line failed
                parserError(r.error);
            }
            free(line);
        };
    }

    ProgramFile Parser::parseFile(const char *filename)
    {
        mpc_result_t res;
        if (!mpc_parse_contents(filename, pProgram, &res)) {
            // parsing failed
            parserError(res.error);
            exit(-1);
        }

        ProgramFile pf = buildProgramFile((mpc_ast_t *) res.output);
        return std::move(pf);
    }

    ProgramFile Parser::parseString(const suil::String &&str)
    {
        mpc_result_t res;
        if (!mpc_parse("<string>", str(), pProgram, &res)) {
            // parsing failed
            parserError(res.error);
            exit(-1);
        }

        ProgramFile pf = buildProgramFile((mpc_ast_t *) res.output);
        mpc_ast_delete((mpc_ast_t *) res.output);
        return std::move(pf);
    }

    ProgramFile Parser::buildProgramFile(mpc_ast_t *root)
    {
        // mpc_ast_print(root);
        try {
            ProgramFile pf;
            for (int i = 0; i < root->children_num; i++) {
                // iterate all root children
                auto child = root->children[i];
                if (strncmp("includes", child->tag, 8) == 0) {
                    // parse file includes
                    build_Includes(pf, child);
                }
                else if (strncmp("symbols", child->tag, 7) == 0) {
                    // parse file symbols
                    build_Symbols(pf, child);
                }
                else {
                    // should be namespace
                    build_Namespace(pf, child);
                }
            }
            // program file successfully built
            return std::move(pf);
        }
        catch (...) {
            // un handled exception should cause compiler to exit
            console::error("error: %s\n", Exception::fromCurrent().what());
            exit(-1);
        }
    }

    void Parser::build_Includes(suil::scc::ProgramFile &out, mpc_ast_t *ast)
    {
        auto build_Include = [&out](mpc_ast_t *inc) -> std::string {
            // include has for children
            std::stringstream ss;
            ss << inc->children[0]->contents << " "
               << inc->children[1]->contents
               << inc->children[2]->contents
               << inc->children[3]->contents;

            return ss.str();
        };

        if (strcmp(ast->tag, "includes|>") == 0) {
            // multiple includes
            for (int i = 0; i < ast->children_num; i++) {
                // enumerate and parse each included
                out.Includes.push_back(build_Include(ast->children[i]));
            }
        }
        else {
            // single include, parse it
            out.Includes.push_back(build_Include(ast));
        }
    }

    void Parser::build_Symbols(suil::scc::ProgramFile &out, mpc_ast_t *ast)
    {
        if (strcmp(ast->tag, "symbols|>") == 0) {
            // multiple includes
            for (auto i = 0; i < ast->children_num; i++) {
                // enumerate and parse each included
                auto child = ast->children[i];
                out.Symbols.emplace_back(child->children[2]->contents);
            }
        }
        else {
            // single include, parse it
            out.Symbols.emplace_back(ast->children[2]->contents);
        }
    }

    void Parser::build_Namespace(suil::scc::ProgramFile &out, mpc_ast_t *ast)
    {
        // get name of the namespace
        out.Namespace = std::string(ast->children[1]->contents);
        // enumerate the and parse the types
        auto tpsAst = ast->children[3];
        for (int i = 0; i < tpsAst->children_num-1; i++) {
            // enumerate and build all the types represented
            auto tpAst = tpsAst->children[i];
            if (strcmp(";", tpAst->contents) == 0) {
                // ignore semi-colons
                continue;
            }

            if (strcmp("meta|>", tpAst->tag) == 0) {
                // this is meta type
                build_MetaType(out, tpAst);
            }
            else {
                // the only other type is rpc
                build_RpcType(out, tpAst);
            }
        }
    }

    void Parser::build_MetaType(ProgramFile &out, mpc_ast_t *ast)
    {
        int offset = 1;
        MetaType mt{};
        if (strcmp("attribs|>", ast->children[offset]->tag) == 0) {
            // meta-type has attributes, parse them
            build_Attribs(mt.Attribs, ast->children[offset++]);
        }
        // get the name of the type
        mt.Name = std::string(ast->children[offset++]->contents);
        auto fields = ast->children[++offset];
        for (int i = 0; i < fields->children_num; i++) {
            // build all fields of the current type
            if (strcmp(";", fields->children[i]->contents) == 0) {
                // ignore all semi-colons
                continue;
            }

            mt.Fields.emplace_back(build_Field(fields->children[i]));
        }
        out.MetaTypes.push_back(std::move(mt));
    }

    void Parser::build_RpcType(suil::scc::ProgramFile &out, mpc_ast_t *ast)
    {
        RpcType rt{};
        int offset = 0;
        rt.Kind = std::string(ast->children[offset++]->contents);

        if (strcmp("attribs|>", ast->children[offset]->tag) == 0) {
            // meta-type has attributes, parse them
            build_Attribs(rt.Attribs, ast->children[offset++]);
        }
        // get the name of the type
        rt.Name = std::string(ast->children[offset++]->contents);
        auto methods = ast->children[++offset];
        for (int i = 0; i < methods->children_num; i++) {
            // build all fields of the current type
            if (strcmp(";", methods->children[i]->contents) == 0) {
                // ignore all semi-colons
                continue;
            }

            rt.Methods.push_back(build_Method(methods->children[i]));
        }

        out.Services.push_back(std::move(rt));
    }

    std::string Parser::build_Scoped(suil::scc::Scoped &out, mpc_ast_t *ast)
    {
        if (strstr(ast->tag, "scoped|>") != nullptr) {
            // should be reassembled
            std::stringstream ss;
            for (int i = 0; i < ast->children_num; i++) {
                // append to scoped output
                ss << ast->children[i]->contents;

                if (strcmp("::", ast->children[i]->contents) == 0) {
                    // ignore scope operator
                    continue;
                }

                out.Parts.emplace_back(ast->children[i]->contents);
            }
            out.Resolved = ss.str();
        }
        else {
            // straight up string
            out.Parts.emplace_back(ast->contents);
            out.Resolved = std::string(ast->contents);
        }

        return out.Resolved;
    }

    void Parser::build_Attribs(std::vector<Attribute> &out, mpc_ast_t *ast)
    {
        for (int i = 1; i < ast->children_num-1; i++) {
            // attribute listing starts at child #1
            if (strcmp(",", ast->children[i]->contents) == 0) {
                // ignore comma's (,)
                continue;
            }

            Attribute atb{};
            build_Scoped(atb, ast->children[i]);

            out.push_back(std::move(atb));
        }
    }

    std::string Parser::build_Generic(mpc_ast_t *ast)
    {
        if (strcmp("generic|>", ast->tag) == 0) {
            // generic, meta type
            std::stringstream ss;
            int i = 0;
            ss << build_Scoped(ast->children[i++]);
            ss << ast->children[i++]->contents;
            for (; i < ast->children_num-1; i++) {
                // process rest of entries
                auto gtAst = ast->children[i];
                if (strcmp(",", gtAst->contents) == 0) {
                    // append comma's
                    ss << gtAst->contents;
                }
                else {
                    // generic type, build
                    ss << build_Generic(gtAst);
                }
            }
            ss << ast->children[i]->contents;

            return ss.str();
        }
        else {
            // not really a generic type, but scoped
            auto tmp = build_Scoped(ast);
            return tmp;
        }
    }

    void Parser::build_Parameters(std::vector<suil::scc::Parameter> &out, mpc_ast_t *ast)
    {
        auto build_Param = [&](mpc_ast_t *p) {
            // given ast is a parameter
            Parameter pmt{};
            int offset{0};
            if (strcmp("const", p->children[offset]->contents) == 0) {
                // parameter is constant
                pmt.IsConst = true;
                offset++;
            }

            pmt.ParameterType = build_Generic(p->children[offset++]);
            if (strcmp("&&", p->children[offset]->contents) == 0) {
                // we have a parse by move
                pmt.Kind = Parameter::Move;
                offset++;
            }
            else if (strcmp("&", p->children[offset]->contents) == 0) {
                // we have a parse by reference
                pmt.Kind = Parameter::Reference;
                offset++;
            }

            pmt.Name          = std::string(p->children[offset]->contents);
            out.push_back(std::move(pmt));
        };

        if (strcmp("params|>", ast->tag) == 0) {
            // complex parameters
            for (int i = 0; i < ast->children_num; i++) {
                // build all included parameter
                if (strcmp(",", ast->children[i]->contents) == 0) {
                    // ignore commas
                    continue;
                }

                build_Param(ast->children[i]);
            }
        }
        else {
            // single parameter
            build_Param(ast);
        }
    }

    Field Parser::build_Field(mpc_ast_t *ast)
    {
        Field fld{};
        int offset = 0;
        if (strcmp("attribs|>", ast->children[offset]->tag) == 0) {
            // field has attributes
            build_Attribs(fld.Attribs, ast->children[offset++]);
        }

        fld.FieldType = build_Generic(ast->children[offset++]);
        fld.Name      =  std::string(ast->children[offset]->contents);

        return std::move(fld);
    }

    Method Parser::build_Method(mpc_ast_t *ast)
    {
        Method m{};
        int offset = 0;
        if (strcmp("attribs|>", ast->children[offset]->tag) == 0) {
            // field has attributes
            build_Attribs(m.Attribs, ast->children[offset++]);
        }

        m.ReturnType = build_Generic(ast->children[offset++]);
        m.Name       =  std::string(ast->children[offset++]->contents);
        if (strcmp(")", ast->children[offset+1]->contents) != 0) {
            // has parameters build them
            build_Parameters(m.Params, ast->children[++offset]);
        }

        return std::move(m);
    }
}