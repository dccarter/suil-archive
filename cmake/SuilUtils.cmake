##
# file with utility macros
##

##
# @brief generate IOD symbols
##
function(suil_iod_symbols name)
    set(options "")
    set(kvargs  BINARY OUTPUT PROJECT)
    set(kvvargs DEPENDS SYMBOLS)

    cmake_parse_arguments(IOD_SYMBOLS "${options}" "${kvargs}" "${kvvargs}" ${ARGN})

    # locate binary to use
    set(iodsyms iodsyms)
    if (IOD_SYMBOLS_BINARY)
        set(iodsyms ${IOD_SYMBOLS_BINARY})
    endif()

    # locate symbols file
    set(${name}_SYMBOLS ${CMAKE_SOURCE_DIR}/${name}.sym)
    if (IOD_SYMBOLS_SYMBOLS)
        set(${name}_SYMBOLS ${IOD_SYMBOLS_SYMBOLS})
    endif()

    # set output path
    set(${name}_OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/${name}_symbols.h)
    if (IOD_SYMBOLS_OUTPUT)
        set(${name}_OUTPUT ${IOD_SYMBOLS_OUTPUT})
    endif()

    list(GET ${name}_SYMBOLS 0 ${name}_MAIN_SYM)
    list(REMOVE_AT ${name}_SYMBOLS 0)
    set(${name}_FLAT_SYMS ${${name}_MAIN_SYM})
    foreach(__${name}_SYM ${${name}_SYMBOLS})
        set(${name}_FLAT_SYMS "${${name}_FLAT_SYMS} ${__${name}_SYM}")
    endforeach()

    message(STATUS "${name} symbols. main: ${${name}_MAIN_SYM} flat: ${${name}_FLAT_SYMS}")

    add_custom_command(OUTPUT ${${name}_OUTPUT}
            COMMAND ${iodsyms} ${${name}_MAIN_SYM} ${${name}_SYMBOLS} ${${name}_OUTPUT}
            WORKING_DIRECTORY  ${CMAKE_BINARY_DIR}
            DEPENDS            ${${name}_MAIN_SYM} ${${name}_SYMBOLS})
    # add symbol generation target
    add_custom_target(${name}-gensyms
            DEPENDS            ${${name}_OUTPUT}
            COMMENT            "Generating IOD symbols used by ${name}")
    message(STATUS "${iodsyms} ${${name}_FLAT_SYMS} ${${name}_OUTPUT}")

    if (IOD_SYMBOLS_DEPENDS)
        add_dependencies(${name}-gensyms ${IOD_SYMBOLS_DEPENDS})
    endif()

    if (NOT IOD_SYMBOLS_PROJECT)
        add_dependencies(${name} ${name}-gensyms)
    endif()
endfunction()

function(suil_gen_types name)
    set(options "")
    set(kvargs  BINARY OUTPUT PROJECT)
    set(kvvargs DEPENDS SCHEMAS)

    cmake_parse_arguments(TYPES_SCHEMA "${options}" "${kvargs}" "${kvvargs}" ${ARGN})

    # locate binary to use
    set(suilgen suiltps)
    if (TYPES_SCHEMA_BINARY)
        set(suilgen ${TYPES_SCHEMA_BINARY})
    endif()

    # locate symbols file
    set(${name}_SCHEMAS ${CMAKE_SOURCE_DIR}/${name}.schema)
    if (TYPES_SCHEMA_SCHEMAS)
        set(${name}_SCHEMAS ${TYPES_SCHEMA_SCHEMAS})
    endif()

    # set output path
    set(${name}_OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/${name}.types.hpp)
    if (TYPES_SCHEMA_OUTPUT)
        set(${name}_OUTPUT ${TYPES_SCHEMA_OUTPUT})
    endif()

    list(GET ${name}_SCHEMAS 0 ${name}_MAIN_SCHEMA)
    list(REMOVE_AT ${name}_SCHEMAS 0)
    set(${name}_FLAT_SCHEMAS ${${name}_MAIN_SCHEMA})
    foreach(__${name}_SCHEMA ${${name}_SCHEMAS})
        set(${name}_FLAT_SCHEMAS "${${name}_FLAT_SCHEMAS} ${__${name}_SCHEMA}")
    endforeach()

    message(STATUS "${name} schemas. main: ${${name}_MAIN_SCHEMA} flat: ${${name}_FLAT_SCHEMAS}")

    add_custom_command(OUTPUT ${${name}_OUTPUT}
            COMMAND ${suilgen} "gen" "-i" ${${name}_MAIN_SCHEMA} "-o" ${${name}_OUTPUT}
            WORKING_DIRECTORY  ${CMAKE_BINARY_DIR}
            DEPENDS            ${${name}_MAIN_SCHEMA})
    # add symbol generation target
    add_custom_target(${name}-gentps
            DEPENDS ${${name}_OUTPUT}
            COMMENT "Generating types used by ${name}")
    message(STATUS "${suilgen} ${${name}_FLAT_SCHEMAS} ${${name}_OUTPUT}")

    if (TYPES_SCHEMA_DEPENDS)
        add_dependencies(${name}-gentps ${TYPES_SCHEMA_DEPENDS})
    endif()

    if (NOT TYPES_SCHEMA_PROJECT)
        add_dependencies(${name} ${name}-gentps)
    endif()
endfunction()

function(suil_scc name)
    set(options "")
    set(kvargs  BINARY OUTDIR PROJECT)
    set(kvvargs DEPENDS SOURCES)

    cmake_parse_arguments(SUIL_SCC "${options}" "${kvargs}" "${kvvargs}" ${ARGN})

    # locate binary to use
    set(suilscc scc)
    if (SUIL_SCC_BINARY)
        set(suilscc ${SUIL_SCC_BINARY})
    endif()

    # locate symbols file
    set(${name}_SOURCES ${SUIL_SCC_SOURCES})
    if (NOT SUIL_SCC_SOURCES)
        set(${name}_SOURCES ${CMAKE_SOURCE_DIR}/${name}.scc)
    endif()

    # set output directory
    set(${name}_OUTDIR ${CMAKE_CURRENT_BINARY_DIR}/.generated)
    if (SUIL_SCC_OUTDIR)
        set(${name}_OUTDIR ${SUIL_SCC_OUTDIR})
    endif()

    # get outputs
    set(${name}_OUTPUTS)
    foreach(__${name}_SOURCE ${${name}_SOURCES})
        get_filename_component(__temp ${__${name}_SOURCE} NAME)
        list(APPEND ${name}_OUTPUTS
                ${${name}_OUTDIR}/${__temp}.h
                ${${name}_OUTDIR}/${__temp}.cpp)
    endforeach()

    list(JOIN SUIL_SCC_SOURCES "," ${name}_SOURCES)
    message(STATUS "${name} scc sources: ${${name}_SOURCES}")
    message(STATUS "${name} scc outputs: ${${name}_OUTPUTS}")

    add_custom_command(OUTPUT ${${name}_OUTPUTS}
            COMMAND ${suilscc} "gen" "-i" ${${name}_SOURCES} "-O" ${${name}_OUTDIR}
            WORKING_DIRECTORY  ${CMAKE_BINARY_DIR}
            DEPENDS            ${${name}_SOURCES})
    # add symbol generation target
    add_custom_target(${name}-scc
            DEPENDS ${${name}_OUTPUTS}
            COMMENT "Generating types/services used by ${name}")
    message(STATUS "${suilscc} ${${name}_SOURCES} ${${name}_OUTDIR}")

    if (SUIL_SCC_DEPENDS)
        add_dependencies(${name}-scc ${SUIL_SCC_DEPENDS})
    endif()

    if (NOT SUIL_SCC_PROJECT)
        add_dependencies(${name} ${name}-scc)
    endif()
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