//
// Created by dc on 28/09/18.
//

#include <suil/cmdl.h>
#include <suil/init.h>
#include <suil/json.h>
#include <suil/file.h>
#include "typegen.hpp"


using namespace suil;

static void gen_Types(suil::String& in, suil::String& out);

void cmd_generate(cmdl::Parser& parser) {
    cmdl::Cmd gen{"gen", "Generate type base of json file"};
    gen << cmdl::Arg{"input",
                     "path to the json file to use to generate types file",
                     'i', false, true};
    gen << cmdl::Arg{"output",
                     "path to the json file to use to generate types file",
                     'o', false};

    gen([&](cmdl::Cmd& cmd){
        String in = cmd["input"];
        if (in.empty()) {
            // fail immediately
            fprintf(stderr, "error: input must be a valid filenamess\n");
            exit(EXIT_FAILURE);
        }

        String out = cmd["output"];
        gen_Types(in, out);
    });

    parser.add(std::move(gen));
}


int main(int argc, char *argv[])
{
    suil::init(opt(printinfo, false));

    cmdl::Parser parser(APP_NAME, APP_VERSION);
    cmd_generate(parser);
    try {
        parser.parse(argc, argv);
        parser.handle();
    }
    catch (...)
    {
        fprintf(stderr, "error: %s\n", Exception::fromCurrent().what());
        exit(EXIT_FAILURE);
    }
    return 0;
}

namespace suil::tools  {

    static SuilgenSchema Schema_load(const char *schemaFile)
    {
        if (!utils::fs::exists(schemaFile))
        {
            // cannot generate from non-existent file
            throw Exception::create(
                    "input schema file '", schemaFile, "' does not exist");
        }

        // read json file
        auto data = utils::fs::readall(schemaFile);

        // parse json file
        SuilgenSchema  schema;
        iod::json_decode(schema, data);
        return std::move(schema);
    }


    static void Schema_symbols(SuilgenSchema& schema, File& out)
    {
        Map<bool> added;
        auto addSymbol = [&](const String& name) {
            if (added.find(name) != added.end())
                return;

            out << "#ifndef IOD_SYMBOL_"    << name   << "\n"
                << "#define IOD_SYMBOL_"    << name   << "\n"
                << "    iod_define_symbol(" << name << ")\n"
                << "#endif\n\n";
            added[name] = true;
        };

        for (auto& ft: schema.extraSymbols)
        {
            // add floating definitions first
            addSymbol(ft);
        }

        for (auto& tp: schema.types)
        {
            // add field symbols defined for each type
            for (auto field: tp.fields)
            {
                addSymbol(field.name);
            }
        }
    }

    static void Schema_types(SuilgenSchema& schema, File& out)
    {
        for (auto& tp: schema.types)
        {
            out << "    typedef decltype(iod::D(\n";
            bool isFirst{true};
            for (auto& field: tp.fields)
            {
                if (!isFirst) out << ",\n";
                out << "        prop(" << field.name;
                if (!field.attribs.empty()) {
                    // append attributes
                    bool attrFirst{true};
                    out << "(";
                    for (auto& attr : field.attribs) {
                        if (!attrFirst) out << ", ";
                        out << "var(" << attr << ")";
                        attrFirst = false;
                    }
                    out << ")";
                }
                out << ", " << field.type << ")";
                isFirst = false;
            }
            out << "\n"
                << "    )) " << tp.name << ";\n\n";
        }
    }

    static void Schema_generate(SuilgenSchema& schema, const char* outputFile)
    {
        if (utils::fs::exists(outputFile))
        {
            // truncate file
            utils::fs::remove(outputFile);
        }

        // create new file
        File f(outputFile, O_WRONLY|O_CREAT|O_APPEND, 0666);

        auto guard = schema.guard;
        if (guard.empty()) {
            // generate random guard
            guard = utils::catstr("_SUILGEN_", utils::randbytes(12), "_");
        }
        f << "#ifndef " << guard << "\n";
        f << "#define " << guard << "\n\n";

        if (!schema.includes.empty()) {
            // define include paths
            for (auto& inc : schema.includes)
                f << "#include " << inc << "\n";
        }
        f << "#include <iod/symbol.hh>\n\n";

        // generate the symbols
        Schema_symbols(schema, f);

        if (!schema.ns.empty()) {
            // add namespace declaration if any is defined
            f << "namespace " << schema.ns << " { \n\n";
        }

        // generate the types
        Schema_types(schema, f);

        if (!schema.ns.empty()) {
            // add terminame namespaces
            f << "}\n";
        }

        f << "\n"
          << "#endif // " << guard;
        f.close();
    }
}

void gen_Types(suil::String& in, suil::String& out)
{
    auto outputFile = out.peek();
    if (outputFile.empty()) {
        outputFile = utils::catstr(in, ".h");
    }

    // load schema
    sinfo("loading schema file %s", in());
    auto schema = suil::tools::Schema_load(in());
    // generate types from loaded schema
    sinfo("generating types file %s", out());
    suil::tools::Schema_generate(schema, outputFile());
}