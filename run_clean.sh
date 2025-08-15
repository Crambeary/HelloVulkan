#!/bin/bash
rm -rf build/*
# cmake --preset debug
cmake -G Ninja -S . -B build \
       -DCMAKE_BUILD_TYPE=Debug \
       -DCMAKE_PREFIX_PATH="$VULKAN_SDK/lib/cmake"
cmake --build build
cd build
./HelloVulkan
