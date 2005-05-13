#!/bin/sh

aclocal $ACLOCAL_FLAGS && autoheader && automake --add-missing && autoconf

