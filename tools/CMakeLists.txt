cmake_minimum_required(VERSION 3.8)

set(SUIL_BASE_PATH "${CMAKE_BINARY_DIR}/artifacts")

set(CMAKE_CXX_STANDARD 17)
include(Suil)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(tools_VERSION SUIL_PACKAGE_VERSION CACHE STRING "Current application version")

set(tools_DEFINES
        "-DTOOL_VERSION=${tools_VERSION}")

SuilProject(tools
        IOD_BIN     ${CMAKE_BINARY_DIR}/deps/iod/iodsyms
        GENTPS_BIN  ${CMAKE_BINARY_DIR}/tools/suiltps
        SYMBOLS     ${CMAKE_CURRENT_SOURCE_DIR}/tools.sym)

set(suiltps_SOURCES
        typegen/main.cc)

SuilApp(suiltps
        SOURCES   ${suiltps_SOURCES}
        VERSION   ${tools_VERSION}
        DEFINES   ${tools_DEFINES}
        INSTALL   ON
        DEPENDS   suil)

set(suilapp_SOURCES
        sapp/cmd_init.cpp
        sapp/main.cc)

SuilApp(suilapp
        SOURCES    ${suilapp_SOURCES}
        VERSION    ${tools_VERSION}
        DEFINES    ${tools_DEFINES} "-DBASE_PATH=\"${SUIL_BASE_PATH}\""
        INSTALL    ON
        DEPENDS    suil)

set(sdocker_SOURCES
        sdocker/docker.cpp
        sdocker/containers.cpp
        sdocker/images.cpp
        sdocker/networks.cpp
        sdocker/volumes.cpp
        sdocker/exec.cpp)

SuilStatic(sdocker
        SOURCES    ${sdocker_SOURCES}
        VERSION    ${tools_VERSION}
        DEFINES    ${tools_DEFINES}
        INSTALL    ON
        TPSCHEMA   ${CMAKE_CURRENT_SOURCE_DIR}/sdocker/types.json
        DEPENDS    suiltps)
set(sdocker_HEADERS
        ${CMAKE_CURRENT_SOURCE_DIR}/sdocker/types.json.h
        ${CMAKE_CURRENT_SOURCE_DIR}/sdocker/docker.hpp)
install(FILES ${sdocker_HEADERS}
        DESTINATION include/sdocker)
