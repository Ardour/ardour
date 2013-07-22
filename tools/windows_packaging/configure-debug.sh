#!/bin/bash

. ./mingw-env.sh

. ./print-env.sh

cd $BASE || exit 1
./waf configure --prefix="/" --bindir="/" --configdir="/share" --noconfirm --no-lv2 --test --single-tests --dist-target=mingw "$@"
