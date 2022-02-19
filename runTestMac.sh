#!/bin/sh

clang++ -std=c++17 -stdlib=libc++ -I./DaisySP/Source tests.cpp looper.cpp -o tests
./tests