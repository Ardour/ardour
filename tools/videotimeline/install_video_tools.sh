#!/bin/sh
TARGETDIR="$1"

if test -z "$(which curl)"; then
	echo "This script requires 'curl' - please install it" >&2
	exit 1
fi

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
echo "installing video-tools to '${TARGETDIR}'.."
exit
cd "$TARGETDIR" || exit 1

HARVID_VERSION=$(curl http://ardour.org/files/video-tools/harvid_version.txt)
curl -L http://ardour.org/files/video-tools/harvid-${MULTIARCH}-${HARVID_VERSION}.tgz \
	| tar -x -z --exclude=README --exclude=harvid.1 --strip-components=1 || exit 1
XJADEO_VERSION=$(curl http://ardour.org/files/video-tools/xjadeo_version.txt)
curl -L http://ardour.org/files/video-tools/xjadeo-${MULTIARCH}-${XJADEO_VERSION}.tgz \
	| tar -x -z --exclude=README --exclude=xjadeo.1 --strip-components=1 || exit 1
mv xjadeo xjremote

echo "ardour video tools installed successfully"
