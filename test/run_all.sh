#!/bin/bash
set -e
mkdir -p $PWD/tests/out
echo '[+] Testing ...'
[ ! -f $PWD/mqtt/driver.so ] && \
  ln -s $PWD/build/mqtt/driver.so $PWD/mqtt/driver.so
for test_case in `ls $PWD/tests | grep -e '$*\.lua'`; do
  echo "[+] Case: $test_case"
  $PWD/tests/$test_case 2> $PWD/tests/out/$test_case.out
  echo "[+] Case: $test_case -- OK"
done
