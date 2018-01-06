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
#set_target_properties(sabci_s PROPERTIES OUTPUT_NAME sabci)
target_link_libraries(sabci suil_s ${SUIL_LIBRARIES})
add_dependencies(sabci   suil_s)
add_dependencies(sabci_s suil_s)

set(sabci_INSTALL_TARGETS sabci)
if (SUIL_BUILD_DEV_PACKAGE)
    list(APPEND sabci_INSTALL_TARGETS sabci_s)
endif()
install(TARGETS ${sabci_INSTALL_TARGETS}
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin)