//
// Created by dc on 01/10/18.
//

#include "sapp.hpp"

namespace suil::tools  {

    static void add_SourcesFolder()
    {
        utils::fs::mkdir("src");
        const char *mainFile = "src/main.cc";
        sinfo("generating file %s ...", mainFile);

        File main(mainFile, O_WRONLY|O_CREAT|O_APPEND, 0666);
        // write stub code
        main << "#include <suil/sys.hpp>\n"
             << "#include <suil/cmdl.hpp>\n"
             << "\n"
             << "using namespace suil;\n"
             << "\n"
             << "int main(int argc, char *argv[])\n"
             << "{\n"
             << "    suil::init(opt(printinfo, false));\n"
             << "    log::setup(opt(verbose,4));\n"
             << "    cmdl::Parser parser(APP_NAME, APP_VERSION, \"\");\n"
             << "\n"
             << "    try\n"
             << "    {\n"
             << "        parser.parse(argc, argv);\n"
             << "        parser.handle(); \n"
             << "    }\n"
             << "    catch(...)\n"
             << "    {\n"
             << "        fprintf(stderr, \"error: %s\\n\", exmsg());\n"
             << "        return EXIT_FAILURE;\n"
             << "    }\n"
             << "\n"
             << "    return EXIT_SUCCESS;\n"
             << "}";
        main.close();
    }

    static void add_TestsFolder()
    {
        utils::fs::mkdir("tests");
        const char *mainFile = "tests/main.cc";
        sinfo("generating file %s ...", mainFile);

        File main(mainFile, O_WRONLY|O_CREAT|O_APPEND, 0666);
        // write stub code
        main << "#define CATCH_CONFIG_RUNNER\n"
             << "\n"
             << "#include <catch/catch.hpp>\n"
             << "#include <suil/sys.hpp>\n"
             << "\n"
             << "int main(int argc, const char *argv[])\n"
             << "{\n"
             << "    suil::init(opt(printinfo, false));\n"
             << "    sinfo(\"starting Catch unit tests\");\n"
             << "    int result = Catch::Session().run(argc, argv);\n"
             << "    return (result < 0xff ? result: 0xff);\n"
             << "}";

        main.close();
    }

    static void add_CMakeLists(const zcstring& name, const zcstring& basePath)
    {
        const char *cmakeFile = "CMakeLists.txt";
        sinfo("generating file %s ...", cmakeFile);
        File cmake(cmakeFile, O_WRONLY|O_CREAT|O_APPEND, 0666);

        cmake << "cmake_minimum_required(VERSION 3.8)\n"
              << "\n";
        if (!basePath.empty())
            cmake << "set(SUIL_BASE_PATH \"" << basePath << "\" CACHE STRING \"Path to an install of suil package\")\n";
        else
            cmake << "set(SUIL_BASE_PATH \"\" CACHE STRING \"Path to an install of suil package\")\n"
                  << "\n";
        cmake << "if (SUIL_BASE_PATH)\n"
              << "    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} \"${SUIL_BASE_PATH}/share/cmake/Modules\")\n"
              << "endif (SUIL_BASE_PATH)\n"
              << "\n";

        cmake << "set(CMAKE_CXX_STANDARD 17)\n"
              << "include(Suil)\n"
              << "include_directories(${CMAKE_CURRENT_SOURCE_DIR})\n"
              << "\n"
              << "set(" << name << "_VERSION        0.0.0 CACHE STRING     \"Current application version\")\n"
              << "\n"
              << "set(" << name << "_DEFINES\n"
              << "        \"-DAPI_VERSION=\\\"${" << name << "_VERSION}\\\"\")\n"
              << "\n"
              << "SuilProject(" << name  << ")\n"
              << "\n"
              << "set(" << name << "_SOURCES\n"
              << "        src/main.cc)\n"
              << "\n"
              << "SuilApp(" << name << "\n"
              << "        SOURCES   ${" << name << "_SOURCES}\n"
              << "        VERSION   ${" << name << "_VERSION}\n"
              << "        DEFINES   ${" << name << "_DEFINES}\n"
              << "        INSTALL   ON\n"
              << "        INSTALL_DIRS res)\n"
              << "\n"
              << "file(GLOB " << name << "_TEST_SOURCES tests/*.cpp)\n"
              << "SuilTest(" << name << "\n"
              << "        SOURCES ${" << name << "_TEST_SOURCES} tests/main.cc\n"
              << "        VERSION ${" << name << "_VERSION}\n"
              << "        DEFINES ${" << name << "_DEFINES})\n";

        cmake.close();
    }

    void suil_InitProjectTemplate(const zcstring& name, const zcstring& basePath)
    {
        auto currdir = utils::fs::currdir();
        if (!utils::fs::isdirempty(currdir())) {
            // suil will not override current directory contents
            throw SuilError::create("current directory '", currdir, "' is not empty");
        }

        auto projectName = name.peek();
        if (projectName.empty()) {
            // use the name of the directory for project name
            projectName = utils::fs::getname(currdir());
        }

        auto suilBasePath = basePath.peek();
        if (suilBasePath.empty() && utils::fs::exists(BASE_PATH)) {
            // use base path specified on project
            suilBasePath = zcstring{BASE_PATH};
        }

        sinfo("initializing project %s", projectName());
        strace("using base path %s", suilBasePath());

        // create resources directory
        utils::fs::mkdir("res");
        // add base project files
        add_CMakeLists(projectName, suilBasePath);
        add_SourcesFolder();
        add_TestsFolder();
    }
}