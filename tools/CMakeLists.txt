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
        GENTPS_BIN  ${CMAKE_BINARY_DIR}/suil/tools/suiltps
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
        DEPENDS   suil)
