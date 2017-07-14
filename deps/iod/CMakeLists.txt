cmake_minimum_required(VERSION 3.5)
project(iod)

set(CMAKE_CXX_STANDARD 14)

add_executable(iodsyms tools/iod_generate_symbols.cc)

install(TARGETS iodsyms DESTINATION bin)
