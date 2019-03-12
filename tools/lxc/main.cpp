//
// Created by dc on 27/02/19.
//
#include <suil/init.h>
#include "lxc.h"
using namespace suil;

int main(int argc, char *argv[])
{
    suil::init(opt(printinfo, false));
    auto containers = lxc::listAllContainers("/home/dc/.local/share/lxc");
    auto& demo = containers.begin()->second;
    sinfo("%s - %s", demo.getName()(), demo.getState()());
    return EXIT_SUCCESS;
}