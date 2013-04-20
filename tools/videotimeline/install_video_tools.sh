#!/bin/sh
TARGETDIR="$1"

if test -z "$(which curl)"; then
	echo "This script requires 'curl' - please install it" >&2
	exit 1
fi

###############################################################################
### look-up architecture

case $(uname -m) in
	i[3456789]86|x86|i86pc)
		echo "Architecture is x86"
		MULTIARCH="i386"
		;;
	x86_64|amd64|AMD64)
		echo "Architecture is x86_64"
		MULTIARCH="x86_64"
		;;
	*)
		echo
		echo "ERROR: Unknown architecture `uname -m`" >&2
		exit 1
		;;
esac

case $(uname) in
	Linux|linux)
		MULTIARCH="${MULTIARCH}-linux-gnu"
		;;
	*)
		echo
		echo "ERROR: Platform `uname` is not supported by this script" >&2
		exit 1
		;;
esac

echo "Multiarch triplet is '$MULTIARCH'"


###############################################################################
### install target directory

checkdir () {
  DUT="$1"
	CHECKPATH="${2:-yes}"
	ECHO="${3:-echo}"

	if test -z "$DUT"; then
		echo "-1"
		return
	fi

	if test ! -d "$DUT"; then
		$ECHO "ERROR: '$DUT' is not a directory'"; >&2
		echo "-1"
		return
	fi

	if test ! -w "$DUT"; then
		$ECHO "ERROR: no write permissions for '$DUT'" >&2
		echo "-1"
		return
	fi

	echo $PATH | grep -q "$DUT"
	if test $? != 0; then
		if test "$CHECKPATH" != "yes"; then
			$ECHO "WARNING: '$DUT' is not in \$PATH" >&2
		else
			$ECHO "ERROR: '$DUT' is not in \$PATH" >&2
			echo "-1"
			return
		fi
	fi

	echo 0
}

while test $(checkdir "$TARGETDIR" no) != 0 ; do

	ARDOUR=$(ls -td /opt/Ardour* 2>/dev/null | head -n 1)
	if test -n "${ARDOUR}" -a $(checkdir "${ARDOUR}/bin" no true) = 0; then
		echo -n "found ardour installation in '${ARDOUR}/bin'. Install there? [Y|n] "
		read a;
		if test "$a" != "n" -a "$a" != "N"; then
			TARGETDIR="${ARDOUR}/bin"
			continue
		fi
	fi

	if test $(checkdir "/usr/bin" yes true) = 0; then
		echo -n "Can write to '/usr/bin' Install there? [Y|n] "
		read a;
		if test "$a" != "n" -a "$a" != "N"; then
			TARGETDIR="/usr/bin"
			continue
		fi
	fi

	if test $(checkdir "${HOME}/bin" yes true) = 0; then
		echo -n "Found '${HOME}/bin' in PATH. Install there? [Y|n] "
		read a;
		if test "$a" != "n" -a "$a" != "N"; then
			TARGETDIR="${HOME}/bin"
			continue
		fi
	fi

	if test $(checkdir "/usr/local/bin" yes true) = 0; then
		echo -n "Can write to '/usr/local/bin' Install there? [Y|n] "
		read a;
		if test "$a" != "n" -a "$a" != "N"; then
			TARGETDIR="/usr/local/bin"
			continue
		fi
	fi

	echo
	echo "ERROR: Cannot find a suitable installation directory" >&2
	echo "run:  $0 /install/path/bin" >&2
	echo "'/install/path/bin' must be an existing directory and should be in \$PATH" >&2
	exit 1
done

###############################################################################
### actual install procedure

echo "installing video-tools to '${TARGETDIR}'."
cd "$TARGETDIR" || exit 1

HARVID_VERSION=$(curl -s http://ardour.org/files/video-tools/harvid_version.txt)
echo "Downloading harvid-${MULTIARCH}-${HARVID_VERSION}."
curl -L --progress-bar \
	http://ardour.org/files/video-tools/harvid-${MULTIARCH}-${HARVID_VERSION}.tgz \
	| tar -x -z --exclude=README --exclude=harvid.1 --strip-components=1 || exit 1

XJADEO_VERSION=$(curl -s http://ardour.org/files/video-tools/xjadeo_version.txt)
echo "Downloading xjadeo-${MULTIARCH}-${XJADEO_VERSION}."
curl -L --progress-bar \
	http://ardour.org/files/video-tools/xjadeo-${MULTIARCH}-${XJADEO_VERSION}.tgz \
	| tar -x -z --exclude=README --exclude=xjadeo.1 --strip-components=1 || exit 1
mv xjadeo xjremote

echo "ardour video tools installed successfully."
