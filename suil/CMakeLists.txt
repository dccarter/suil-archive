cmake_minimum_required(VERSION 3.5)
project(libsuil C CXX)

file(GLOB_RECURSE LIB_SUIL_SOURCES *.cpp *.c)
add_library(suil STATIC ${LIB_SUIL_SOURCES})
target_link_libraries(suil ${SUIL_LIBRARIES})
add_library(suil_s SHARED ${LIB_SUIL_SOURCES})
target_link_libraries(suil_s ${SUIL_LIBRARIES})
set_target_properties(suil_s PROPERTIES OUTPUT_NAME suil)

suil_iod_symbols(suil
        BINARY  ${CMAKE_BINARY_DIR}/deps/iod/iodsyms
        SYMBOLS ${CMAKE_SOURCE_DIR}/suil/suil.sym
        OUTPUT  ${CMAKE_SOURCE_DIR}/suil/symbols.h
        DEPENDS iodsyms)
add_dependencies(suil_s suil-gensyms)

set(suil_INSTALL_TARGETS suil_s)
if (SUIL_BUILD_DEV_PACKAGE)
    list(APPEND suil_INSTALL_TARGETS suil)
endif()
install(TARGETS ${suil_INSTALL_TARGETS}
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin)
install(FILES suil.sql suil.pgsql.sql
        DESTINATION share/suil/db/)
install(FILES suil.sqlite
        DESTINATION share/suil/sqlite/)