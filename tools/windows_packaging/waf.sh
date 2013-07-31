#!/bin/bash

. ./mingw-env.sh

cd $BASE || exit 1
./waf "$@"
