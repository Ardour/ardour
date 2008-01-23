#! /bin/sh

# check all tools first

LIBTOOLIZE=libtoolize

if /usr/bin/which libtoolize >/dev/null 2>&1 ; then 
    : 
else 
    if /usr/bin/which glibtoolize >/dev/null 2>&1 ; then
	LIBTOOLIZE=glibtoolize
    else
	echo "You do not have libtool installed, which is very sadly required to build part of Ardour" 
	exit 1
    fi
fi
if /usr/bin/which automake >/dev/null 2>&1 ; then 
    : 
else 
    echo "You do not have automake installed, which is very sadly required to build part of Ardour" 
    exit 1
fi
if /usr/bin/which autoconf >/dev/null 2>&1 ; then 
    : 
else 
    echo "You do not have autoconf installed, which is very sadly required to build part of Ardour" 
    exit 1
fi


srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

echo "Adding libtools."
$LIBTOOLIZE --automake --copy --force

echo "Building macros."
aclocal  -I "$srcdir/scripts" $ACLOCAL_FLAGS

echo "Building makefiles."
automake --add-missing --copy

echo "Building configure."
autoconf

rm -f config.cache
