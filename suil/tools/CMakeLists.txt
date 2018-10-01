cmake_minimum_required(VERSION 3.8)
project(suil-tools C CXX)


add_executable(suilgen typegen.cc)
# generate symbols
suil_iod_symbols(suilgen
        BINARY  ${CMAKE_BINARY_DIR}/deps/iod/iodsyms
        SYMBOLS ${CMAKE_CURRENT_SOURCE_DIR}/tools.sym
        OUTPUT  ${CMAKE_CURRENT_SOURCE_DIR}/tools_symbols.h)

# Add target
target_link_libraries(suilgen suil_s ${SUIL_LIBRARIES})
add_dependencies(suilgen suil_s)

set(stools_INSTALL_TARGETS suilgen)
install(TARGETS ${stools_INSTALL_TARGETS}
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin)