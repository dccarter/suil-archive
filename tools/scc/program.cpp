//
// Created by dc on 24/12/18.
//

#include <suil/console.h>
#include <suil/file.h>
#include <suil/utils.h>
#include "program.h"

namespace suil::scc {

#define spaces(n) suil::String(' ', n)

    static void generateprogramFileSymbols(ProgramFile &pf, File &out)
    {
        std::map<std::string, bool> added;
        auto addSymbol = [&](const std::string& name) {
            if (added.find(name) != added.end())
                return;

            out << "#ifndef IOD_SYMBOL_"    << name   << "\n"
                << "#define IOD_SYMBOL_"    << name   << "\n"
                << "    iod_define_symbol(" << name << ")\n"
                << "#endif\n\n";
            added[name] = true;
        };

        for (auto& ft: pf.Symbols)
        {
            // add floating definitions first
            addSymbol(ft);
        }

        for (auto& tp: pf.MetaTypes)
        {
            // add field symbols defined for each type
            for (auto field: tp.Fields)
            {
                addSymbol(field.Name);
                for (auto attr: field.Attribs)
                {
                    if (attr.isSimple())
                        addSymbol(attr.Resolved);
                }
            }
        }
    }

    static void generateMetaTypeHeaders(ProgramFile &pf, File &out)
    {
        for (auto& tp: pf.MetaTypes) {
            // ignoring attributes in header
            out << "    typedef decltype(iod::D(\n";
            bool isFirst{true};
            for (auto& field: tp.Fields)
            {
                if (!isFirst) out << ",\n";
                out << "        prop(" << field.Name;
                if (!field.Attribs.empty()) {
                    // append attributes
                    bool attrFirst{true};
                    out << "(";
                    for (auto& attr : field.Attribs) {
                        if (!attr.isSimple())
                            continue;
                        if (!attrFirst) out << ", ";
                        out << "var(" << attr.Resolved << ")";
                        attrFirst = false;
                    }
                    out << ")";
                }
                out << ", " << field.FieldType << ")";
                isFirst = false;
            }
            out << "\n"
                << "    )) " << tp.Name << ";\n\n";
        }
    }

    static void generateServiceHeaders(ProgramFile &pf, File &out)
    {
        auto appendMethods = [&](const std::vector<Method>& methods) {
            for (auto& m: methods) {
                out << "        " << m.ReturnType << " " << m.Name << "(";
                bool first = true;
                for (auto& p : m.Params) {
                    if (!first)
                        out << ", ";
                    first = false;
                    if (p.IsConst)
                        out << "const ";
                    out << p.ParameterType;
                    if (p.Kind == Parameter::Move)
                        out << "&&";
                    else if (p.Kind == Parameter::Reference)
                        out << "&";
                    out << " " << p.Name;
                }
                out << ");\n\n";
            }
        };

        auto generateForJrpc = [&](const RpcType& svc) {
            // start with with client
            out << spaces(4) << "struct j" << svc.Name << "Client: suil::rpc::JsonRpcClient {\n\n"
                << spaces(4) << "public:\n\n";
            appendMethods(svc.Methods);
            out << spaces(4) << "};\n\n";
            // add service server handler
            out << spaces(4) << svc.Kind << " j" << svc.Name << "Handler : " << svc.Name
                             << ", suil::rpc::JsonRpcHandler {\n\n";
            out << spaces(4) << "protected:\n\n"
                << spaces(8) << "suil::rpc::ReturnType operator()("
                                "const suil::String& method, const suil::json::Object& params, int id) override;\n"
                << spaces(4) << "};\n\n";
        };

        auto generateForSuil = [&](const RpcType& svc) {
            // start with with client
            out << spaces(4) << "struct s" << svc.Name << "Client: suil::rpc::SuilRpcClient {\n\n"
                << spaces(4) << "public:\n\n";
            appendMethods(svc.Methods);
            out << spaces(4) << "};\n\n";
            // add service server handler
            out << spaces(4) << svc.Kind << " s" << svc.Name << "Handler : " << svc.Name
                             << ", suil::rpc::SuilRpcHandler {\n\n"
                << spaces(8) << "s" << svc.Name << "Handler();\n\n";
            out << spaces(4) << "protected:\n\n"
                << spaces(8) << "suil::Result operator()("
                                "suil::Breadboard& results, int method, suil::Breadboard& params, int id) override;\n"
                << spaces(4) << "};\n\n";
        };

        for (auto& svc: pf.Services) {
            out << spaces(4) << svc.Kind << " " << svc.Name << " {\n\n";
            appendMethods(svc.Methods);
            out << spaces(4) << "};\n\n";

            auto both = (svc.Kind == "service");
            // start with with client
            if (both || (svc.Kind == "srpc"))
                generateForSuil(svc);
            if (both || (svc.Kind == "jrpc"))
                generateForJrpc(svc);
        }
    }

