cmake_minimum_required(VERSION 3.5)
project(libsuil C CXX)

file(GLOB_RECURSE LIB_SUIL_SOURCES *.cpp *.c)
add_library(suil STATIC ${LIB_SUIL_SOURCES})
target_link_libraries(suil ${SUIL_LIBRARIES})
target_compile_definitions(suil
        PUBLIC "-DSUIL_FILE_SERVER_ROUTE=\"1511d5908a7811e790ed6f6e6c696e65\"")
add_library(suil_s SHARED ${LIB_SUIL_SOURCES})
target_link_libraries(suil_s ${SUIL_LIBRARIES})
target_compile_definitions(suil_s
        PUBLIC "-DSUIL_FILE_SERVER_ROUTE=\"1511d5908a7811e790ed6f6e6c696e65\"")
set_target_properties(suil_s PROPERTIES OUTPUT_NAME suil)

suil_iod_symbols(suil
        BINARY  ${CMAKE_BINARY_DIR}/deps/iod/iodsyms
        SYMBOLS ${CMAKE_SOURCE_DIR}/suil/lib/suil.sym
        OUTPUT  ${CMAKE_SOURCE_DIR}/suil/lib/symbols.h
        DEPENDS iodsyms)

set(suil_INSTALL_TARGETS suil_s)
if (SUIL_BUILD_DEV_PACKAGE)
    list(APPEND suil_INSTALL_TARGETS suil_s)
endif()
install(TARGETS ${suil_INSTALL_TARGETS}
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin)
install(FILES suil.sql
        DESTINATION share/suil/src/sqlite)
install(FILES suil.sqlite
        DESTINATION share/suil/sqlite/)