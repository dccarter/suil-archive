cmake_minimum_required(VERSION 3.5)
project(libsuil C CXX)

file(GLOB LIB_SUIL_BASE_SOURCES *.cpp *.c)
file(GLOB_RECURSE LIB_SUIL_HTTP_SOURCES http/*.cpp http/*.c)
file(GLOB_RECURSE LIB_SUIL_SQL_SOURCES  sql/*.cpp  sql/*.c)
set(LIB_SUIL_SOURCES
        ${LIB_SUIL_BASE_SOURCES}
        ${LIB_SUIL_HTTP_SOURCES}
        ${LIB_SUIL_SQL_SOURCES})

add_library(suil_s STATIC ${LIB_SUIL_SOURCES})
target_link_libraries(suil_s ${SUIL_LIBRARIES})
add_library(suil SHARED ${LIB_SUIL_SOURCES})
target_link_libraries(suil ${SUIL_LIBRARIES})
set_target_properties(suil_s PROPERTIES OUTPUT_NAME suil)
if (NOT SUIL_BUILD_DEV_PACKAGE)
     target_compile_options(suil PUBLIC "$<$<CONFIG:RELEASE>:-Os>")
endif()

suil_iod_symbols(suil
        BINARY  ${CMAKE_BINARY_DIR}/deps/iod/iodsyms
        SYMBOLS ${CMAKE_SOURCE_DIR}/suil/suil.sym
        OUTPUT  ${CMAKE_SOURCE_DIR}/suil/symbols.h
        DEPENDS iodsyms)
add_dependencies(suil_s suil-gensyms)
add_dependencies(suil   suil-gensyms)

set(suil_INSTALL_TARGETS suil)
if (SUIL_BUILD_DEV_PACKAGE)
    list(APPEND suil_INSTALL_TARGETS suil_s)
endif()
install(TARGETS ${suil_INSTALL_TARGETS}
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin)
install(FILES suil.sql suil.pgsql.sql
        DESTINATION share/suil/db/)
install(FILES suil.sqlite
        DESTINATION share/suil/sqlite/)

if (SUIL_BUILD_ABCI)
    add_subdirectory(abci)

endif()