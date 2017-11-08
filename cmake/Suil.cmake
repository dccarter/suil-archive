##
# Suil project helper functions
##

# the base path where suil development library was installed
# if not installed in system
set(SUIL_BASE_PATH "" CACHE STRING "Base path on which suil was installed")

# Enable building debug project
option(SUIL_PROJECT_DEBUG "Enable building of debug projects" ON)

if (NOT SUIL_BASE_PATH)
    set(SUIL_BASE_PATH /usr/local/)
endif()
message(STATUS "Base path: ${SUIL_BASE_PATH}")

include(CheckSymbolExists)
include(CheckFunctionExists)

# Include some helper utilities
include(SuilUtils)
include(SuilConfigVersion)

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

        message(STATUS "using suil link directory: ${SUIL_BASE_PATH}/lib")
        link_directories(${SUIL_BASE_PATH}/lib)

        # find required packages
        find_package(PostgreSQL REQUIRED)
        find_package(OpenSSL REQUIRED)
        set(SUIL_LIBRARIES mill sqlite3 uuid ${PostgreSQL_LIBRARIES})

        # include all source
        include_directories(${SUIL_PACKAGE_INCLUDES})
        message(STATUS "using suil include directory: ${SUIL_BASE_PATH}/include")
        include_directories(${SUIL_BASE_PATH}/include)
        message(STATUS "Package includes: ${SUIL_PACKAGE_INCLUDES}")
        set(SUIL_PROJECT_INCLUDES ${SUIL_PACKAGE_INCLUDES} PARENT_SCOPE)

        # add definitions from suil
        add_definitions(${SUIL_PACKAGE_DEFINES})
        set(SUIL_PROJECT_DEFINES ${SUIL_PACKAGE_DEFINES} PARENT_SCOPE)

        # push suil libraries to parent scope
        set(SUIL_PROJECT_LIBRARIES ${SUIL_PACKAGE_LIBRARIES} PARENT_SCOPE)

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
    set(kvvargs VERSION SOURCES DEFINES LIBRARIES INCLUDES INSTALL_FILES INSTALL_DIRS)
    cmake_parse_arguments(SUIL_APP "${options}" "${kvargs}" "${kvvargs}" ${ARGN})

    # get the source files
    set(${name}_SOURCES ${SUIL_APP_SOURCES})
    if (NOT SUIL_APP_SOURCES)
        file(GLOB_RECURSE ${name}_SOURCES src/*.c src/*.cpp)
    endif()
    if (NOT ${name}_SOURCES)
        message(FATAL_ERROR "adding application '${name}' without sources")
    else()
        message(STATUS "target '${name} sources: ${${name}_SOURCES}")
    endif()

    # get application version
    set(${name}_VERSION ${SUIL_APP_VERSION})
    if (NOT ${name}_VERSION)
        set(${name}_VERSION "0.0.0")
    endif()

    # add the target
    add_executable(${name} ${${name}_SOURCES})

    # generate symbols
    set(${name}_SYMBOLS ${CMAKE_SOURCE_DIR}/${name}.sym)
    if (NOT ${name}_SYMBOLS)
        set(${name}_SYMBOLS symbols.sym)
    endif()
    message(STATUS "using symbols: ${${name}_SYMBOLS}")

    if (EXISTS ${${name}_SYMBOLS})
        # generate symbols if project uses symbols
        suil_iod_symbols(${name}
                BINARY ${SUIL_BASE_PATH}/bin/iodsyms
                SYMBOLS ${${name}_SYMBOLS})
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
    target_compile_definitions(${name} PUBLIC "-DAPP_VERSION=\"${${name}_VERSION}\"")
    target_compile_definitions(${name} PUBLIC "-DAPP_NAME=\"${name}\"")

    # add target include directories
    set(${name}_INCLUDES src includes)
    if (SUIL_APP_INCLUDES)
        set(${name}_INCLUDES ${${name}_INCLUDES} ${SUIL_APP_INCLUDES})
    endif()
    message(STATUS "target '${name}' includes: ${${name}_INCLUDES}")
    target_include_directories(${name} PUBLIC ${${name}_INCLUDES})

    # install the target
    set(${name}_INSTALL_FILES ${PROJECT_BINARY_DIR}/${name}/${name})
    if (SUIL_APP_INSTALL_FILES)
        message(STATUS "target '${name} install files: ${SUIL_APP_INSTALL_FILES}")
        install(FILES ${SUIL_APP_INSTALL_FILES}
                DESTINATION ${PROJECT_SOURCE_DIR}/artifacts/)
    endif()
    message(STATUS "target '${name} install target")
    install(TARGETS ${name}
            ARCHIVE DESTINATION ${PROJECT_SOURCE_DIR}/artifacts/
            LIBRARY DESTINATION ${PROJECT_SOURCE_DIR}/artifacts/
            RUNTIME DESTINATION ${PROJECT_SOURCE_DIR}/artifacts/)


    set(${name}_INSTALL_DIRS ${SUIL_APP_INSTALL_DIRS})
    if (${name}_INSTALL_DIRS)
        message(STATUS "target '${name} install directories: ${${name}_INSTALL_DIRS}")
        install(DIRECTORY ${${name}_INSTALL_DIRS}
                DESTINATION ${PROJECT_SOURCE_DIR}/artifacts/)
    endif()
endfunction()