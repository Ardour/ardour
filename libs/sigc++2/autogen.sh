#! /bin/sh

# Copyright (c) 2006, The libsigc++ Development Team
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA


# Be Bourne compatible. (stolen from autoconf)
if test -n "${ZSH_VERSION+set}" && (emulate sh) >/dev/null 2>&1; then
  emulate sh
  NULLCMD=:
  # Zsh 3.x and 4.x performs word splitting on ${1+"$@"}, which
  # is contrary to our usage.  Disable this feature.
  alias -g '${1+"$@"}'='"$@"'
elif test -n "${BASH_VERSION+set}" && (set -o posix) >/dev/null 2>&1; then
  set -o posix
fi

PROJECT=libsigc++2
MIN_AUTOMAKE_VERSION=1.9

srcdir=`dirname "$0"`
test -n "$srcdir" || srcdir=.

origdir=`pwd`
cd "$srcdir"

LIBTOOLIZE_FLAGS="--automake --copy --force $LIBTOOLIZE_FLAGS"
ACLOCAL_FLAGS="-I scripts $ACLOCAL_FLAGS"
AUTOMAKE_FLAGS="--add-missing --copy $AUTOMAKE_FLAGS"

if test "x$*$AUTOGEN_SUBDIR_MODE" = x
then
  echo "I am going to run ./configure with no arguments -- if you wish"
  echo "to pass any to it, please specify them on the $0 command line."
fi

libtoolize=libtoolize
autoconf=autoconf
autoheader=autoheader
aclocal=
automake=
auto_version=0

# awk program to transform the output of automake --version
# into an integer value suitable for numeric comparison.
extract_version='{ printf "%.0f", 1000000 * v[split($1, v, " ")] + 1000 * $2 + $3; exit }'

for suffix in -1.7 -1.8 -1.9 ""
do
  aclocal_version=`aclocal$suffix --version </dev/null 2>/dev/null | awk -F. "$extract_version"`
  automake_version=`automake$suffix --version </dev/null 2>/dev/null | awk -F. "$extract_version"`

  if test "$aclocal_version" -eq "$automake_version" 2>/dev/null \
     && test "$automake_version" -ge "$auto_version" 2>/dev/null
  then
    auto_version=$automake_version
    aclocal=aclocal$suffix
    automake=automake$suffix
  fi
done

min_version=`echo "$MIN_AUTOMAKE_VERSION" | awk -F. "$extract_version"`

if test "$auto_version" -ge "$min_version" 2>/dev/null
then :; else
  echo "Sorry, at least automake $MIN_AUTOMAKE_VERSION is required to configure $PROJECT."
  exit 1
fi

rm -f config.guess config.sub depcomp install-sh missing mkinstalldirs
rm -f config.cache acconfig.h
rm -rf autom4te.cache

#WARNINGS=all
#export WARNINGS

if (set -x && set +x) >/dev/null 2>&1
then
  set_xtrace=set
else
  set_xtrace=:
fi

$set_xtrace -x

"$libtoolize" $LIBTOOLIZE_FLAGS	|| exit 1
"$aclocal" $ACLOCAL_FLAGS	|| exit 1
#"$autoheader"			|| exit 1
"$automake" $AUTOMAKE_FLAGS	|| exit 1
"$autoconf"			|| exit 1
cd "$origdir"			|| exit 1

if test -z "$AUTOGEN_SUBDIR_MODE"
then
  "$srcdir/configure" --enable-maintainer-mode ${1+"$@"} || exit 1
  $set_xtrace +x
  echo
  echo "Now type 'make' to compile $PROJECT."
fi

exit 0
