##
# Suil project helper functions
##

# the base path where suil development library was installed
# if not installed in system
set(SUIL_PATH_BASE "" CACHE STRING "Base path on which suil was installed")

# Enable building debug project
option(SUIL_PROJECT_DEBUG "Enable building of debug projects" ON)

if (${SUIL_PATH_BASE})
    # ensure that the base path will be searched for libraries
    # includes
    list(APPEND CMAKE_LIBRARY_PATH ${SUIL_PATH_BASE}/lib)
    include_directories(${SUIL_PATH_BASE}/include)
else()
    set(SUIL_PATH_BASE /usr/local/)
endif()

# Include some helper utilities
include(SuilUtils.cmake)
include(SuilConfigVersion.cmake)

# Ensure that some libraries exist
SuilCheckFunctions()
SuilCheckLibrary(uuid
        INCLUDE uuid/uuid.h)
SuilCheckLibrary(sqlite3
        LIBRARY sqlite3 libsqlite3)

##
# @brief Initialize a new suil project
#
# @param {name} the name of the project
##
function(SuilProject name)
    if (SUIL_PROJECT_CREATED)
        # only create project when not already created
        message(WARNING
                "A project instance has already been created: ${SUIL_APP_PROJECT_NAME}")
    else ()
        set(options "")
        set(kvargs  VERSION_MAJOR VERSION_MINOR VERSION_PATCH)
        set(kvvargs "")
        cmake_parse_arguments(SUIL_PROJECT "${options}" "${kvargs}" "${kvvargs}" ${ARGN})

        # initialize project
        project(${name} C CXX)

        # find required packages
        find_package(PostgreSQL REQUIRED)
        find_package(OpenSSL REQUIRED)
        set(SUIL_LIBRARIES mill sqlite3 uuid ${PostgreSQL_LIBRARIES})

        # include all source
        include_directories(${SUIL_PACKAGE_INCLUDES})
        set(SUIL_PROJECT_INCLUDES ${SUIL_PACKAGE_INCLUDES} PARENT_SCOPE)

        # add definitions from suil
        add_definitions(${SUIL_PACKAGE_DEFINES})
        set(SUIL_PROJECT_DEFINES ${SUIL_PACKAGE_DEFINES} PARENT_SCOPE)

        # push suil libraries to parent scope
        set(SUIL_PROJECT_LIBRARIES ${SUIL_PACKAGE_LIBRARIES} PARENT_SCOPE)

        # push suil debug library and file
        set(SUIL_PROJECT_DBG_MAIN
                ${SUIL_PATH_BASE}/share/suil/src/dbgshell.cpp
                PARENT_SCOPE)
        set(SUIL_PROJECT_DBG_LIB
                suildbgshell
                PARENT_SCOPE)

        # Some basic project definitions
        set(SUIL_PROJECT_NAME ${name} PARENT_SCOPE)
        set(SUIL_PROJECT_CREATED ON PARENT_SCOPE)
    endif()
endfunction()

##
# @brief add a suill application target
#
# @param {name:required} the name of the application
# @param {DEBUG:option:optional} if set, an executable debug
#        target ( ${name}dbg ) for debug purposes
# @param {SYMBOLS:file} a file with symbol definitions
# @param {SOURCES:list} a list of source files making up the
#        the application
# @param {DEFINES:list}  a list of extra defines
# @param {INCLUDES:list} a list of extra include directories
# @param {LIBRARIES:list} a list of extra libraries
# @param {INSTALL_FILES:list} a list of files to install
# @param {INSTALL_DIRS:list}  a list of directories to install
#
##
function(SuilApp name)
    if (NOT SUIL_PROJECT_CREATED)
        # cannot add an app without creating project
        message(FATAL_ERROR "cannot create an app before creating a project")
    endif()

    # parse function arguments
    set(options DEBUG)
    set(kvargs)
    set(kvvargs SOURCES DEFINES LIBRARIES INCLUDES INSTALL_FILES INSTALL_DIRS)
    cmake_parse_arguments(SUIL_APP "${options}" "${kvargs}" "${kvvargs}" ${ARGN})

    # get the source files
    set(${name}_SOURCES ${SUIL_APP_SOURCES})
    if (NOT SUIL_APP_SOURCES)
        file(GLOB_RECURSE ${name}_SOURCES ${name}/src/*.c ${name}/src/*.cpp)
    endif()
    if (NOT ${name}_SOURCES)
        message(FATAL_ERROR "adding application'${name} without sources")
    else()
        message(STATUS "target '${name} sources: ${${name}_SOURCES}")
    endif()

    # add the target
    add_library(${name} SHARED ${${name}_SOURCES})

    # generate symbols
    set(${name}_SYMBOLS ${SUIL_APP_SYMBOLS})
    if (NOT ${name}_SYMBOLS)
        set(${name}_SYMBOLS ${name}/symbols.sym)
    endif()
    if (EXISTS ${${name}_SYMBOLS})
        # generate symbols if project uses symbols
        suil_iod_symbols(${name}
                SYMBOLS ${${name}_SYMBOLS})
        add_dependencies(${name} ${name}-gensysms)
    endif()

    # add dependecy libraries
    set(${name}_LIBRARIES ${SUIL_PROJECT_LIBRARIES} ${SUIL_APP_LIBRARIES})
    message(STATUS "target '${name}' libraries: ${${name}_LIBRARIES}")
    target_link_libraries(${name} ${${name}_LIBRARIES})

    # add custom definitions if provided
    if (SUIL_APP_DEFINES)
        message(STATUS "target '${name}' extra defines: ${SUIL_APP_DEFINES}")
        target_compile_definitions(${name} PUBLIC ${SUIL_APP_DEFINES})
    endif()

    # add target include directories
    set(${name}_INCLUDES ${name}/src ${name}/includes)
    if (SUIL_APP_INCLUDES)
        set(${name}_INCLUDES ${${name}_INCLUDES} ${SUIL_APP_INCLUDES})
    endif()
    message(STATUS "target '${name}' includes: ${${name}_INCLUDES}")
    target_include_directories(${name} PUBLIC ${${name}_INCLUDES})

    # install the target
    set(${name}_INSTALL_FILES ${PROJECT_BINARY_DIR}/${name}/${name}.app)
    if (SUIL_APP_INSTALL_FILES)
        set(${name}_INSTALL_FILES ${${name}_INSTALL_FILES} ${SUIL_APP_INSTALL_FILES})
    endif()
    message(STATUS "target '${name} install files: ${${name}_INSTALL_FILES}")
    install(FILES ${${name}_INSTALL_FILES}
            DESTINATION ${PROJECT_SOURCE_DIR}/artifacts/${name}/)

    set(${name}_INSTALL_DIRS ${SUIL_APP_INSTALL_DIRS})
    if (${name}_INSTALL_DIRS)
        message(STATUS "target '${name} install directories: ${${name}_INSTALL_DIRS}")
        install(DIRECTORY ${${name}_INSTALL_DIRS}
                DESTINATION ${PROJECT_SOURCE_DIR}/artifacts/${name}/)
    endif()

    if (SUIL_APP_DEBUG AND SUIL_PROJECT_DEBUG)
        set(${name}dbg_SOURCES ${SUIL_PROJECT_DBG_MAIN})
        add_executable(${name}dbg ${${name}dbg_SOURCES})
        add_dependencies(${name}dbg ${${name}})
        target_link_libraries(${name}dbg
                ${name}
                ${SUIL_PROJECT_DBG_LIBRARY}
                ${${name}_LIBRARIES})
        target_compile_definitions(${name}dbg
                PUBLIC "-DSUIL_APP_NAME=\"${name}\"")
    endif()
endfunction()