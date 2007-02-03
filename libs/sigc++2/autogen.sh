#! /bin/sh

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

PKG_NAME="sigc++"

echo "Adding libtools."
libtoolize --automake --copy --force

echo "Building macros."
aclocal  -I "$srcdir/scripts" $ACLOCAL_FLAGS

#echo "Building config header."
#autoheader

echo "Building makefiles."
automake --add-missing --copy

echo "Building configure."
autoconf

rm -f config.cache
