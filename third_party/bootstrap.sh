#!/bin/bash

cd mosquitto
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DWITH_SRV=off ../
make libmosquitto
cd -
