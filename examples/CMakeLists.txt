cmake_minimum_required(VERSION 3.8)
project(examples C CXX)

set(SUIL_BASE_PATH "" CACHE STRING "Path to an install of suil package")

# add all example projects
add_subdirectory(basic)
add_subdirectory(websock)
add_subdirectory(process)
if (SUIL_PACKAGE_HAVE_DOCKER)
    # Build docker example only when SDK has docker API support
    add_subdirectory(docker)
endif(SUIL_PACKAGE_HAVE_DOCKER)