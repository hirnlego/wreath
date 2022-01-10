#!/bin/sh

clang++ -std=c++17 -stdlib=libc++ -o tests tests.cpp
./tests