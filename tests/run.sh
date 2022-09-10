#!/bin/bash

(cd .. && make) >& /dev/null || { echo 'make failed!'; }

mkdir -p out
for index in {1..1}; do
    [ -f in/$index.s ] || continue
    echo test $index
    ./xparse in/$index.s >& out/$index.out
    diff good/$index.out out/$index.out || { echo '--- FAILED ---'; exit 1; }
done
echo DONE
