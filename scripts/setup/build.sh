#!/bin/bash

# Clean out and navigate to build directory
cd "$(dirname "$0")/../.."
mkdir -p build
cd build

# build all targets
cmake .. -DCMAKE_BUILD_TYPE=DEBUG -DDRUP_TO_LRUP_CONVERSION=1
make -j

cd ..
