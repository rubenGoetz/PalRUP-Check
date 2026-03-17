#!/bin/bash

# Clean out and navigate to build directory
cd "$(dirname "$0")/../.."
mkdir -p build
cd build

# build all targets
cmake ..
make

cd ..
