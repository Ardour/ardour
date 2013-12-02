#!/bin/bash

. ./mingw-env.sh

export CC="distcc $HOST-gcc"
export CPP="distcc $HOST-g++"
export CXX="distcc $HOST-g++"

. ./print-env.sh

cd $BASE || exit 1
./waf configure --prefix="/" --bindir="/" --configdir="/share" --noconfirm --no-lv2 --test --single-tests --dist-target=mingw "$@"
