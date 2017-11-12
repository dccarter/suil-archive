cmake_minimum_required(VERSION 3.8)
project(examples C CXX)

macro(AddExample name)
    file(GLOB_RECURSE ${name}_SOURCES ${name}/*.c ${name}/*.cpp)
    add_executable(${name} ${${name}_SOURCES})
    target_link_libraries(${name} suil ${SUIL_LIBRARIES})
endmacro()

AddExample(hello-world)
AddExample(basic)