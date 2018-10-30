cmake_minimum_required(VERSION 3.8)

set(SUIL_BASE_PATH "" CACHE STRING "Path to an install of suil package")

if (SUIL_BASE_PATH)
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${SUIL_BASE_PATH}/share/cmake/Modules")
endif (SUIL_BASE_PATH)

set(CMAKE_CXX_STANDARD 17)
include(Suil)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(docker_VERSION        0.0.0 CACHE STRING     "Current application version")

set(docker_DEFINES
        "-DAPI_VERSION=\"${docker_VERSION}\"")

SuilProject(docker)

set(docker_SOURCES
        src/main.cc)

SuilApp(docker
        SOURCES   ${docker_SOURCES}
        VERSION   ${docker_VERSION}
        DEFINES   ${docker_DEFINES}
        LIBRARIES sdocker
        INSTALL   ON
        INSTALL_DIRS res)