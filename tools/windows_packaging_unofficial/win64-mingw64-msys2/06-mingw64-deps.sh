#!/bin/sh
# Run this in a MinGW64 MSYS2 shell.
#
# Some runtime dependencies which are needed.

export ARG1="${ARG1:-$1}"

mingw64_deps()
{
	SOURCE="${SOURCE:-.}"
	INCLUDE_SCRIPT="${INCLUDE_SCRIPT:-00-00-conf.sh}"
	$SOURCE "./$INCLUDE_SCRIPT"
	cd "$BUILD_DIR"
	wget http://ftp.gnome.org/pub/GNOME/sources/gtk-engines/2.20/gtk-engines-2.20.2.tar.gz
	tar zxvf gtk-engines-2.20.2.tar.gz
	cd gtk-engines-2.20.2
	./configure --build=x86_64-w64-mingw32 --host=x86_64-w64-mingw32 --prefix=/mingw64
	make
	make install
	cd ..
	cp "$SRC_REPO_NAME/build/libs/clearlooks-newer/clearlooks.dll" "/mingw64/lib/gtk-2.0/2.10.0/engines/libclearlooks.la"
}

mingw64_deps
