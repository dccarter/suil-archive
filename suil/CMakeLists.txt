cmake_minimum_required(VERSION 3.5)
project(libsuil C CXX)

set(LIB_SUIL_BASE_SOURCES
        base.cpp
        buffer.cpp
        channel.cpp
        cmdl.cpp
        compress.cpp
        config.cpp
        console.cpp
        email.cpp
        base64.cpp
        file.cpp
        init.cpp
        json.cpp
        logging.cpp
        net.cpp
        process.cpp
        redis.cpp
        sock.cpp
        utils.cpp
        varint.cpp
        zstring.cpp)

set(LIB_SUIL_HTTP_SOURCES
        http/auth.cpp
        http/client.cpp
        http/cors.cpp
        http/fserver.cpp
        http/parser.cpp
        http/qstring.cpp
        http/request.cpp
        http/response.cpp
        http/routing.cpp
        http/wsock.cpp)

set(LIB_SUIL_SQL_SOURCES
        sql/pgsql.cpp)

# only include docker sources when enables
set(LIB_SUIL_DOCKER_SOURCES)

set(LIB_SUIL_SOURCES
        ${LIB_SUIL_BASE_SOURCES}
        ${LIB_SUIL_HTTP_SOURCES}
        ${LIB_SUIL_SQL_SOURCES})

set_property(GLOBAL PROPERTY prop_SUIL_SOURCES
        ${LIB_SUIL_BASE_SOURCES}
        ${LIB_SUIL_HTTP_SOURCES}
        ${LIB_SUIL_SQL_SOURCES})

# add a shared and a static library
add_library(suil SHARED
        ${LIB_SUIL_BASE_SOURCES}
        ${LIB_SUIL_HTTP_SOURCES}
        ${LIB_SUIL_SQL_SOURCES})

target_link_libraries(suil ${SUIL_LIBRARIES}   mill_s snappy lua)

# generate all symbols used in suil project
suil_iod_symbols(suil
        BINARY  ${CMAKE_BINARY_DIR}/deps/iod/iodsyms
        SYMBOLS ${CMAKE_SOURCE_DIR}/suil/suil.sym
        OUTPUT  ${CMAKE_SOURCE_DIR}/suil/symbols.h
        DEPENDS iodsyms)

# change output directory of suil
set_target_properties(suil
        PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

if (SUIL_BUILD_UNIT_TEST)
    get_property(_SUIL_SOURCES GLOBAL PROPERTY prop_SUIL_SOURCES)
    file(GLOB_RECURSE SUIL_TEST_SOURCES ../tests/*.cpp ../test/*.c)
    add_executable(sut ${_SUIL_SOURCES} ${SUIL_TEST_SOURCES})
    add_dependencies(sut suil-gensyms)
    target_link_libraries(sut ${SUIL_LIBRARIES} mill_s snappy lua)
    target_compile_definitions(sut PUBLIC -Dunit_test -Dsuil_ut=:public)
endif()

set(suil_INSTALL_TARGETS suil)

install(TARGETS ${suil_INSTALL_TARGETS}
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin)