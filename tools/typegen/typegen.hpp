//
// Created by dc on 28/09/18.
//

#ifndef SUIL_TYPEGEN_HPP
#define SUIL_TYPEGEN_HPP

#include <vector>

#include <suil/utils.h>
#include <tools_symbols.h>

namespace suil::tools {

    typedef decltype(iod::D(
        prop(name,    String),
        prop(type,    String),
        prop(attribs(var(optional)), std::vector<String>)
    )) SuilgenSchemaField;

    typedef decltype(iod::D(
        prop(name,       String),
        prop(fields,     std::vector<SuilgenSchemaField>)
    )) SuilgenSchemaType;

    typedef decltype(iod::D(
        prop(ns,           String),
        prop(includes,     std::vector<String>),
        prop(guard,        String),
        prop(extraSymbols, std::vector<String>),
        prop(types,        std::vector<SuilgenSchemaType>)
    )) SuilgenSchema;
}

#endif //SUIL_TYPEGEN_HPP
