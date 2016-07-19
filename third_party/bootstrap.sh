#!/bin/bash

cd mosquitto
mkdir -p build
cd build
cmake \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo
  -DWITH_SRV=off \
../
cd -
make libmosquitto -C build
