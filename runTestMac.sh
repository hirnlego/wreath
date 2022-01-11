#!/bin/sh

clang++ -std=c++17 -stdlib=libc++ -I../DaisyExamples/DaisySP/Source tests.cpp looper.cpp -o tests
./tests