#!/bin/bash

set -e -x

mosquitto &
for test_case in `ls $PWD/test | grep -e '$*\.lua'`; do
  $PWD/test/$test_case
  echo "[+] Case: $test_case -- OK"
done
kill -s TERM %1 || echo "it's ok"
echo "[+] Done"
