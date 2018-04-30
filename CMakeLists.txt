cmake_minimum_required(VERSION 3.8)
project(suil C CXX)

set(CMAKE_MODULE_PATH
        ${CMAKE_MODULE_PATH}
        "${CMAKE_SOURCE_DIR}/cmake/")

include(SuilUtils)

set(SUIL_MAJOR_VERSION 0 CACHE STRING "Build major version")
set(SUIL_MINOR_VERSION 0 CACHE STRING "Build minor version")
set(SUIL_PATCH_VERSION 0 CACHE STRING "Build patch version")
set(SUIL_BUILD_NUMBER  0 CACHE STRING "Build number version")
set(SUIL_BUILD_TAG     devel CACHE STRING "Version tag string")
set(SUIL_SOFTWARE_NAME suil  CACHE STRING "The name of the server")
set(SUIL_DEBUG_TAG     1a30f8248a6b11e781846f6e6c696e65 CACHE STRING "The tag used by the debug application tag")
set(SUIL_BASE_DIR     ".suil" CACHE STRING "The base directory of suil application")
set(SUIL_ENABLE_TRACE 1 CACHE STRING "Enable debug traces in debug builds")

set(SUIL_COPYRIGHT "Copyright (C) 2017, 2017-2017 Suilteam")
set(SUIL_VERSION ${SUIL_MAJOR_VERSION}.${SUIL_MINOR_VERSION}.${SUIL_PATCH_VERSION})

option(SUIL_BUILD_UNIT_TEST    "Enable unit testing" ON)
option(SUIL_BUILD_EXAMPLES     "Enable building examples" OFF)
option(SUIL_BUILD_DEV_PACKAGE  "Produce a development package" OFF)
option(SUIL_DEV_IDE            "Currently using an IDE" OFF)
set(SUIL_FILE_SERVER_ROUTE     1511d5908a7811e790ed6f6e6c696e65)

