cmake_minimum_required(VERSION 3.8)

if (SUIL_BASE_PATH)
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${SUIL_BASE_PATH}/share/cmake/Modules")
endif (SUIL_BASE_PATH)

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
