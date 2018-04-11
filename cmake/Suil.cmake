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
        set(kvvargs STATIC_LINK LINK_DIRS)
        cmake_parse_arguments(SUIL_PROJECT "${options}" "${kvargs}" "${kvvargs}" ${ARGN})

        # initialize project
        project(${name} C CXX)

        message(STATUS "using suil link directory: ${SUIL_BASE_PATH}/lib")
        link_directories(${SUIL_BASE_PATH}/lib)

        # find required packages
        find_package(PostgreSQL REQUIRED)
        find_package(OpenSSL REQUIRED)

        # include all source
        include_directories(${SUIL_PACKAGE_INCLUDES})
        message(STATUS "using suil include directory: ${SUIL_BASE_PATH}/include")
        include_directories(${SUIL_BASE_PATH}/include)
        message(STATUS "Package includes: ${SUIL_PACKAGE_INCLUDES}")
        set(SUIL_PROJECT_INCLUDES ${SUIL_PACKAGE_INCLUDES} PARENT_SCOPE)

        # add definitions from suil
        add_definitions(${SUIL_PACKAGE_DEFINES})
        set(SUIL_PROJECT_DEFINES ${SUIL_PACKAGE_DEFINES} PARENT_SCOPE)

        ## push suil libraries to parent scope
        message(STATUS "Pushing link libraries in build mode ${CMAKE_BUILD_TYPE}")
        # Executable linking mode
        if (NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Os" PARENT_SCOPE)
            set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -Os" PARENT_SCOPE)
        endif()

        set(SUIL_PROJECT_LIBRARIES ${SUIL_PACKAGE_LIBRARIES} PARENT_SCOPE)
        set(SUIL_PROJECT_STATIC_LIBRARIES ${SUIL_PACKAGE_STATIC_LIBRARIES} PARENT_SCOPE)
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
# @param {INSTALL_FILES:list} a list of files to install (if INSTALL is enabled)
# @param {INSTALL_DIRS:list}  a list of directories to install (if INSTALL is enabled)
# @param {DEPENDS:list} a list of other target that the application depends on
# @param {INSTALL:ON|OFF} enable install targets, files & directories
##
function(SuilApp name)
    if (NOT SUIL_PROJECT_CREATED)
        # cannot add an app without creating project
        message(FATAL_ERROR "cannot create an app before creating a project")
    endif()

    # parse function arguments
    set(options DEBUG IODSYMS_PATHBIN)
    set(kvargs)
    set(kvvargs LIBRARY DEPENDS INSTALL VERSION SOURCES TEST DEFINES SYMBOLS
                EXTRA_SYMS LIBRARIES INCLUDES INSTALL_FILES INSTALL_DIRS ARTIFACTS_DIR)
    cmake_parse_arguments(SUIL_APP "${options}" "${kvargs}" "${kvvargs}" ${ARGN})

    # get the source files
    set(${name}_SOURCES ${SUIL_APP_SOURCES})
    if (NOT SUIL_APP_SOURCES)
        file(GLOB_RECURSE ${name}_SOURCES src/*.c src/*.cpp src/*.cc)
        if (SUIL_APP_TEST)
            file(GLOB_RECURSE ${name}_TEST_SOURCES test/*.c test/*.cpp test/*.cc)
            set(${name}_SOURCES ${${name}_SOURCES} ${${name}_TEST_SOURCES})
        endif()
    endif()

    if (NOT ${name}_SOURCES)
        message(FATAL_ERROR "adding application '${name}' without sources")
    else()
        message(STATUS "target '${name} sources: ${${name}_SOURCES}")
    endif()

    # get application version
    set(${name}_VERSION ${SUIL_APP_VERSION})
    if (NOT SUIL_APP_VERSION)
        set(${name}_VERSION "0.0.0")
    endif()
    message(STATUS "version configured to: ${${name}_VERSION}")

    if (SUIL_APP_LIBRARY)
        message(STATUS "configuring target ${name} as a ${SUIL_APP_LIBRARY} library")
        add_library(${name} ${SUIL_APP_LIBRARY} ${${name}_SOURCES})
        target_compile_definitions(${name} PUBLIC "-DLIB_VERSION=\"${${name}_VERSION}\"")
    else()
        # add the target
        message(STATUS "configuring target ${name} as ${SUIL_APP_LIBRARY} an executable")
        add_executable(${name} ${${name}_SOURCES})
        target_compile_definitions(${name} PUBLIC "-DAPP_VERSION=\"${${name}_VERSION}\"")
        target_compile_definitions(${name} PUBLIC "-DAPP_NAME=\"${name}\"")
        target_compile_definitions(${name} PUBLIC "-DLIB_VERSION=\"${${name}_VERSION}\"")
    endif()

    # generate symbols
    set(${name}_SYMBOLS ${SUIL_APP_SYMBOLS})
    if (NOT SUIL_APP_SYMBOLS)
        set(${name}_SYMBOLS ${CMAKE_CURRENT_SOURCE_DIR}/${name}.sym)
    endif()

    if (EXISTS ${${name}_SYMBOLS})
        message(STATUS "using symbols: ${${name}_SYMBOLS} ${SUIL_APP_EXTRA_SYMS}")
        set(IODSYMS_BIN iodsyms)
        if (SUIL_APP_IODSYMS_PATHBIN)
            set(IODSYMS_BIN ${SUIL_BASE_PATH}/bin/iodsyms)
        endif()
        # generate symbols if project uses symbols
        GET_FILENAME_COMPONENT(${name}_SYMBOLS_OUTPUT ${${name}_SYMBOLS} NAME)
        suil_iod_symbols(${name}
                BINARY ${IODSYMS_BIN}
                SYMBOLS ${${name}_SYMBOLS} ${SUIL_APP_EXTRA_SYMS}
                OUTPUT  ${CMAKE_CURRENT_SOURCE_DIR}/${${name}_SYMBOLS_OUTPUT}.h)
    endif()

    if (SUIL_APP_DEPENDS)
        message(STATUS "adding dependencies to ${name}: ${SUIL_APP_DEPENDS}")
        add_dependencies(${name} ${SUIL_APP_DEPENDS})
    endif()

    if (SUIL_APP_TEST)
        list(APPEND SUIL_APP_DEFINES "-DSUIL_TESTING")
        set(${name}_LIBRARIES ${SUIL_PROJECT_LIBRARIES} ${SUIL_APP_LIBRARIES})
    else()
        set(${name}_LIBRARIES ${SUIL_PROJECT_STATIC_LIBRARIES} ${SUIL_APP_LIBRARIES})
    endif()
    # add custom definitions if provided
    if (SUIL_APP_DEFINES)
        message(STATUS "target '${name}' extra defines: ${SUIL_APP_DEFINES}")
        target_compile_definitions(${name} PUBLIC ${SUIL_APP_DEFINES})
    endif()
    # add dependecy libraries
    target_link_libraries(${name} ${${name}_LIBRARIES})
    message(STATUS "target '${name}' libraries: ${${name}_LIBRARIES}")


    # add target include directories
    set(${name}_INCLUDES src includes)
    if (SUIL_APP_INCLUDES)
        set(${name}_INCLUDES ${${name}_INCLUDES} ${SUIL_APP_INCLUDES})
    endif()
    message(STATUS "target '${name}' includes: ${${name}_INCLUDES}")
    target_include_directories(${name} PUBLIC ${${name}_INCLUDES})
    include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if (SUIL_APP_TEST )
        include_directories(${CMAKE_CURRENT_SOURCE_DIR}/test)
    endif()

    if (SUIL_APP_INSTALL)
        message(STATUS "target install is enabled")
        # Get the artifacts directory
        set(${name}_ARTIFACTS_DIR ${SUIL_APP_ARTIFACTS_DIR})
        if (NOT SUIL_APP_ARTIFACTS_DIR)
            set(${name}_ARTIFACTS_DIR ${name})
        endif ()

        # install the files
        if (SUIL_APP_INSTALL_FILES)
            message(STATUS "target '${name} install files: ${SUIL_APP_INSTALL_FILES}")
            install(FILES ${SUIL_APP_INSTALL_FILES}
                    DESTINATION ${${name}_ARTIFACTS_DIR})
        endif()

        message(STATUS "target '${name} install target")
        install(TARGETS ${name}
                ARCHIVE DESTINATION targets/lib
                LIBRARY DESTINATION targets/lib
                RUNTIME DESTINATION targets/bin)


        set(${name}_INSTALL_DIRS ${SUIL_APP_INSTALL_DIRS})
        if (${name}_INSTALL_DIRS)
            message(STATUS "target '${name} install directories: ${${name}_INSTALL_DIRS}")
            install(DIRECTORY ${${name}_INSTALL_DIRS}
                    DESTINATION ${${name}_ARTIFACTS_DIR})
        endif()
    else()
        message(STATUS "target install is disabled")
    endif()
endfunction()

macro(SuilTest name)
    SuilApp(${name}-test
            ${ARGN}
            TEST    ON
            INSTALL OFF)
endmacro()

macro(SuilStatic name)
    SuilApp(${name}
            ${ARGN}
            LIBRARY STATIC)
endmacro()

macro(SuilShared name)
    SuilApp(${name}
            ${ARGN}
            LIBRARY SHARED)
endmacro()

function(ProtoGen proto)
    # parse function arguments
    set(options)
    set(kvargs)
    set(kvvargs OUTDIR WORKINGDIR)
    cmake_parse_arguments(PROTOC "${options}" "${kvargs}" "${kvvargs}" ${ARGN})

    if (NOT PROTOC_OUTDIR)
        set(PROTOC_OUTDIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
    endif()

    if (NOT PROTOC_WORKINGDIR)
        set(PROTOC_WORKINGDIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
    endif()

    execute_process(
            COMMAND protoc --cpp_out ${PROTOC_OUTDIR} ${proto}
            WORKING_DIRECTORY ${PROTOC_WORKINGDIR}
            RESULT_VARIABLE PROTOGEN_RESULT
            OUTPUT_VARIABLE PROTOGEN_OUTPUT)

    if (RESULT_VARIABLE)
        message(FATAL_ERROR "protogen: ${PROTOGEN_OUTPUT}")
    endif()
endfunction()