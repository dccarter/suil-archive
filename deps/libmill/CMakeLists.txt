cmake_minimum_required(VERSION 3.5)
project(libmill C)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")

# static and shared libs
file(GLOB sources *.c dns/*.c)
set_source_files_properties(dns/dns.c PROPERTIES COMPILE_FLAGS -std=c99)
add_library(mill_s STATIC ${sources})
add_library(mill   SHARED ${sources})
#set_target_properties(mill_s PROPERTIES OUTPUT_NAME mill)
set_target_properties(
        mill_s PROPERTIES POSITION_INDEPENDENT_CODE ON)

set_target_properties(mill mill_s
        PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

if(HAVE_CLOCK_GETTIME)
  target_link_libraries(mill rt)
  target_link_libraries(mill_s rt)
endif()
if (OpenSSL_FOUND)
    target_link_libraries(mill   ${OPENSSL_LIBRARIES})
    target_link_libraries(mill_s ${OPENSSL_LIBRARIES})
endif()

set(mill_INSTALL_TARGETS mill_s)
if (SUIL_BUILD_DEV_PACKAGE)
    list(APPEND mill_INSTALL_TARGETS mill)
endif()
# Installation and packaging with cpack
install(TARGETS ${mill_INSTALL_TARGETS}
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)