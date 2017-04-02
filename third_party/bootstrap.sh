#!/bin/bash

set -e -x

cd mosquitto
git checkout v1.4.10
rm -r build
mkdir -p build
cd build

cmake \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DWITH_SRV=off \
  -DOPENSSL_ROOT_DIR=/usr/local/Cellar/openssl/1.0.2j/ ../
make -j2 libmosquitto

cd -
