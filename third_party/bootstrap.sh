#!/bin/bash

set -e -x

cd mosquitto
git checkout v1.5
rm -rf build
mkdir -p build
cd build

if [[ "$OSTYPE" == "darwin"* ]]; then
  cmake \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DWITH_SRV=off \
    -DOPENSSL_ROOT_DIR=/usr/local/Cellar/openssl/1.0.2o_2/ ../
else
  cmake \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DWITH_SRV=off
fi

make -j2 libmosquitto

cd -