set(SUIL_DEFINES)
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(SUIL_BUILD_DEBUG ON)
    set(SUIL_DEFINES "${SUIL_DEFINES} -DSUIL_DEBUG")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-expansion-to-defined -Wno-implicit-fallthrough -Wno-unused-result -Wno-format-security")
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -Wno-expansion-to-defined -Wno-implicit-fallthrough -Wno-unused-result")
set(CMAKE_CXX_FLAGS_DEBUG   "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

include(CheckSymbolExists)
include(CheckFunctionExists)

SuilCheckFunctions()
SuilCheckLibrary(uuid INCLUDE uuid/uuid.h)
SuilCheckLibrary(sqlite3 LIBRARY sqlite3 libsqlite3)

# Configuration file
configure_file(
        ${CMAKE_SOURCE_DIR}/config.hpp.in
        ${CMAKE_BINARY_DIR}/.generated/suil/config.hpp)
include_directories(${CMAKE_BINARY_DIR}/.generated)

find_package(OpenSSL REQUIRED)
if (OpenSSL_FOUND)
    set(SUIL_DEFINES "${SUIL_DEFINES} -DSSL_SUPPORTED")
endif()

find_package(PostgreSQL ${SUIL_PGSQL_VERSION} REQUIRED)

set(SUIL_LIBRARIES
        ssl crypto uuid sqlite3 pq)

set(SUIL_STATIC_LIBRARIES
        ssl crypto uuid sqlite3 pq)

if (SUIL_BUILD_ABCI)
    find_package(Protobuf REQUIRED)
    set(SUIL_LIBRARIES ${SUIL_LIBRARIES} ${PROTOBUF_LIBRARIES})
    set(SUIL_STATIC_LIBRARIES ${SUIL_STATIC_LIBRARIES} protobuf.a)
endif()

message(STATUS "Suil dependecies ${SUIL_LIBRARIES}")
message(STATUS "Suil static dependecies ${SUIL_STATIC_LIBRARIES}")

add_definitions(${SUIL_DEFINES})

# add dependencies projects
add_subdirectory(deps/libmill)
add_subdirectory(deps/iod)
include_directories(deps)

if (SUIL_BUILD_DEV_PACKAGE)
    configure_file(buildapp.sh ${CMAKE_BINARY_DIR}/buildapp COPYONLY)
    configure_file(suilteam.ddns.net.crt ${CMAKE_BINARY_DIR}/suilteam.ddns.net.crt COPYONLY)
    install(DIRECTORY deps/libmill
            DESTINATION include
            FILES_MATCHING PATTERN "*.h")

    # Packaging
    if (SUIL_BUILD_ABCI)
        set(SUIL_PACKAGE_STATIC_LIBRARIES ${SUIL_STATIC_LIBRARIES} mill_s suil_s sabci_s)
        set(SUIL_PACKAGE_LIBRARIES ${SUIL_LIBRARIES} mill suil sabci)
    else()
        set(SUIL_PACKAGE_STATIC_LIBRARIES ${SUIL_STATIC_LIBRARIES} mill_s suil_s)
        set(SUIL_PACKAGE_LIBRARIES ${SUIL_LIBRARIES} mill suil)
    endif()

    set(SUIL_PACKAGE_INCLUDES  ${PostgreSQL_INCLUDE_DIRS})
    set(SUIL_PACKAGE_DEFINES   ${SUIL_DEFINES})

    # Create version configuration file
    configure_file(SuilVersion.cmake.in
            "${CMAKE_SOURCE_DIR}/cmake/SuilConfigVersion.cmake" @ONLY)

    install(DIRECTORY deps/iod
            DESTINATION include
            FILES_MATCHING PATTERN "*.hh")
    install(DIRECTORY deps/iod
            DESTINATION include
            FILES_MATCHING PATTERN "*.hpp")
    install(DIRECTORY deps/catch
            DESTINATION include
            FILES_MATCHING PATTERN "*.hh")
    install(DIRECTORY deps/catch
            DESTINATION include
            FILES_MATCHING PATTERN "*.hpp")
    install(DIRECTORY deps/boost
            DESTINATION include
            FILES_MATCHING PATTERN "*.h")
    install(DIRECTORY deps/boost
            DESTINATION include
            FILES_MATCHING PATTERN "*.hh")
    install(DIRECTORY deps/boost
            DESTINATION include
            FILES_MATCHING PATTERN "*.hpp")
    install(DIRECTORY suil/
            DESTINATION include/suil
            FILES_MATCHING PATTERN "*.hh")
    install(DIRECTORY suil/
            DESTINATION include/suil
            FILES_MATCHING PATTERN "*.hpp")
    install(DIRECTORY suil/
            DESTINATION include/suil
            FILES_MATCHING PATTERN "*.h")
    install(DIRECTORY cmake/
            DESTINATION share/cmake/Modules
            FILES_MATCHING PATTERN "*.cmake")
    install(FILES ${CMAKE_BINARY_DIR}/.generated/suil/config.hpp
            DESTINATION include/suil)
endif()

include_directories(${PostgreSQL_INCLUDE_DIRS})
include_directories(SYSTEM ${PROJECT_SOURCE_DIR})

add_subdirectory(suil)

add_executable(scrap main.cpp)
add_dependencies(scrap suil_s)
target_link_libraries(scrap ${SUIL_LIBRARIES} mill_s suil_s)

if (SUIL_BUILD_UNIT_TEST)
    get_property(_SUIL_SOURCES GLOBAL PROPERTY prop_SUIL_SOURCES)
    file(GLOB_RECURSE SUIL_TEST_SOURCES tests/*.cpp test/*.c)
    add_executable(sut ${_SUIL_SOURCES} ${SUIL_TEST_SOURCES})
    target_link_libraries(sut ${SUIL_LIBRARIES} mill_s)
    target_compile_definitions(sut PUBLIC -DSUIL_TESTING)
endif()

if (SUIL_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if (SUIL_BUILD_DEV_PACKAGE)
    message(STATUS "Development package enabled")
    set(SUIL_PACKAGE_POSTFIX dev)
else()
    message(STATUS "Production package enabled")
    set(SUIL_PACKAGE_POSTFIX prod)
endif()

if (SUIL_DEV_IDE)
#    add_subdirectory(apps/suil-web)
#    add_subdirectory(apps/molly)
#    add_subdirectory(apps/sodoin)
endif()

set(CPACK_PACKAGE_NAME   "suil")
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CPACK_PACKAGE_NAME "${CPACK_PACKAGE_NAME}dbg")
endif()
set(CPACK_PACKAGE_VENDOR "Suil Team")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Carter Mbotho")
set(CPACK_PACKAGE_VERSION_MAJOR ${SUIL_MAJOR_VERSION})
set(CPACK_PACKAGE_VERSION_MINOR ${SUIL_MINOR_VERSION})
set(CPACK_PACKAGE_VERSION_PATCH ${SUIL_PATCH_VERSION})
set(CPACK_PACKAGE_VERSION ${SUIL_MAJOR_VERSION}.${SUIL_MINOR_VERSION}.${SUIL_PATCH_VERSION}-${SUIL_BUILD_TAG})
set(CPACK_PACKAGE_FILE_NAME ${CPACK_PACKAGE_NAME}-${SUIL_PACKAGE_POSTFIX}_${CPACK_PACKAGE_VERSION})
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_SOURCE_DIR}/LICENSE)
set(CPACK_GENERATOR "TGZ")

include(CPack)
