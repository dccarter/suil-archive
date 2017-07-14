cmake_minimum_required(VERSION 3.5)
project(suil C CXX)

set(SUIL_MAJOR_VERSION 0 CACHE STRING "Build major version")
set(SUIL_MINOR_VERSION 0 CACHE STRING "Build minor version")
set(SUIL_PATCH_VERSION 0 CACHE STRING "Build patch version")
set(SUIL_BUILD_NUMBER  0 CACHE STRING "Build number version")
set(SUIL_BUILD_TAG     devel CACHE STRING "Version tag string")
set(SUIL_SOFTWARE_NAME suil CACHE STRING "The name of the server")

option(SUIL_UT_ENABLE "Enable unit testing" ON)

set(CMAKE_CXX_STANDARD 14)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DSUIL_DEBUG)
endif()

include(CheckSymbolExists)
include(CheckFunctionExists)

find_package(OpenSSL REQUIRED)
if (OpenSSL_FOUND)
    add_definitions("-DSSL_SUPPORTED")
endif()

# check and enable stack guard and dns if available
list(APPEND CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
set(CMAKE_REQUIRED_LIBRARIES)

check_function_exists(mprotect HAVE_MPROTECT)
if(HAVE_MPROTECT)
    add_definitions(-DHAVE_MPROTECT)
endif()

check_function_exists(posix_memalign HAVE_POSIX_MEMALIGN)
if(HAVE_POSIX_MEMALIGN)
    add_definitions(-DHAVE_POSIX_MEMALIGN)
endif()

# check and enable rt if available
list(APPEND CMAKE_REQUIRED_LIBRARIES rt)
check_symbol_exists(clock_gettime time.h HAVE_CLOCK_GETTIME)

# add dependencies
add_subdirectory(deps/libmill)
add_subdirectory(deps/iod)
include_directories(deps)

install(DIRECTORY deps/libmill DESTINATION include
        FILES_MATCHING PATTERN "*.h")

install(DIRECTORY deps/iod DESTINATION include
        FILES_MATCHING PATTERN "*.hh")
install(DIRECTORY deps/iod DESTINATION include
        FILES_MATCHING PATTERN "*.hpp")

install(DIRECTORY deps/boost DESTINATION include
        FILES_MATCHING PATTERN "*.hh")
install(DIRECTORY deps/boost DESTINATION include
        FILES_MATCHING PATTERN "*.hpp")

install(DIRECTORY suil DESTINATION include
        FILES_MATCHING PATTERN "*.hh")
install(DIRECTORY suil DESTINATION include
        FILES_MATCHING PATTERN "*.hpp")

# Configuration file
configure_file(
    ${CMAKE_SOURCE_DIR}/config.hpp.in
    ${CMAKE_BINARY_DIR}/config.hpp)
install(FILES ${CMAKE_BINARY_DIR}/config.hpp
        DESTINATION include/suil)
include_directories(${CMAKE_BINARY_DIR})

# Suil uses IOD which uses symbols for introspection,
# suil creates the symbols in a symbol files
set(SUIL_SYMBOLS
        ${CMAKE_SOURCE_DIR}/suil.sym)
add_custom_target(gensyms
        COMMAND ./deps/iod/iodsyms ${SUIL_SYMBOLS} ${CMAKE_SOURCE_DIR}/suil/symbols.h
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        DEPENDS ${SUIL_SYMBOLS}
        COMMENT "Generating IOD symbols used by suil")
add_dependencies(gensyms iodsyms)

include_directories(suil)
file(GLOB_RECURSE SUIL_SOURCES suil/*.cpp suil/*.c)
set(SUIL_LIBRARIES mill sqlite3)
add_library(suil ${SUIL_SOURCES})
add_dependencies(suil gensyms)
install(TARGETS suil
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin)

add_executable(scrap main.cpp)
target_link_libraries(scrap suil ${SUIL_LIBRARIES})

if (SUIL_UT_ENABLE)
    file(GLOB_RECURSE SUIL_TEST_SOURCES tests/*.cpp test/*.c)
    add_executable(sut ${SUIL_TEST_SOURCES})
    target_link_libraries(sut suil ${SUIL_LIBRARIES})
    target_compile_definitions(sut PUBLIC -DSUIL_TESTING)
endif()

set(CPACK_PACKAGE_NAME "suil")
set(CPACK_PACKAGE_VENDOR "Suil Team")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Carter Mbotho")
set(CPACK_PACKAGE_VERSION_MAJOR ${SUIL_MAJOR_VERSION})
set(CPACK_PACKAGE_VERSION_MINOR ${SUIL_MINOR_VERSION})
set(CPACK_PACKAGE_VERSION_PATCH ${SUIL_PATCH_VERSION})
set(CPACK_PACKAGE_VERSION ${SUIL_MAJOR_VERSION}.${SUIL_MINOR_VERSION}.${SUIL_PATCH_VERSION})
set(CPACK_PACKAGE_FILE_NAME ${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION})
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_SOURCE_DIR}/LICENSE)
set(CPACK_GENERATOR "TGZ;DEB")

include(CPack)