    static void generateJsonRpcSources(File& sf, scc::RpcType& svc)
    {
        // start by implementing handler
        sf << spaces(4) << "ReturnType j" << svc.Name
           << "Handler::operator()(const suil::String& method, const suil::json::Object& params, int id)\n"
           << spaces(4) << "{\n";
        sf << spaces(8) <<"static const suil::Map<int> scMappings = {\n";
        int id = 0;
        for (auto& m: svc.Methods) {
            // append method mappings
            if (id != 0)
                sf << ",\n";
            sf << spaces(12) << "{\"" << m.Name << "\"," << utils::tostr(id++) << "}";
        }
        sf << "};\n\n";
        sf << spaces(8) << "auto it = scMappings.find(method);\n"
           << spaces(8) << "if (it == scMappings.end())\n"
           << spaces(8) << "{\n"
           << spaces(12)<<     "// method not found\n"
           << spaces(12)<<     "return std::make_pair(JRPC_METHOD_NOT_FOUND,"
           <<                             " suil::json::Object(\"method does not exists\"));\n"
           << spaces(8) << "}\n"
           << "\n"
           << spaces(8) << "switch(it->second) {\n";
        id = 0;
        for(auto& m: svc.Methods) {
            // append method handling cases
            sf << spaces(12) << "case " << utils::tostr(id++) << ": {\n";
            OBuffer ob(32);
            bool first{true};
            for (auto& p: m.Params) {
                sf << spaces(16) << "" << p.ParameterType << " " << p.Name << " = (" << p.ParameterType << ") "
                   << "params[\"" << p.Name << "\"];\n";
                if (!first)
                    ob << ", ";
                first = false;
                if (p.Kind == Parameter::Move)
                    ob << "std::move(";
                ob << p.Name;
                if (p.Kind == Parameter::Move)
                    ob << ")";
            }
            sf << "\n";
            if (m.ReturnType != "void") {
                // void methods will return nullptr
                sf << spaces(16) << "return std::make_pair(0, suil::json::Object(Ego."
                   << m.Name << "(" << ob << ")));\n";
            }
            else {
                sf << spaces(16) << "Ego." << m.Name << "(" << ob << ");\n";
                sf << spaces(16) << "return std::make_pair(0, suil::json::Object(nullptr));\n";
            }
            sf << spaces(12) << "}\n";
        }
        sf << spaces(12) << "default:\n"
           << spaces(16) << "  // method not found\n"
           << spaces(16) << "  return std::make_pair(JRPC_METHOD_NOT_FOUND,"
           <<                             " suil::json::Object(\"method does not exists\"));\n"
           << spaces(12) << "}\n"
           << spaces(8)  <<"}\n\n";

        // implement client
        for (auto& m: svc.Methods) {
            sf << spaces(4) << m.ReturnType << " j" << svc.Name << "Client::" << m.Name << "(";
            bool first = true;
            OBuffer ob(32);
            for (auto& p : m.Params) {
                if (!first) {
                    sf << ", ";
                    ob << ", ";
                }
                first = false;
                if (p.IsConst)
                    sf << "const ";
                sf << p.ParameterType;
                if (p.Kind == Parameter::Move)
                    sf << "&&";
                else if (p.Kind == Parameter::Reference)
                    sf << "&";
                sf << " " << p.Name;
                ob << "\"" << p.Name << "\", " << p.Name;
            }
            sf << ")\n"
               << spaces(4) << "{\n";
            if (m.Params.empty())
                sf << spaces(8) << "suil::json::Object params(nullptr);";
            else
                sf << spaces(8) << "suil::json::Object params(json::Obj, " << ob << ");";
            sf << "\n"
               << spaces(8)  << "auto ret = Ego.call(\"" << m.Name << "\", std::move(params));\n"
               << spaces(8)  << "if (ret.first)\n"
               << spaces(12) << "// api error\n"
               << spaces(12) << "throw suil::Exception::create((String)ret.second);\n\n";
            if (m.ReturnType != "void")
                sf << spaces(8)  << "return (" << m.ReturnType << ") ret.second;\n";
            sf << spaces(4)  << "}\n\n";
        }
    }

    static void generateSuilRpcSources(File& sf, scc::RpcType& svc)
    {
        sf << spaces(4) << "s" << svc.Name << "Handler::s" << svc.Name << "Handler() : "
                        << svc.Name << "(), suil::rpc::SuilRpcHandler()\n"
           << spaces(4) << "{\n";
        int id = 1;
        for (auto& m: svc.Methods) {
            sf << spaces(8) << "Ego.methodsMeta.emplace_back(" << utils::tostr(id++) << ", \"" << m.Name << "\");\n";
        }
        sf << spaces(4) << "}\n\n";

        sf << spaces(4) << "Result s" << svc.Name
           << "Handler::operator()(suil::Breadboard& results, int method, suil::Breadboard& params, int id)\n"
           << spaces(4) << "{\n"
           << spaces(8) << "switch(method) {\n";

        id = 1;
        for(auto& m: svc.Methods) {
            // append method handling cases
            sf << spaces(12) << "case " << utils::tostr(id++) << ": {\n";
            OBuffer ob(32);
            bool first{true};
            for (auto& p: m.Params) {
                sf << spaces(16) << "" << p.ParameterType << " " << p.Name << "{};\n"
                   << spaces(16) << "params >> " << p.Name << ";\n";
                if (!first)
                    ob << ", ";
                first = false;
                if (p.Kind == Parameter::Move)
                    ob << "std::move(";
                ob << p.Name;
                if (p.Kind == Parameter::Move)
                    ob << ")";
            }
            sf << "\n";
            if (m.ReturnType == "void") {
                // void methods will return nullptr
                sf << spaces(16) << "Ego." << m.Name << "(" << ob << ");\n";
            }
            else {
                sf << spaces(16) << m.ReturnType << " tmp = Ego." << m.Name << "(" << ob << ");\n";
                sf << spaces(16) << "results << tmp;\n";
            }
            sf << spaces(16) << "return Result(0);\n"
               << spaces(12) << "}\n";
        }
        sf << spaces(12) << "default:\n"
           << spaces(16) << "// method not found\n"
           << spaces(16) << "Result res(SRPC_METHOD_NOT_FOUND);\n"
           << spaces(16) << "res << \"requested method does not exist\";\n"
           << spaces(16) << "return std::move(res);\n"
           << spaces(12) << "}\n"
           << spaces(8)  <<"}\n\n";

        // implement client
        for (auto& m: svc.Methods) {
            sf << spaces(4) << m.ReturnType << " s" << svc.Name << "Client::" << m.Name << "(";
            bool first = true;
            OBuffer ob(32);
            for (auto& p : m.Params) {
                if (!first) {
                    sf << ", ";
                    ob << ", ";
                }
                first = false;
                if (p.IsConst)
                    sf << "const ";
                sf << p.ParameterType;
                if (p.Kind == Parameter::Move)
                    sf << "&&";
                else if (p.Kind == Parameter::Reference)
                    sf << "&";
                sf << " " << p.Name;
                ob << p.Name;
            }
            auto ret = (m.ReturnType == "void")? "" : "return ";
            sf << ")\n"
               << spaces(4) << "{\n";
            if (m.Params.empty())
                sf << spaces(8) << ret << "Ego.call<" << m.ReturnType << ">(\"" << m.Name << "\");\n";
            else
                sf << spaces(8) << ret << "Ego.call<" << m.ReturnType << ">(\"" << m.Name << "\", " << ob << ");\n";
            sf << spaces(4)  << "}\n\n";
        }
    }

    void ProgramFile::generateHeaderFile(const suil::String &path)
    {
        if (utils::fs::exists(path())) {
            // remove current file
            utils::fs::remove(path());
        }

        File hf(path(), O_WRONLY|O_CREAT|O_APPEND, 0666);
        // append guard
        hf << "#pragma once\n\n";
        // append includes
        hf << "#include <suil/result.h>\n";
        for(auto& inc: Ego.Includes) {
            // add all includes
            hf << inc << "\n";
        }
        hf << "\n";
        // append symbols
        generateprogramFileSymbols(Ego, hf);

        hf << "namespace " << Ego.Namespace << " {\n\n";

        generateMetaTypeHeaders(Ego, hf);
        generateServiceHeaders(Ego, hf);

        hf << "}\n";
        hf.flush();
        hf.close();
    }

    void ProgramFile::generateSourceFile(const char *filename, const String &spath)
    {
        if (utils::fs::exists(spath())) {
            // remove current file
            utils::fs::remove(spath());
        }

        File sf(spath(), O_WRONLY|O_CREAT|O_APPEND, 0666);
        sf << "/* file generated by suil's scc program, do not modify */\n\n"
           << "#include \"" << filename << ".h\"\n\n"
           << "using namespace suil::rpc;\n\n"
           << "namespace " << Ego.Namespace << " {\n\n";

        // implement client methods
        for (auto& svc: Ego.Services) {

            auto both = (svc.Kind == "service");
            if (both || (svc.Kind == "jrpc")) {
                // generate json rpc
                generateJsonRpcSources(sf, svc);
            }

            if (both || (svc.Kind == "srpc")) {
                // generate suil rpc
                generateSuilRpcSources(sf, svc);
            }
        }

        sf << "}\n";

        sf.flush();
        sf.close();
    }

    void ProgramFile::generate(const char *filename, const char *outDir)
    {
        const char *dir = outDir == nullptr? "./" : outDir;
        if (!utils::fs::exists(dir)) {
            // output directory does not exist, attempt to create
            utils::fs::mkdir(dir, true);
        }

        auto fname = utils::fs::getname(filename);
        auto hdrFile = utils::catstr(dir, "/", fname, ".h");
        auto cppFile = utils::catstr(dir, "/", fname, ".cpp");

        // generate header file
        generateHeaderFile(hdrFile);
        generateSourceFile(fname(), cppFile);
    }
}