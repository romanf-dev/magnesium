#!/bin/bash

for f in *.c; do
    gcc -O2 -std=c99 -Wextra -fanalyzer -pedantic -Wall -o $f.tst -I .. -I . $f 
done

for f in *.tst; do
    ./$f
    if [ $? -eq 0 ]; then
        echo $f: OK
    fi
done

rm -f *.tst

