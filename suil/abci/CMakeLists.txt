cmake_minimum_required(VERSION 3.8)
project(suil-abci C CXX)


execute_process(
        COMMAND protoc --cpp_out ${CMAKE_CURRENT_SOURCE_DIR} abci.proto
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        RESULT_VARIABLE PROTOGEN_RESULT
        OUTPUT_VARIABLE PROTOGEN_OUTPUT)

if (RESULT_VARIABLE)
    message(FATAL_ERROR "protogen: ${PROTOGEN_OUTPUT}")
endif()
file(GLOB_RECURSE LIB_SUIL_ABCI_SOURCES *.cpp *.c)
set(LIB_SUIL_ABCI_SOURCES ${LIB_SUIL_ABCI_SOURCES} abci.pb.cc)

add_executable(sabci ${LIB_SUIL_ABCI_SOURCES} main.cc)
target_link_libraries(sabci suil_s ${SUIL_LIBRARIES})
add_library(sabci_s STATIC ${LIB_SUIL_ABCI_SOURCES})
target_link_libraries(sabci suil_s ${SUIL_LIBRARIES})