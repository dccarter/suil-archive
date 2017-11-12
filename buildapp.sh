#!/bin/sh

# Used to build suil applications
echo 'build suil application `pwd`'

# Ensure that this indeed is a Cmake project
[ -f CMakeLists.txt ] || {
    echo "Current project is not a cmake project"
    exit -1
}

mkdir -p .build
cd .build

# run cmake
cmake $@ .. || {
    echo "cmake failed"
    exit 1
}

make -j2 install || {
    echo "building and installing application failed"
    exit
}

echo "application successfully built.."
exit 0