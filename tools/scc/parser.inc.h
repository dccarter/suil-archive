//
// Created by dc on 24/12/18.
//

#if defined(__PARSER_DECLARE)
constexpr int __count() const {return __LINE__; }
__PARSER_DECLARE(Ident);
__PARSER_DECLARE(Scoped);
__PARSER_DECLARE(Generic);
__PARSER_DECLARE(Attribs);
__PARSER_DECLARE(Field);
__PARSER_DECLARE(Fields);
__PARSER_DECLARE(Param);
__PARSER_DECLARE(Params);
__PARSER_DECLARE(Method);
__PARSER_DECLARE(Methods);
__PARSER_DECLARE(Meta);
__PARSER_DECLARE(Rpc);
__PARSER_DECLARE(Types);
__PARSER_DECLARE(Namespace);
__PARSER_DECLARE(Symbol);
__PARSER_DECLARE(Symbols);
__PARSER_DECLARE(Include);
__PARSER_DECLARE(Includes);
__PARSER_DECLARE(Program);
constexpr int count() const { return __LINE__ - __count() - 1; }
#elif defined(__PARSER_INITIALIZE)
__PARSER_INITIALIZE(Ident,                  "ident");
__PARSER_INITIALIZE(Scoped,                 "scoped");
__PARSER_INITIALIZE(Generic,                "generic");
__PARSER_INITIALIZE(Attribs,                "attribs");
__PARSER_INITIALIZE(Field,                  "field");
__PARSER_INITIALIZE(Fields,                 "fields");
__PARSER_INITIALIZE(Param,                  "param");
__PARSER_INITIALIZE(Params,                 "params");
__PARSER_INITIALIZE(Method,                 "method");
__PARSER_INITIALIZE(Methods,                "methods");
__PARSER_INITIALIZE(Meta,                   "meta");
__PARSER_INITIALIZE(Rpc,                    "rpc");
__PARSER_INITIALIZE(Types,                  "types");
__PARSER_INITIALIZE(Namespace,              "namespace");
__PARSER_INITIALIZE(Symbol,                 "symbol");
__PARSER_INITIALIZE(Symbols,                "symbols");
__PARSER_INITIALIZE(Include,                "include");
__PARSER_INITIALIZE(Includes,               "includes");
__PARSER_INITIALIZE(Program,                "program");
#elif defined(__PARSER_ASSIGN)
__USE_PARSER(Ident)     = __PARSER_ASSIGN(Ident);
__USE_PARSER(Scoped)    = __PARSER_ASSIGN(Scoped);
__USE_PARSER(Generic)   = __PARSER_ASSIGN(Generic);
__USE_PARSER(Attribs)   = __PARSER_ASSIGN(Attribs);
__USE_PARSER(Field)     = __PARSER_ASSIGN(Field);
__USE_PARSER(Fields)    = __PARSER_ASSIGN(Fields);
__USE_PARSER(Param)     = __PARSER_ASSIGN(Param);
__USE_PARSER(Params)    = __PARSER_ASSIGN(Params);
__USE_PARSER(Method)    = __PARSER_ASSIGN(Method);
__USE_PARSER(Methods)   = __PARSER_ASSIGN(Methods);
__USE_PARSER(Meta)      = __PARSER_ASSIGN(Meta);
__USE_PARSER(Rpc)       = __PARSER_ASSIGN(Rpc);
__USE_PARSER(Types)     = __PARSER_ASSIGN(Types);
__USE_PARSER(Namespace) = __PARSER_ASSIGN(Namespace);
__USE_PARSER(Symbol)    = __PARSER_ASSIGN(Symbol);
__USE_PARSER(Symbols)   = __PARSER_ASSIGN(Symbols);
__USE_PARSER(Include)   = __PARSER_ASSIGN(Include);
__USE_PARSER(Includes)  = __PARSER_ASSIGN(Includes);
__USE_PARSER(Program)   = __PARSER_ASSIGN(Program);
#else
__USE_PARSER(Ident),
__USE_PARSER(Scoped),
__USE_PARSER(Generic),
__USE_PARSER(Attribs),
__USE_PARSER(Field),
__USE_PARSER(Fields),
__USE_PARSER(Param),
__USE_PARSER(Params),
__USE_PARSER(Method),
__USE_PARSER(Methods),
__USE_PARSER(Meta),
__USE_PARSER(Rpc),
__USE_PARSER(Types),
__USE_PARSER(Namespace),
__USE_PARSER(Symbol),
__USE_PARSER(Symbols),
__USE_PARSER(Include),
__USE_PARSER(Includes),
__USE_PARSER(Program)
#endif
