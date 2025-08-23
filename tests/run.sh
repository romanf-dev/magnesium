#!/bin/bash

error() {
    rm -f *.test
    exit 1
}

for f in *.c; do
    gcc -O2 -std=c99 -Wextra -fanalyzer -pedantic -Wall -o $f.test -I .. -I . $f 
    if [ $? -ne 0 ]; then error 
    fi
    ./$f.test
    if [ $? -ne 0 ]; then error 
    fi
    echo $f: OK
done

rm -f *.test

