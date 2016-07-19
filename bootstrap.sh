#!/bin/bash

set -e

cd third_party
./bootstrap.sh
cd -

mkdir -p build

cd build

cmake \
  -DMOSQUITTO_INCLUDE_DIR=$PWD/third_party/mosquitto/lib/mosquitto.h \
  -DMOSQUITTO_LIBRARY=$PWD/third_party/mosquitto/build/lib/libmosquitto.so \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
../

cd -
