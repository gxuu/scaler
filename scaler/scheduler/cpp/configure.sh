#!/usr/bin/bash -e

rm -rf build
CC=/usr/bin/clang CXX=/usr/bin/clang++ cmake -GNinja -S . -B build -DCMAKE_BUILD_TYPE=Debug
