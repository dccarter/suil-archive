//
// Created by dc on 13/12/18.
//

#include <suil/console.h>
#include <suil/file.h>
#include <suil/utils.h>
#include "meta_builder.h"

namespace suil::scc {

    static const char* sc_Grammar =
R"(ident           : /[a-zA-Z_][a-zA-Z0-9_]*/ | /[a-zA-Z_]/;
attrib          : <ident> ("::" <ident>)* ;
type            : <ident> ("::" <ident>)* ;
attribs         : "[[" <attrib> (',' <attrib>)* "]]" ;
field           : <attribs>* <type> <ident>;
fieldDcls       : (<field> ';')+ ;
param           : <type> <ident> ;
paramDcls       : <param> (',' <param>)* ;
func            : <type> <ident> '(' <paramDcls>? ')' ;
funcDcls        : (<func> ';')+ ;
meta            : "meta" <attribs>* <ident> '{' <fieldDcls> '}' ;
rpc             : ("suilrpc" | "jsonrpc") <attribs>* <ident> '{' <funcDcls> '}' ;
scc             : (<meta> | <rpc>) ';' ;
include         : "#include" (('"' /(\\.|[^"])+/ '"')|('<' /(\\.|[^>])+/ '>')) ;
includes        : <include>*;
namespace       : "namespace" <type> ;
program         : <includes> <namespace> '{' <scc>+ '}';)";


    Compiler::Compiler() {
#define __initparser(p, name) p = mpc_new(name)

#include "parsers.inc.h"

#undef __initparser
    }

    bool Compiler::load(const char *grammarFile)
    {
        mpc_err_t *err;
        if (grammarFile && utils::fs::exists(grammarFile)) {
            /* load grammar from file*/
            err = mpca_lang_contents(MPCA_LANG_DEFAULT,
                                     grammarFile,

#include "parsers.inc.h"

            );
        }
        else {
            /* load static grammar */
            err = mpca_lang(MPCA_LANG_DEFAULT,
                    sc_Grammar,
#include "parsers.inc.h"
            );
        }

        if (err != nullptr) {
            /* parsing grammar failed */
            char *str = mpc_err_string(err);
            console::red("error: %s", str);
            free(str);
            mpc_err_delete(err);
            return false;
        }

        return true;
    }

    void Compiler::repl() {
        while (true) {
            char *line;
            size_t len{0};
            console::info("> ");
            if (getline(&line, &len, stdin) < 0) {
                // error reading input
                console::warn("error: failed to read input: %s", errno_s);
                break;
            }

            if (strcmp("exit", line) == 0) {
                /* exit requested */
                free(line);
            }
            mpc_result_t r;
            if (mpc_parse("<stdin>", line, Program, &r)) {
                // parse and display whatever is typed on console
                mpc_ast_print((mpc_ast_t *) r.output);
                mpc_ast_delete((mpc_ast_t *) r.output);
            } else {
                mpc_err_print(r.error);
                mpc_err_delete(r.error);
            }

            free(line);
        }
    }

    void Compiler::parseFile(const char *file)
    {
        if (!mpc_parse_contents(file, Program, &parseResult)) {
            // parsing failed
            char *str = mpc_err_string(parseResult.error);
            String tmp{str, strlen(str), true};
            throw Exception::create("error: ", tmp);
        }
    }

    void Compiler::parseString(suil::String &&str)
    {
        if (!mpc_parse("<string>", str(), Program, &parseResult)) {
            // parsing failed
            char *err = mpc_err_string(parseResult.error);
            String tmp{err, strlen(err), true};
            throw Exception::create("error: ", tmp());
        }
    }

    void Compiler::clear() {
        if (astRoot()) {
            // delete as if not already deleted
            mpc_ast_delete(astRoot());
        }
        parseResult.output = nullptr;
    }

    Compiler::~Compiler() {
        Ego.clear();
        mpc_cleanup(8,
#include "parsers.inc.h"
                );
    }

    void AstBuilder::expect(mpc_ast_t *node, const char *tag, const char *contents)
    {
        if (node == nullptr) {
            // node cannot be null
            throw Exception::create(
                    "error: was expecting '",tag,"' but go nothing");
        }

        if (strcmp(node->tag, tag) != 0) {
            // unexpected lexeme
            throw Exception::create(
                    "error: was expecting lexeme '", tag , "' but got '", node->tag, "'");
        }

        if (contents && strcmp(node->contents, contents) != 0) {
            // unexpected string
            throw Exception::create(
                    "error: was expecting string '",contents,
                    "' but got '",node->contents,"'");
        }
    }

    mpc_ast_t* AstBuilder::build_Type(std::string &type)
    {
        std::stringstream ss;
        bool first{true};
        mpc_ast_t *it{nullptr};

        do {
            it = mpc_ast_traverse_next(trav);
            // expect a part
            Ego.expect(it, "ident|regex");
            if (!first)
                ss << "::";
            first = false;
            ss << it->contents;

            it = mpc_ast_traverse_next(trav);
            // take all parts of the attribute
        } while (it != nullptr && strcmp(it->contents, "::") == 0);

        // end of attribute
        type = ss.str();
        return it;
    }


    void ProgramBuilder::addSymbol(std::string symbol)
    {
        if (std::find_if(Ego.symbolTable.begin(), Ego.symbolTable.end(),
                         [&symbol](const auto& a) { return a != symbol; }) != Ego.symbolTable.end())
        {
            /* addOnly when it is not duplicate */
            Ego.symbolTable.push_back(symbol);
        }
    }

    mpc_ast_t* ProgramBuilder::build_Includes()
    {
        mpc_ast_t *it = mpc_ast_traverse_next(trav);
        do {
            Ego.expect(it, "string", "#include");
            // consume < or "
            mpc_ast_traverse_next(trav);
            // move to actual include
            it = mpc_ast_traverse_next(trav);
            // we now have our include
            Ego.expect(it, "regex");
            Ego.includes.emplace_back(it->contents);
            // consume closing " or >
            mpc_ast_traverse_next(trav);
            // move to next lexeme
            it = mpc_ast_traverse_next(trav);
        } while (it != nullptr && strcmp(it->tag, "include|>") == 0);

        return it;
    }

    void ProgramBuilder::build(suil::scc::Compiler &program)
    {
        // build from root AST
        mpc_ast_trav_t* it_trav = mpc_ast_traverse_start(program.astRoot(), mpc_ast_trav_order_pre);
        this->resetTrav(&it_trav);

        // start of our parsed file AST
        mpc_ast_t *it = mpc_ast_traverse_next(trav);
        Ego.expect(it, ">");
        it = mpc_ast_traverse_next(trav);

        // either includes or a namespace declaration or symbols
        if (strcmp(it->tag, "includes|include|>") == 0) {
            // parses single include
            it = build_Includes();
        }
        else if (strcmp(it->tag, "includes|>") == 0) {
            // parse multiple includes
            mpc_ast_traverse_next(trav);
            it = build_Includes();
        }

        // namespace expected here
        Ego.expect(it, "namespace|>");
        it = mpc_ast_traverse_next(trav);
        Ego.expect(it, "string", "namespace");
        it = mpc_ast_traverse_next(trav);
        if (strcmp(it->tag, "type|>") == 0) {
            // nested namespace
            it = Ego.build_Type(Namespace);
        }
        else {
            // un-nested namespace
            Ego.expect(it, "type|ident|regex");
            Ego.Namespace = std::string(it->contents);
            it = mpc_ast_traverse_next(trav);
        }
        Ego.expect(it, "char", "{");
        it = mpc_ast_traverse_next(trav);
        do {
            Ego.expect(it, "scc|>");
            // consume scc opening block
            mpc_ast_traverse_next(trav);
            // consume component type opening block
            it = mpc_ast_traverse_next(trav);
            Ego.expect(it, "string");
            // jump to component builder
            auto& componentBuilder = Ego.fileCompiler.getComponetBuilderFor(it->contents);
            it = componentBuilder.build(trav);
        } while(it && strcmp(it->tag, "scc|>") == 0);
        // expect namespace closing brace
        Ego.expect(it, "char", "}");

        // free the traverse
        mpc_ast_traverse_free(&it_trav);
        Ego.resetTrav(nullptr);
        trav = nullptr;
    }

    void ProgramBuilder::clear()
    {
        Ego.includes.clear();
        Ego.Namespace = "";
        Ego.symbolTable.clear();
    }

    FileCompiler::FileCompiler(suil::String &&grammar)
        : programBuilder(*this),
          grammar(std::move(grammar))
    {
        Ego.program.load(Ego.grammar.data());
    }

    ComponentBuilder& FileCompiler::getComponetBuilderFor(const char *component)
    {
        auto builder = Ego.generators.find(std::string(component));
        if (builder == Ego.generators.end()) {
            // unregistered component
            throw Exception::create("cannot build unknown component '", component, "'");
        }
        return builder->second->getBuilder();
    }

    void FileCompiler::registerGenerator(std::string component, ComponentGenerator *generator)
    {
        if (Ego.generators.find(component) != Ego.generators.end()) {
            // cannot duplicate generators
            throw Exception::create("component generator '", component, "' already added");
        }
        Ego.generators.emplace(component, generator);
    }

    void FileCompiler::clear()
    {
        for(auto& gen: Ego.generators) {
            // clear component generators
            gen.second->clear();
        }
        programBuilder.clear();
        program.clear();
    }

    void FileCompiler::compileString(String&& str)
    {
        Ego.clear();
        // compile string
        program.parseString(str.peek());
        mpc_ast_print(program.astRoot());
        // build compiled program
        // programBuilder.build(program);
    }

    void FileCompiler::compileFile(const char *fileName)
    {
        Ego.clear();
        // compile string
        program.parseFile(fileName);
        // build compiled program
        programBuilder.build(program);
    }
}