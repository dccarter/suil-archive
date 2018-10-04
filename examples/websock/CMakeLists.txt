cmake_minimum_required(VERSION 3.8)

set(SUIL_BASE_PATH "/home/dc/projects/dev/suil/build/artifacts" CACHE STRING "Path to an install of suil package")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${SUIL_BASE_PATH}/share/cmake/Modules")

set(CMAKE_CXX_STANDARD 17)
include(Suil)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(websock_VERSION        0.0.0 CACHE STRING     "Current application version")

set(websock_DEFINES
        "-DAPI_VERSION=\"${websock_VERSION}\""
        "-DCHAT_HTML=\"${CMAKE_CURRENT_SOURCE_DIR}/res/chat.html\"")

SuilProject(websock)

set(websock_SOURCES
        src/main.cc)

SuilApp(websock
        SOURCES   ${websock_SOURCES}
        VERSION   ${websock_VERSION}
        DEFINES   ${websock_DEFINES}
        INSTALL   ON
        INSTALL_DIRS res)

file(GLOB websock_TEST_SOURCES tests/*.cpp)
SuilTest(websock
        SOURCES ${websock_TEST_SOURCES} tests/main.cc
        VERSION ${websock_VERSION}
        DEFINES ${websock_DEFINES})
