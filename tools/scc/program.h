//
// Created by dc on 24/12/18.
//

#ifndef SUIL_PROGRAM_H
#define SUIL_PROGRAM_H

#include <vector>

#include <suil/base.h>

namespace suil::scc {

    struct Scoped {
        std::vector<std::string> Parts;
        std::string              Resolved;
        inline std::string toString() const {
            return Resolved;
        }

        inline void toString(std::stringstream& ss) const {
            ss << Resolved;
        }
    };

    using Type = Scoped;

    struct Attribute: Scoped {
        inline bool isSimple() const {
            return Parts.size() == 1;
        }
    };

    struct WithAttributes {
        std::vector<Attribute> Attribs;
        bool hasAttribute(const std::string &attrib) const {
            for (auto& a: Attribs) {
                if (a.Resolved == attrib)
                    return true;
            }
            return false;
        }
    };

    struct Field : WithAttributes {
        std::string            FieldType;
        std::string            Name;
    };

    struct Parameter {
        enum _Kind {
            Normal,
            Move,
            Reference
        };
        std::string     ParameterType;
        std::string     Name;
        _Kind           Kind{Normal};
        bool            IsConst{false};
    };

    struct Method : WithAttributes {
        std::string             ReturnType;
        std::string             Name;
        std::vector<Parameter>  Params;
    };

    struct MetaType : WithAttributes {
        std::vector<Attribute> Attribs;
        std::string            Name;
        std::vector<Field>     Fields;
    };

    struct RpcType : WithAttributes {
        std::string            Kind;
        std::string            Name;
        std::vector<Method>    Methods;
    };

    struct ProgramFile {
        std::vector<std::string> Includes;
        std::vector<std::string> Symbols;
        std::string              Namespace;
        std::vector<MetaType>    MetaTypes;
        std::vector<RpcType>     Services;

        void generate(const char *filename, const char *outDir = nullptr);

    private:
        void generateHeaderFile(const String &path);
        void generateSourceFile(const char *filename, const String &spath);
    };
}

#endif //SUIL_PROGRAM_H
