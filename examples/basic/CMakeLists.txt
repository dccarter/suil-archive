cmake_minimum_required(VERSION 3.8)

set(SUIL_BASE_PATH "/home/dc/projects/dev/suil/build/artifacts" CACHE STRING "Path to an install of suil package")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${SUIL_BASE_PATH}/share/cmake/Modules")

set(CMAKE_CXX_STANDARD 17)
include(Suil)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(basic_VERSION        0.0.0 CACHE STRING     "Current application version")

set(basic_DEFINES
        "-DAPI_VERSION=\"${basic_VERSION}\"")

SuilProject(basic)

set(basic_SOURCES
        src/main.cc)

SuilApp(basic
        SOURCES   ${basic_SOURCES}
        VERSION   ${basic_VERSION}
        DEFINES   ${basic_DEFINES}
        INSTALL   ON
        INSTALL_DIRS res)

file(GLOB basic_TEST_SOURCES tests/*.cpp)
SuilTest(basic
        SOURCES ${basic_TEST_SOURCES} tests/main.cc
        VERSION ${basic_VERSION}
        DEFINES ${basic_DEFINES})
