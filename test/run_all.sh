#!/bin/bash
set -e
rm -Rf $PWD/test/out
mkdir -p $PWD/test/out
echo '[+] Testing ...'
rm -Rf $PWD/mqtt/driver.so
ln -s $PWD/build/mqtt/driver.so $PWD/mqtt/driver.so
for test_case in `ls $PWD/test | grep -e '$*\.lua'`; do
  echo "[+] Case: $test_case"
  tarantool $PWD/test/$test_case 2> $PWD/test/out/$test_case.out
  echo "[+] Case: $test_case -- OK"
done
