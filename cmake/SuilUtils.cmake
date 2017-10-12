##
# file with utility macros
##

##
# @brief generate IOD symbols
##
function(suil_iod_symbols name)
    set(options "")
    set(kvargs  BINARY SYMBOLS OUTPUT)
    set(kvvargs DEPENDS)

    cmake_parse_arguments(IOD_SYMBOLS "${options}" "${kvargs}" "${kvvargs}" ${ARGN})

    # locate binary to use
    set(iodsyms iodsyms)
    if (IOD_SYMBOLS_BINARY)
        set(iodsyms ${IOD_SYMBOLS_BINARY})
    endif()

    # locate symbols file
    set(${name}_SYMBOLS ${CMAKE_SOURCE_DIR}/${name}/${name}.sym)
    if (IOD_SYMBOLS_SYMBOLS)
        set(${name}_SYMBOLS ${IOD_SYMBOLS_SYMBOLS})
    endif()

    # set output path
    set(${name}_OUTPUT ${CMAKE_SOURCE_DIR}/${name}/symbols.h)
    if (IOD_SYMBOLS_OUTPUT)
        set(${name}_OUTPUT ${IOD_SYMBOLS_OUTPUT})
    endif()

    # add symbol generation target
    add_custom_target(${name}-gensyms
            COMMAND ${iodsyms} ${${name}_SYMBOLS} ${${name}_OUTPUT}
            WORKING_DIRECTORY  ${CMAKE_BINARY_DIR}
            DEPENDS            ${${name}_SYMBOLS}
            COMMENT            "Generating IOD symbols used by ${name}")
    message(STATUS "${iodsyms} ${${name}_SYMBOLS} ${${name}_OUTPUT}")

    if (IOD_SYMBOLS_DEPENDS)
        add_dependencies(${name}-gensyms ${IOD_SYMBOLS_DEPENDS})
    endif()

    add_dependencies(${name} ${name}-gensyms)
endfunction()

##
# @brief Check to ensure that a system library exists in the system
# @param {name:string:required} the name of the library
# @param {INCLUDE:file} the include file to look for
# @patra {LIBRARY:name} the name of the library in the system
##
function(SuilCheckLibrary name)
    set(kvvargs INCLUDE LIBRARY)
    cmake_parse_arguments(SUIL_CHECK "" "" "${kvvargs}" ${ARGN})
    if(NOT SUIL_CHECK_INCLUDE)
        set(SUIL_CHECK_INCLUDE ${name}.h)
    endif()
    if(NOT SUIL_CHECK_LIBRARY)
        set(SUIL_CHECK_LIBRARY ${name} lib${name})
    endif()
    message(STATUS
            "finding library '${name}' inc: ${SUIL_CHECK_INCLUDE}, lib: ${SUIL_CHECK_LIBRARY}")

    find_path(${name}_INCLUDE_DIR
            NAMES ${SUIL_CHECK_INCLUDE})
    find_path(${name}_LIBRARY
            NAMES ${SUIL_CHECK_LIBRARY}
            HINTS /usr/lib/x86_64-linux-gnu/)

    message(STATUS
            "library '${name}' inc: ${${name}_INCLUDE_DIR}, lib:${${name}_LIBRARY}")

    if ((NOT ${name}_LIBRARY) OR (NOT ${name}_INCLUDE_DIR))
        message(FATAL_ERROR "${name} libary missing in build system")
    endif()
endfunction()

function(SuilCheckFunctions)
    # check and enable stack guard and dns if available
    list(APPEND CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
    set(CMAKE_REQUIRED_LIBRARIES)

    check_function_exists(mprotect MPROTECT_FOUND)
    if(MPROTECT_FOUND)
        add_definitions(-DHAVE_MPROTECT)
    endif()

    check_function_exists(posix_memalign POSIX_MEMALIGN_FOUND)
    if(POSIX_MEMALIGN_FOUND)
        add_definitions(-DHAVE_POSIX_MEMALIGN)
    endif()

    # check and enable rt if available
    list(APPEND CMAKE_REQUIRED_LIBRARIES rt)
    check_symbol_exists(clock_gettime time.h CLOCK_GETTIME_FOUND)
    if (CLOCK_GETTIME_FOUND)
        add_definitions(-DHAVE_CLOCK_GETTIME)
    endif()
endfunction()