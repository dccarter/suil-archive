#ifdef __dclparser
__dclparser(Ident);
__dclparser(Attrib);
__dclparser(Type);
__dclparser(Generic);
__dclparser(Attribs);
__dclparser(Field);
__dclparser(FieldDcls);
__dclparser(Param);
__dclparser(ParamDcls);
__dclparser(Func);
__dclparser(FuncDcls);
__dclparser(Meta);
__dclparser(Rpc);
__dclparser(Scc);
__dclparser(Include);
__dclparser(Includes);
__dclparser(Namespace);
__dclparser(Program);
#elif defined(__initparser)
__initparser(Ident,         "ident");
__initparser(Attrib,        "attrib");
__initparser(Type,          "type");
__initparser(Generic,       "generic");
__initparser(Attribs,       "attribs");
__initparser(Field,         "field");
__initparser(FieldDcls,     "fieldDcls");
__initparser(Param,         "param");
__initparser(ParamDcls,     "paramDcls");
__initparser(Func,          "func");
__initparser(FuncDcls,      "funcDcls");
__initparser(Meta,          "meta");
__initparser(Rpc,           "rpc");
__initparser(Scc,           "scc");
__initparser(Include,       "include");
__initparser(Includes,      "includes");
__initparser(Namespace,     "namespace");
__initparser(Program,       "program");
#else
Ident,
Attrib,
Type,
Generic,
Attribs,
Field,
FieldDcls,
Param,
ParamDcls,
Func,
FuncDcls,
Meta,
Rpc,
Scc,
Include,
Includes,
Namespace,
Program
#endif