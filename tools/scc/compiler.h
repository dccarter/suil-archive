//
// Created by dc on 13/12/18.
//

#ifndef SUIL_COMPILER_H
#define SUIL_COMPILER_H

#include <suil/buffer.h>
#include <suil/logging.h>
#include <suil/file.h>

#include <map>

#include "mpc.h"

namespace suil::scc {

    define_log_tag(SCC);

    struct Compiler {
        Compiler();

        bool load(const char *grammarFile = nullptr);

        void repl();

        void parseFile(const char* file);

        void parseString(String&& str);

        void clear();

        ~Compiler();

    private:
        friend struct ProgramBuilder;
        friend struct FileCompiler;
        inline mpc_ast_t* astRoot() {
            return (mpc_ast_t *) parseResult.output;
        }

        String     fileName{};
        mpc_result_t parseResult{nullptr};
    private:
#define __dclparser(name) mpc_parser_t *name{nullptr};

#include "parsers.inc.h"

#undef __dclparser
    };

    using Symbols = std::vector<std::string>;

    struct ProgramBuilder;

    struct AstBuilder {
        AstBuilder(ProgramBuilder &programBuilder)
            : programBuilder(programBuilder)
        {}

        virtual void clear() { this->trav = nullptr; }

    protected:
        inline void resetTrav(mpc_ast_trav_t **trav) {
            this->trav = trav;
        }

        virtual mpc_ast_t* build_Type(std::string& name);

        virtual mpc_ast_t* build_GenericType(std::string& type);

        void expect(mpc_ast_t *node, const char *tag, const char *contents = nullptr);

        mpc_ast_trav_t **trav{nullptr};
        ProgramBuilder &programBuilder;
    };


    struct ComponentBuilder : AstBuilder {

        ComponentBuilder(ProgramBuilder &programBuilder)
            : AstBuilder(programBuilder)
        {}

        virtual mpc_ast_t* build(mpc_ast_trav_t** trav) = 0;
    };

    using Includes          = std::vector<std::string>;
    using ComponentBuilders = std::map<std::string, ComponentBuilder *>;

    struct FileCompiler;

    struct ProgramBuilder : AstBuilder {
        ProgramBuilder(FileCompiler& compiler)
            : AstBuilder(*this),
              fileCompiler(compiler)
        {}

        void addSymbol(std::string symbol);

        const Symbols &getSymbols() const {
            return symbolTable;
        }

        const Includes& getIncludes() const {
            return includes;
        }

        void build(Compiler &program);

        void clear() override;

    protected:
        mpc_ast_t* build_Includes();
        Symbols     symbolTable;
        Includes    includes;
        std::string Namespace;
    private:
        FileCompiler& fileCompiler;
    };

    struct ComponentGenerator {
        virtual void appendHeaderContent(File &headerFile, int tab) = 0;

        virtual void appendCppContent(File &cppFile, int tab) = 0;

        virtual ComponentBuilder &getBuilder() = 0;

        virtual void clear() {};

    };

    using ComponentGenerators = std::map<std::string, ComponentGenerator*>;

    struct FileCompiler {

        FileCompiler(String&& grammar = nullptr);

        void compileString(String&& str);
        void compileFile(const char* fileName);

        void registerGenerator(std::string component, ComponentGenerator *generator);

        inline ProgramBuilder& getProgramBuilder() {
            return Ego.programBuilder;
        }

        void clear();

    private:
        friend struct ProgramBuilder;

        ComponentBuilder &getComponetBuilderFor(const char *component);

        ComponentGenerators generators;
        ProgramBuilder      programBuilder;
        Compiler            program;
        String              grammar;
    };
}

#endif //SUIL_COMPILER_H
