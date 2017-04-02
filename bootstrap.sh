#!/bin/bash

set -e -x

# Deps
cd third_party
./bootstrap.sh
cd -

# Common
rm -r build && mkdir -p build && cd build

if [[ "$OSTYPE" == "darwin"* ]]; then
  LIBS=$PWD/../third_party/mosquitto/build/lib/libmosquitto.dylib
else
  LIBS=$PWD/../third_party/mosquitto/build/lib/libmosquitto.so
fi

cmake \
  -DMOSQUITTO_INCLUDE_DIR=$PWD/../third_party/mosquitto/lib/mosquitto.h \
  -DMOSQUITTO_LIBRARY=${LIBS}\
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
../

cd -
