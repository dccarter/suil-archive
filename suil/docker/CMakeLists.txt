cmake_minimum_required(VERSION 3.8)
project(suil-docker C CXX)

file(GLOB_RECURSE LIB_SUIL_DOCKER_SOURCES *.cpp *.c)

add_executable(sdocker ${LIB_SUIL_DOCKER_SOURCES} main.cc)
# generate symbols
suil_gen_types(sdocker
        BINARY  ${CMAKE_BINARY_DIR}/suil/tools/suilgen
        SCHEMAS ${CMAKE_CURRENT_SOURCE_DIR}/types.json
        OUTPUT  ${CMAKE_CURRENT_SOURCE_DIR}/types.hpp)
target_link_libraries(sdocker suil_s ${SUIL_LIBRARIES})
add_library(sdocker_s STATIC ${LIB_SUIL_DOCKER_SOURCES})
#set_target_properties(sabci_s PROPERTIES OUTPUT_NAME sabci)
target_link_libraries(sdocker suil_s ${SUIL_LIBRARIES})
add_dependencies(sdocker   suil_s)
add_dependencies(sdocker_s suil_s)

set(sdocker_INSTALL_TARGETS sdocker_s)
install(TARGETS ${sdocker_INSTALL_TARGETS}
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin)