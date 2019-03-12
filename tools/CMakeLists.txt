cmake_minimum_required(VERSION 3.8)

set(SUIL_BASE_PATH "${CMAKE_BINARY_DIR}/artifacts")

set(CMAKE_CXX_STANDARD 17)
include(Suil)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
set(tools_VERSION ${SUIL_PACKAGE_VERSION} CACHE STRING "Current application version")
set(tools_DEFINES
        "-DTOOL_VERSION=${tools_VERSION}")

if (SUIL_BUILD_DEV_PACKAGE)
    set(_SCC_BIN     ${CMAKE_BINARY_DIR}/tools/scc)
    set(_IOD_BIN     ${CMAKE_BINARY_DIR}/deps/iod/iodsyms)
    set(_GENTPS_BIN  ${CMAKE_BINARY_DIR}/tools/suiltps)
endif()

SuilProject(tools
        SCC_BIN     ${_SCC_BIN}
        IOD_BIN     ${_IOD_BIN}
        GENTPS_BIN  ${_GENTPS_BIN}
        SYMBOLS     ${CMAKE_CURRENT_SOURCE_DIR}/tools.sym)

set(suiltps_SOURCES
        typegen/main.cc)

SuilApp(suiltps
        SOURCES   ${suiltps_SOURCES}
        VERSION   ${tools_VERSION}
        DEFINES   ${tools_DEFINES}
        INSTALL   ON
        DEPENDS   suil)

set(scc_SOURCES
        scc/main.cpp
        scc/mpc.c
        scc/program.cpp
        scc/parser.cpp)

SuilApp(scc
        SOURCES    ${scc_SOURCES}
        VERSION    ${tools_VERSION}
        DEFINES    ${tools_DEFINES} "-DGRAMMAR_DIR=\"${CMAKE_CURRENT_LIST_DIR}\""
        INSTALL    ON
        DEPENDS    suil)

set(suilapp_SOURCES
        sapp/cmd_init.cpp
        sapp/main.cc)

SuilApp(suilapp
        SOURCES    ${suilapp_SOURCES}
        VERSION    ${tools_VERSION}
        DEFINES    ${tools_DEFINES} "-DBASE_PATH=\"${SUIL_BASE_PATH}\""
        INSTALL    ON
        DEPENDS    suil)

if (SUIL_BUILD_LXC)
    # Build docker only requested to do so
    set(lxc_SOURCES
            lxc/lxc.cpp
            lxc/lxc.scc.cpp)

    SuilStatic(suilxc
            SOURCES      ${lxc_SOURCES}
            VERSION      ${tools_VERSION}
            DEFINES      ${tools_DEFINES}
            INSTALL      ON
            LIBRARIES    lxc
            SCC_SOURCES  ${CMAKE_CURRENT_SOURCE_DIR}/lxc/lxc.scc
            SCC_OUTDIR   ${CMAKE_CURRENT_SOURCE_DIR}/lxc)

    set(scc_HEADERS
            ${CMAKE_CURRENT_SOURCE_DIR}/lxc/lxc.scc.h
            ${CMAKE_CURRENT_SOURCE_DIR}/lxc/lxc.h)
    install(FILES ${sdocker_HEADERS}
            DESTINATION include/suil/lxc/)

    SuilApp(suilxc-demo
            SOURCES    lxc/main.cpp
            VERSION    ${tools_VERSION}
            DEFINES    ${tools_DEFINES} "-DBASE_PATH=\"${SUIL_BASE_PATH}\""
            INSTALL    ON
            DEPENDS    suil
            LIBRARIES  lxc suilxc)
endif(SUIL_BUILD_LXC)