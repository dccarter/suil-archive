#include <suil/sys.hpp>
#include <suil/cmdl.hpp>
#include <sdocker/docker.hpp>

using namespace suil;

int main(int argc, char *argv[])
{
    suil::init(opt(printinfo, false));
    log::setup(opt(verbose,2));
    docker::Docker Docker("localhost",4243);
    if (!Docker.connect()) {
        // connection to docker failed
        serror("connection to docker failed");
    }

    Docker.Images.get("postgres", "image.tar");
    auto obj = Docker.Container.ps(opt(all, true));

    return EXIT_SUCCESS;
}