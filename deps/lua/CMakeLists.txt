cmake_minimum_required(VERSION 3.10)
project(sci C)

set(CMAKE_C_STANDARD 11)

set(lua_SOURCES
        lapi.c
        lauxlib.c
        lbaselib.c
        lcode.c
        lcorolib.c
        lctype.c
        ldblib.c
        ldebug.c
        ldo.c
        ldump.c
        lfunc.c
        lgc.c
        linit.c
        liolib.c
        llex.c
        lmathlib.c
        lmem.c
        loadlib.c
        lobject.c
        lopcodes.c
        loslib.c
        lparser.c
        lstate.c
        lstring.c
        lstrlib.c
        ltable.c
        ltablib.c
        ltests.c
        ltm.c
        lundump.c
        lutf8lib.c
        lvm.c
        lzio.c)

set(lua_HEADERS
        luaconf.h
        lualib.h
        lauxlib.h
        lua.h
        lua.hpp)

add_library(lua STATIC ${lua_SOURCES})
target_compile_definitions(lua PRIVATE LUA_USE_POSIX)

set_target_properties(lua
        PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

# install the library file
install(TARGETS lua
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)

# install the header files
install(FILES ${lua_HEADERS} DESTINATION include/lua)

