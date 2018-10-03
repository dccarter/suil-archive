cmake_minimum_required(VERSION 3.5)
project(libsuil C CXX)

file(GLOB LIB_SUIL_BASE_SOURCES *.cpp *.c)
file(GLOB_RECURSE LIB_SUIL_HTTP_SOURCES http/*.cpp http/*.c)
file(GLOB_RECURSE LIB_SUIL_SQL_SOURCES  sql/*.cpp  sql/*.c)
# only include docker sources when enables
set(LIB_SUIL_DOCKER_SOURCES)
if (SUIL_BUILD_DOCKER)
    file(GLOB_RECURSE LIB_SUIL_DOCKER_SOURCES  docker/*.cpp  docker/*.c)
endif()

set(LIB_SUIL_SOURCES
        ${LIB_SUIL_BASE_SOURCES}
        ${LIB_SUIL_HTTP_SOURCES}
        ${LIB_SUIL_SQL_SOURCES}
        ${LIB_SUIL_DOCKER_SOURCES})

set_property(GLOBAL PROPERTY prop_SUIL_SOURCES ${LIB_SUIL_SOURCES})

# add a shared and a static library
add_library(suil SHARED ${LIB_SUIL_SOURCES})
target_link_libraries(suil ${SUIL_LIBRARIES}   mill_s snappy)

# generate all symbols used in suil project
suil_iod_symbols(suil
        BINARY  ${CMAKE_BINARY_DIR}/deps/iod/iodsyms
        SYMBOLS ${CMAKE_SOURCE_DIR}/suil/suil.sym
        OUTPUT  ${CMAKE_SOURCE_DIR}/suil/symbols.h
        DEPENDS iodsyms)

if (LIB_SUIL_DOCKER_SOURCES)
    # generate docker types
    suil_gen_types(suil
            BINARY  ${CMAKE_BINARY_DIR}/tools/suilgen
            SCHEMAS ${CMAKE_CURRENT_SOURCE_DIR}/docker/types.json
            OUTPUT  ${CMAKE_CURRENT_SOURCE_DIR}/docker/types.hpp)
endif(LIB_SUIL_DOCKER_SOURCES)

# change output directory of suil
set_target_properties(suil
        PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

set(suil_INSTALL_TARGETS suil)

install(TARGETS ${suil_INSTALL_TARGETS}
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin)
install(FILES suil.sql suil.pgsql.sql
        DESTINATION share/suil/db/)
install(FILES suil.sqlite
        DESTINATION share/suil/sqlite/)