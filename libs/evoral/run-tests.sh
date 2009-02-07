#!/bin/sh
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./build/default/
./waf && ./build/default/run-tests
