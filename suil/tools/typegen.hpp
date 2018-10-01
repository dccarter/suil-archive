//
// Created by dc on 28/09/18.
//

#ifndef SUIL_TYPEGEN_HPP
#define SUIL_TYPEGEN_HPP

#include <vector>

#include <suil/sys.hpp>
#include <suil/tools/tools_symbols.h>

namespace suil::tools {

    typedef decltype(iod::D(
        prop(name,    zcstring),
        prop(type,    zcstring)
    )) SuilgenSchemaField;

    typedef decltype(iod::D(
        prop(name,       zcstring),
        prop(fields,     std::vector<SuilgenSchemaField>)
    )) SuilgenSchemaType;

    typedef decltype(iod::D(
        prop(ns,           zcstring),
        prop(includes,     std::vector<zcstring>),
        prop(guard,        zcstring),
        prop(extraSymbols, std::vector<zcstring>),
        prop(types,        std::vector<SuilgenSchemaType>)
    )) SuilgenSchema;
}

#endif //SUIL_TYPEGEN_HPP
