//
// Created by dc on 10/13/17.
//

#ifndef SUIL_TEST_SYMBOLS_H
#define SUIL_TEST_SYMBOLS_H

#include <iod/symbol.hh>

#define tvar(v) test::s::_##v
#define tsym(v) tvar(v)
#define topt(o, v) tvar(o) = v
#define ton(ev) test::s::_on_##ev
#define tprop(o, v) topt(o, v)()

namespace test {

#ifndef TEST_IOD_SYMBOL_a
#define TEST_IOD_SYMBOL_a
    iod_define_symbol(a)
#endif

#ifndef TEST_IOD_SYMBOL_b
#define TEST_IOD_SYMBOL_b
    iod_define_symbol(b)
#endif
#ifndef TEST_IOD_SYMBOL_c
#define TEST_IOD_SYMBOL_c
    iod_define_symbol(c)
#endif
#ifndef TEST_IOD_SYMBOL_d
#define TEST_IOD_SYMBOL_d
    iod_define_symbol(d)
#endif
#ifndef TEST_IOD_SYMBOL_e
#define TEST_IOD_SYMBOL_e
    iod_define_symbol(e)
#endif
#ifndef TEST_IOD_SYMBOL_f
#define TEST_IOD_SYMBOL_f
    iod_define_symbol(f)
#endif

}
#endif //SUIL_TEST_SYMBOLS_H
