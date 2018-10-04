#define CATCH_CONFIG_RUNNER

#include <catch/catch.hpp>
#include <suil/sys.hpp>

int main(int argc, const char *argv[])
{
    suil::init(opt(printinfo, false));
    sinfo("starting Catch unit tests");
    int result = Catch::Session().run(argc, argv);
    return (result < 0xff ? result: 0xff);
}