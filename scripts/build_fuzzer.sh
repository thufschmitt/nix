#!/usr/bin/env bash

export BUILD_DEBUG=1

./bootstrap.sh
./configure $configureFlags --prefix="$(pwd)/outputs/out"
make fuzzer CXXFLAGS=-O1 OPTIMIZE=0
