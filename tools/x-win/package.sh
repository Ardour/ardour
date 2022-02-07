#!/bin/bash

# we assume this script is <ardour-src>/tools/x-win/package.sh
pushd "`/usr/bin/dirname \"$0\"`" > /dev/null; this_script_dir="`pwd`"; popd > /dev/null
cd $this_script_dir

. ../define_versions.sh

cd $this_script_dir/../..

test -f gtk2_ardour/wscript || exit 1

# Defaults (overridden by environment)
: ${XARCH=i686} # or x86_64
: ${ROOT=/home/ardour}
: ${MAKEFLAGS=-j4}
: ${TMPDIR=/var/tmp}
: ${SRCCACHE=/var/tmp/winsrc}  # source-code tgz cache

: ${HARRISONCHANNELSTRIP=harrison_channelstrip}
: ${HARRISONLV2=harrison_lv2s-n}
: ${HARRISONDSPURL=https://rsrc.harrisonconsoles.com/plugins/releases/public}

# see also wscript, video_tool_paths.cc, bundle_env_mingw.cc
# registry keys based on this are used there
PROGRAM_NAME=Ardour
PROGRAM_KEY=Ardour
PROGRAM_VERSION=${major_version}

PRODUCT_NAME=Ardour
PRODUCT_VERSION=${major_version}

WITH_HARRISON_LV2=1 ;
WITH_COMMERCIAL_X42_LV2=
WITH_GRATIS_X42_LV2=

# TODO: grep from build/config.log instead
while [ $# -gt 0 ] ; do
	echo "arg = $1"
	case $1 in
		--mixbus)
			MIXBUS=1
			WITH_HARRISON_LV2=1 ;
			WITH_COMMERCIAL_X42_LV2=1
			WITH_GRATIS_X42_LV2=1
			PROGRAM_NAME=Mixbus
			PROGRAM_KEY=Mixbus
			PRODUCT_NAME=Mixbus
			MANUAL_NAME="mixbus${major_version}-live-manual"
			shift ;;
		--mixbus32c)
			MIXBUS=1
			WITH_HARRISON_LV2=1 ;
			WITH_COMMERCIAL_X42_LV2=1
			WITH_GRATIS_X42_LV2=1
			PRODUCT_NAME=Mixbus32C
			PROGRAM_KEY=Mixbus32C
			PROGRAM_NAME=Mixbus32C-${PROGRAM_VERSION}
			PROGRAM_VERSION=""
			MANUAL_NAME="mixbus32c-${major_version}-live-manual"
			shift ;;
		--chanstrip) HARRISONCHANNELSTRIP=$2 ; shift; shift ;;
	esac
done


LOWERCASE_DIRNAME=ardour${major_version}
STATEFILE_SUFFIX=ardour # see filename_extensions.cc


# derived variables
PRODUCT_ID=${PROGRAM_NAME}${PROGRAM_VERSION}
PRODUCT_EXE=${PRODUCT_NAME}.exe
PRODUCT_ICON=${PRODUCT_NAME}.ico

###############################################################################

echo "Packaging $PRODUCT_ID"

if test "$XARCH" = "x86_64" -o "$XARCH" = "amd64"; then
	echo "Target: 64bit Windows (x86_64)"
	XPREFIX=x86_64-w64-mingw32
	WARCH=w64
else
	echo "Target: 32 Windows (i686)"
	XPREFIX=i686-w64-mingw32
	WARCH=w32
fi

: ${PREFIX=${ROOT}/win-stack-$WARCH}
export SRCCACHE

if [ "$(id -u)" = "0" ]; then
	apt-get -y install nsis curl wget
fi


function download {
echo "--- Downloading.. $2"
test -f ${SRCCACHE}/$1 || curl -k -L -o ${SRCCACHE}/$1 $2
}

################################################################################
set -e

ARDOURVERSION=${release_version}
ARDOURDATE=$(date -R)
if ! test -f build/gtk2_ardour/ardour-${ARDOURVERSION}.exe; then
	echo "*** Please compile  ardour-${ARDOURVERSION}.exe first."
	exit 1
fi

echo " === bundle to $DESTDIR"

./waf install

################################################################################

if test -z "$DESTDIR"; then
	DESTDIR=`mktemp -d`
	trap 'rm -rf $DESTDIR' exit SIGINT SIGTERM
	rm -rf $DESTDIR
fi

echo " === bundle to $DESTDIR"

ALIBDIR=$DESTDIR/lib/${LOWERCASE_DIRNAME}

mkdir -p $DESTDIR/bin
mkdir -p $DESTDIR/share/
mkdir -p $ALIBDIR/surfaces
mkdir -p $ALIBDIR/backends
mkdir -p $ALIBDIR/panners
mkdir -p $ALIBDIR/vamp
mkdir -p $ALIBDIR/suil

cp build/libs/gtkmm2ext/gtkmm2ext-*.dll $DESTDIR/bin/
cp build/libs/midi++2/midipp-*.dll $DESTDIR/bin/
cp build/libs/evoral/evoral-*.dll $DESTDIR/bin/
cp build/libs/ardour/ardour-*.dll $DESTDIR/bin/
cp build/libs/temporal/temporal-*.dll $DESTDIR/bin/
cp build/libs/canvas/canvas-*.dll $DESTDIR/bin/
cp build/libs/widgets/widgets-*.dll $DESTDIR/bin/
cp build/libs/waveview/waveview-*.dll $DESTDIR/bin/
cp build/libs/pbd/pbd-*.dll $DESTDIR/bin/
cp build/libs/ptformat/ptformat-*.dll $DESTDIR/bin/
cp build/libs/audiographer/audiographer-*.dll $DESTDIR/bin/
cp build/libs/fst/ardour-vst-scanner.exe $DESTDIR/bin/ || true
cp build/libs/fst/ardour-vst3-scanner.exe $DESTDIR/bin/ || true
cp build/session_utils/*-*.exe $DESTDIR/bin/ || true
cp build/luasession/ardour*-lua.exe $DESTDIR/bin/ || true
cp `ls -t build/gtk2_ardour/ardour-*.exe | head -n1` $DESTDIR/bin/${PRODUCT_EXE}

mkdir -p $DESTDIR/lib/gtk-2.0/engines
cp build/libs/clearlooks-newer/clearlooks.dll $DESTDIR/lib/gtk-2.0/engines/libclearlooks.la

cp $PREFIX/bin/*.dll $DESTDIR/bin/
cp $PREFIX/bin/*.yes $DESTDIR/bin/ || true
cp $PREFIX/lib/*.dll $DESTDIR/bin/
# special case libportaudio (wasapi), old stack has no wasapi and hence no .xp
cp $PREFIX/bin/libportaudio-2.xp $DESTDIR/bin/ || cp $PREFIX/bin/libportaudio-2.dll $DESTDIR/bin/libportaudio-2.xp

# prefer system-wide DLL
rm -rf $DESTDIR/bin/libjack*.dll
# Also for these (even though M$ recommends to bundle these [1],
# there is no single set that works on all target systems, particularly
# since some plugins also rely on it.
# [1] https://docs.microsoft.com/en-us/windows/win32/debug/calling-the-dbghelp-library
rm -rf $DESTDIR/bin/dbghelp*.dll
rm -rf $DESTDIR/bin/dbgcore*.dll

cp `find build/libs/surfaces/ -iname "*.dll"` $ALIBDIR/surfaces/
cp `find build/libs/backends/ -iname "*.dll"` $ALIBDIR/backends/
cp `find build/libs/panners/ -iname "*.dll"` $ALIBDIR/panners/

cp -r build/libs/LV2 $ALIBDIR/
cp -r build/libs/vamp-plugins/*ardourvampplugins*.dll $ALIBDIR/vamp/libardourvampplugins.dll
cp -r build/libs/vamp-pyin/*ardourvamppyin*.dll $ALIBDIR/vamp/libardourvamppyin.dll
cp $PREFIX/lib/suil-*/*.dll $ALIBDIR/suil/ || true

# lv2 core, classifications
for file in $PREFIX/lib/lv2/*.lv2; do
	BN=$(basename $file)
	mkdir -p $ALIBDIR/LV2/$BN
	cp $PREFIX/lib/lv2/${BN}/*.ttl $ALIBDIR/LV2/${BN}/
done


mv $ALIBDIR/surfaces/ardourcp*.dll $DESTDIR/bin/

# TODO use -static-libgcc -static-libstdc++ -- but for .exe files only
if update-alternatives --query ${XPREFIX}-gcc | grep Value: | grep -q win32; then
	cp /usr/lib/gcc/${XPREFIX}/*-win32/libgcc_s_*.dll $DESTDIR/bin/
	cp /usr/lib/gcc/${XPREFIX}/*-win32/libstdc++-6.dll $DESTDIR/bin/
elif update-alternatives --query ${XPREFIX}-gcc | grep Value: | grep -q posix; then
	cp /usr/lib/gcc/${XPREFIX}/*-posix/libgcc_s_*.dll $DESTDIR/bin/
	cp /usr/lib/gcc/${XPREFIX}/*-posix/libstdc++-6.dll $DESTDIR/bin/
else
	cp /usr/lib/gcc/${XPREFIX}/*/libgcc_s_sjlj-1.dll $DESTDIR/bin/
	cp /usr/lib/gcc/${XPREFIX}/*/libstdc++-6.dll $DESTDIR/bin/
fi
#Ubuntu's 14.04's mingw needs this one for the std libs above
if test -f /usr/${XPREFIX}/lib/libwinpthread-1.dll; then
	cp /usr/${XPREFIX}/lib/libwinpthread-1.dll $DESTDIR/bin/
fi

cp -r $PREFIX/share/${LOWERCASE_DIRNAME} $DESTDIR/share/
cp -r $PREFIX/share/locale $DESTDIR/share/
cp -r $PREFIX/etc/${LOWERCASE_DIRNAME}/* $DESTDIR/share/${LOWERCASE_DIRNAME}/

cp COPYING $DESTDIR/share/
cp gtk2_ardour/icons/${PRODUCT_ICON} $DESTDIR/share/
cp gtk2_ardour/icons/ArdourBug.ico $DESTDIR/share/

# replace default cursor with square version (sans hotspot file)
cp gtk2_ardour/icons/cursor_square/* $DESTDIR/share/${LOWERCASE_DIRNAME}/icons/

# clean build-dir after depoyment
echo " === bundle completed, cleaning up"
./waf uninstall
find $DESTDIR -name "*.dll.a" | xargs rm
echo " === complete"
du -sh $DESTDIR

################################################################################
### get video tools
if test -z "$NOVIDEOTOOLS"; then
	echo " === Including video-tools"
	HARVID_VERSION=$(curl -s -S http://ardour.org/files/video-tools/harvid_version.txt)
	XJADEO_VERSION=$(curl -s -S http://ardour.org/files/video-tools/xjadeo_version.txt)

	rsync -a -q --partial \
		rsync://ardour.org/video-tools/harvid_${WARCH}-${HARVID_VERSION}.tar.xz \
		"${SRCCACHE}/harvid_${WARCH}-${HARVID_VERSION}.tar.xz"

	rsync -a -q --partial \
		rsync://ardour.org/video-tools/xjadeo_${WARCH}-${XJADEO_VERSION}.tar.xz \
		"${SRCCACHE}/xjadeo_${WARCH}-${XJADEO_VERSION}.tar.xz"

	mkdir $DESTDIR/video
	tar -xf "${SRCCACHE}/harvid_${WARCH}-${HARVID_VERSION}.tar.xz" -C "$DESTDIR/video/"
	tar -xf "${SRCCACHE}/xjadeo_${WARCH}-${XJADEO_VERSION}.tar.xz" -C "$DESTDIR/video/"

	echo " === unzipped"
	du -sh $DESTDIR/video
	du -sh $DESTDIR
fi

################################################################################
### include static gdb - re-zipped binaries from
### http://sourceforge.net/projects/mingw/files/MinGW/Extension/gdb/gdb-7.6.1-1/gdb-7.6.1-1-mingw32-bin.tar.lzma
### http://sourceforge.net/projects/mingw-w64/files/Toolchains%20targetting%20Win64/Personal%20Builds/mingw-builds/4.9.1/threads-win32/sjlj/x86_64-4.9.1-release-win32-sjlj-rt_v3-rev1.7z
if ! grep " using ./waf configure" build/config.log | grep -q -- "--optimize"; then
	PACKAGE_GDB=1
fi
if test -n "$PACKAGE_GDB"; then
	download gdb-static-win3264.tar.xz http://robin.linuxaudio.org/gdb-static-win3264.tar.xz
	cd ${SRCCACHE}
	tar xf gdb-static-win3264.tar.xz
	cd - > /dev/null

	echo " === Creating debug.bat"
	cp -r ${SRCCACHE}/gdb_$WARCH $DESTDIR/gdb
	cat > $DESTDIR/debug.bat << EOF
cd bin
START ..\\gdb\\bin\\gdb.exe -iex "set logging overwrite on" -iex "set height 0" -iex "set logging on %UserProfile%\\${PRODUCT_NAME}-debug.log" -iex "target exec ${PRODUCT_EXE}" -iex "run"
EOF
	OUTFILE="${TMPDIR}/${PRODUCT_NAME}-${ARDOURVERSION}-dbg-${WARCH}-Setup.exe"
	VERSIONINFO="Debug Version."
else
	OUTFILE="${TMPDIR}/${PRODUCT_NAME}-${ARDOURVERSION}-${WARCH}-Setup.exe"
	VERSIONINFO="Optimized Version."
fi

################################################################################
### Mixbus plugins, etc
if true ; then
	mkdir -p $ALIBDIR/LV2

	echo "Adding General MIDI Synth LV2"

	for proj in x42-gmsynth; do
		X42_VERSION=$(curl -s -S http://x42-plugins.com/x42/win/${proj}.latest.txt)
		rsync -a -q --partial \
			rsync://x42-plugins.com/x42/win/${proj}-lv2-${WARCH}-${X42_VERSION}.zip \
			"${SRCCACHE}/${proj}-lv2-${WARCH}-${X42_VERSION}.zip"
		unzip -q -d "$ALIBDIR/LV2/" "${SRCCACHE}/${proj}-lv2-${WARCH}-${X42_VERSION}.zip"
	done
fi

if test x$WITH_COMMERCIAL_X42_LV2 != x ; then
	mkdir -p $ALIBDIR/LV2

	echo "Adding commercial x42 Plugins"

	for proj in x42-meters x42-eq x42-whirl; do
		X42_VERSION=$(curl -s -S http://x42-plugins.com/x42/win/${proj}.latest.txt)
		rsync -a -q --partial \
			rsync://x42-plugins.com/x42/win/${proj}-lv2-${WARCH}-${X42_VERSION}.zip \
			"${SRCCACHE}/${proj}-lv2-${WARCH}-${X42_VERSION}.zip"
		unzip -q -d "$ALIBDIR/LV2/" "${SRCCACHE}/${proj}-lv2-${WARCH}-${X42_VERSION}.zip"
	done
fi


if test x$WITH_GRATIS_X42_LV2 != x ; then
	mkdir -p $ALIBDIR/LV2

	echo "Adding gratis x42 Plugins"

	for proj in x42-autotune x42-midifilter x42-stereoroute setBfree x42-avldrums x42-limiter x42-tuner; do
		X42_VERSION=$(curl -s -S http://x42-plugins.com/x42/win/${proj}.latest.txt)
		rsync -a -q --partial \
			rsync://x42-plugins.com/x42/win/${proj}-lv2-${WARCH}-${X42_VERSION}.zip \
			"${SRCCACHE}/${proj}-lv2-${WARCH}-${X42_VERSION}.zip"
		unzip -q -d "$ALIBDIR/LV2/" "${SRCCACHE}/${proj}-lv2-${WARCH}-${X42_VERSION}.zip"
	done
fi

if test x$WITH_HARRISON_LV2 != x ; then
	mkdir -p $DESTDIR/LV2

	echo "Including Harrison LV2s"

	curl -s -S --fail -# \
		-z "${SRCCACHE}/${HARRISONLV2}.${WARCH}.zip" \
		-o "${SRCCACHE}/${HARRISONLV2}.${WARCH}.zip" \
		"${HARRISONDSPURL}/${HARRISONLV2}.${WARCH}.zip"
	unzip -q -d "$DESTDIR/LV2/" "${SRCCACHE}/${HARRISONLV2}.${WARCH}.zip"
fi

if test -n "$MIXBUS"; then
	echo "Deploying Harrison Mixbus Channelstrip"

	mkdir -p $ALIBDIR/ladspa/strip
	curl -s -S --fail -# \
		-z "${SRCCACHE}/${HARRISONCHANNELSTRIP}.${WARCH}.dll" \
		-o "${SRCCACHE}/${HARRISONCHANNELSTRIP}.${WARCH}.dll" \
		"${HARRISONDSPURL}/${HARRISONCHANNELSTRIP}.${WARCH}.dll"

	cp "${SRCCACHE}/${HARRISONCHANNELSTRIP}.${WARCH}.dll" \
		"$ALIBDIR/ladspa/strip/${HARRISONCHANNELSTRIP}.dll"

	echo "Deploying Harrison Vamp Plugins"
	mkdir -p $ALIBDIR/vamp
	curl -s -S --fail -# \
		-z "${SRCCACHE}/harrison_vamp.${WARCH}.dll" \
		-o "${SRCCACHE}/harrison_vamp.${WARCH}.dll" \
		"${HARRISONDSPURL}/harrison_vamp.${WARCH}.dll"

	cp "${SRCCACHE}/harrison_vamp.${WARCH}.dll" \
		"$ALIBDIR/vamp/harrison_vamp.dll"

	# Mixbus Bundled Media Content
	curl -s -S --fail -#  \
		-z "${SRCCACHE}/MixbusBundledMedia.zip" \
		-o "${SRCCACHE}/MixbusBundledMedia.zip" \
		"http://rsrc.harrisonconsoles.com/mixbus/mb8/content/MixbusBundledMedia.zip"

	if test -f "${SRCCACHE}/MixbusBundledMedia.zip"; then
		echo "Adding Mixbus Bundled Content"
		rm -f $DESTDIR/share/media/*.*
		unzip -q -d "$DESTDIR/share/media/" "${SRCCACHE}/MixbusBundledMedia.zip"
	fi
fi

################################################################################

if test x$DEMO_SESSION_URL != x ; then
	mkdir -p $DESTDIR/share/sessions
	DEMO_SESSIONS=$(curl -s -S --fail $DEMO_SESSION_URL/index.txt)
	for demo in $DEMO_SESSIONS; do
		curl -s -S --fail -# -o $DESTDIR/share/sessions/$demo $DEMO_SESSION_URL/$demo
	done
fi

################################################################################

( cd $DESTDIR ; find . ) > ${TMPDIR}/file_list.txt

################################################################################
echo " === Preparing Windows Installer"
NSISFILE=$DESTDIR/a3.nsis

if test "$WARCH" = "w64"; then
	PGF=PROGRAMFILES64
	SFX=
else
	PGF=PROGRAMFILES
	# TODO we should only add this for 32bit on 64bit windows!
	SFX=" (x86)"
fi

if test -n "$QUICKZIP" ; then
	cat > $NSISFILE << EOF
SetCompressor zlib
EOF
else
	cat > $NSISFILE << EOF
SetCompressor /SOLID lzma
SetCompressorDictSize 32
EOF
fi

cat >> $NSISFILE << EOF
!addincludedir "${this_script_dir}\\nsis"
!include MUI2.nsh
!include FileAssociation.nsh
!include WinVer.nsh

Name "${PROGRAM_NAME}${PROGRAM_VERSION}"
OutFile "${OUTFILE}"
RequestExecutionLevel admin
InstallDir "\$${PGF}\\${PRODUCT_ID}"
InstallDirRegKey HKLM "Software\\${PRODUCT_NAME}\\${PRODUCT_ID}\\$WARCH" "Install_Dir"
!define MUI_ICON "share\\${PRODUCT_ICON}"
!define MUI_UNICON "share\\${PRODUCT_ICON}"

EOF

if test -n "$MIXBUS"; then

# TODO: proper welcome/finish text.
	cat >> $NSISFILE << EOF
!define MUI_FINISHPAGE_TITLE "Welcome to Harrison Mixbus"
!define MUI_FINISHPAGE_TEXT "Thanks for your purchase of Mixbus!\$\\r\$\\nYou will find the Mixbus application in the Start Menu (or the All Apps panel for Windows 8) \$\\r\$\\nClick the link below to view the Mixbus manual, and learn ways to get involved with the Mixbus community."
!define MUI_FINISHPAGE_LINK "Mixbus Manual"
!define MUI_FINISHPAGE_LINK_LOCATION "http://www.harrisonconsoles.com/mixbus/${MANUAL_NAME}/"
!define MUI_FINISHPAGE_NOREBOOTSUPPORT
EOF

else

	cat >> $NSISFILE << EOF
!define MUI_FINISHPAGE_TITLE "Welcome to Ardour"
!define MUI_FINISHPAGE_TEXT "This Windows version of Ardour is provided as-is.\$\\r\$\\nThe Ardour community currently has no expertise in supporting Windows users, and there are no developers focusing on Windows-specific issues either.\$\\r\$\\nIf you like Ardour, please consider helping out."
!define MUI_FINISHPAGE_LINK "Ardour Manual"
!define MUI_FINISHPAGE_LINK_LOCATION "http://manual.ardour.org/"
#this would run as admin - see http://forums.winamp.com/showthread.php?t=353366
#!define MUI_FINISHPAGE_RUN "\$INSTDIR\\bin\\${PRODUCT_EXE}"
!define MUI_FINISHPAGE_NOREBOOTSUPPORT
EOF

fi

cat >> $NSISFILE << EOF
!define MUI_ABORTWARNING
!insertmacro MUI_PAGE_LICENSE "share\\COPYING"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Section "${PROGRAM_NAME}${PROGRAM_VERSION} (required)" SecMainProg
  SectionIn RO
  SetOutPath \$INSTDIR
  File /r bin
  File /r lib
  File /r share
  File /nonfatal debug.bat
  File /nonfatal /r gdb
  WriteRegStr HKLM "Software\\${PROGRAM_KEY}\\v${major_version}\\$WARCH" "Install_Dir" "\$INSTDIR"
  WriteRegStr HKLM "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\${PRODUCT_ID}-${WARCH}" "DisplayName" "${PROGRAM_NAME}${PROGRAM_VERSION}"
  WriteRegStr HKLM "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\${PRODUCT_ID}-${WARCH}" "UninstallString" '"\$INSTDIR\\uninstall.exe"'
  WriteRegDWORD HKLM "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\${PRODUCT_ID}-${WARCH}" "NoModify" 1
  WriteRegDWORD HKLM "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\${PRODUCT_ID}-${WARCH}" "NoRepair" 1
  WriteUninstaller "\$INSTDIR\uninstall.exe"
  CreateShortCut "\$INSTDIR\\${PROGRAM_NAME}${PROGRAM_VERSION}.lnk" "\$INSTDIR\\bin\\${PRODUCT_EXE}" "" "\$INSTDIR\\bin\\${PRODUCT_EXE}" 0
  \${registerExtension} "\$INSTDIR\\bin\\${STATEFILE_SUFFIX}" ".${PRODUCT_NAME}" "${PROGRAM_NAME} Session"
SectionEnd
EOF

if test -z "$NOVIDEOTOOLS"; then

	cat >> $NSISFILE << EOF
Section "Videotimeline Tools (required)" SecVideo
  WriteRegStr HKLM "Software\\${PROGRAM_KEY}\\v${major_version}\\video" "Install_Dir" "\$INSTDIR\\video"
  SectionIn RO
  SetOutPath \$INSTDIR
  File /r video
SectionEnd
EOF

fi

if test x$WITH_HARRISON_LV2 != x ; then
if test -n "$MIXBUS"; then
	cat >> $NSISFILE << EOF
Section "Harrison XT plugins (required)" SecXT
  SectionIn RO
  SetOutPath \$INSTDIR\\lib\\${LOWERCASE_DIRNAME}\\LV2
  File LV2\\.harrison_version.txt
  File /r LV2\\*.lv2
SectionEnd
EOF
else
	cat >> $NSISFILE << EOF
Section "Harrison XT plugins and ACE plugin GUIs" SecXT
  SetOutPath \$INSTDIR\\lib\\${LOWERCASE_DIRNAME}\\LV2
  File LV2\\.harrison_version.txt
  File /r LV2\\*.lv2
SectionEnd
EOF
fi
fi

cat >> $NSISFILE << EOF

Section "WASAPI sound driver" SecWASAPI
SectionEnd

Section "Start Menu Shortcuts" SecMenu
  SetShellVarContext all
  SetOutPath \$INSTDIR
  CreateDirectory "\$SMPROGRAMS\\${PRODUCT_ID}${SFX}"
  CreateShortCut "\$SMPROGRAMS\\${PRODUCT_ID}${SFX}\\${PROGRAM_NAME}${PROGRAM_VERSION}.lnk" "\$INSTDIR\\bin\\${PRODUCT_EXE}" "" "\$INSTDIR\\bin\\${PRODUCT_EXE}" 0
EOF

if test -f "$DESTDIR/debug.bat"; then
	cat >> $NSISFILE << EOF
  CreateShortCut "\$SMPROGRAMS\\${PRODUCT_ID}${SFX}\\${PROGRAM_NAME}${PROGRAM_VERSION} GDB.lnk" "\$INSTDIR\\debug.bat" "" "\$INSTDIR\\share\\ArdourBug.ico" 0
EOF
fi

if test -z "$NOVIDEOTOOLS"; then
	cat >> $NSISFILE << EOF
  IfFileExists "\$INSTDIR\\video\\xjadeo\\xjadeo.exe" 0 +2
  CreateShortCut "\$SMPROGRAMS\\${PRODUCT_ID}${SFX}\\Video Monitor.lnk" "\$INSTDIR\\video\\xjadeo\\xjadeo.exe" "" "\$INSTDIR\\video\\xjadeo\\xjadeo.exe" 0
EOF
fi

cat >> $NSISFILE << EOF
  CreateShortCut "\$SMPROGRAMS\\${PRODUCT_ID}${SFX}\\Uninstall.lnk" "\$INSTDIR\\uninstall.exe" "" "\$INSTDIR\\uninstall.exe" 0
SectionEnd
LangString DESC_SecMainProg \${LANG_ENGLISH} "${PROGRAM_NAME} ${ARDOURVERSION}\$\\r\$\\n${VERSIONINFO}\$\\r\$\\n${ARDOURDATE}"
LangString DESC_SecWASAPI \${LANG_ENGLISH} "WASAPI Audio Driver\$\\r\$\\nOnly works on Vista or later. Windows 10 Users may currently also experience issues if this is installed."
EOF

if test -z "$NOVIDEOTOOLS"; then
	cat >> $NSISFILE << EOF
LangString DESC_SecVideo \${LANG_ENGLISH} "Video Tools\$\\r\$\\nxjadeo-${XJADEO_VERSION}\$\\r\$\\nharvid-${HARVID_VERSION}"
EOF
fi
if test x$WITH_HARRISON_LV2 != x ; then
	cat >> $NSISFILE << EOF
LangString DESC_SecXT \${LANG_ENGLISH} "These are proprietary additions, but the DSP is not license encumbered. XT-plugin GUIs are commercial, the additional a-*/ACE plugin GUIs are free."
EOF
fi

cat >> $NSISFILE << EOF
LangString DESC_SecMenu \${LANG_ENGLISH} "Create Start-Menu Shortcuts (recommended)."
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
!insertmacro MUI_DESCRIPTION_TEXT \${SecMainProg} \$(DESC_SecMainProg)
!insertmacro MUI_DESCRIPTION_TEXT \${SecWASAPI} \$(DESC_SecWASAPI)
EOF

if test -z "$NOVIDEOTOOLS"; then
	cat >> $NSISFILE << EOF
!insertmacro MUI_DESCRIPTION_TEXT \${SecVideo} \$(DESC_SecVideo)
EOF
fi

if test x$WITH_HARRISON_LV2 != x ; then
	cat >> $NSISFILE << EOF
!insertmacro MUI_DESCRIPTION_TEXT \${SecXT} \$(DESC_SecXT)
EOF
fi

cat >> $NSISFILE << EOF
!insertmacro MUI_DESCRIPTION_TEXT \${SecMenu} \$(DESC_SecMenu)
!insertmacro MUI_FUNCTION_DESCRIPTION_END
Section "Uninstall"
  SetShellVarContext all
  DeleteRegKey HKLM "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\${PRODUCT_ID}-${WARCH}"
  DeleteRegKey HKLM "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\${PRODUCT_ID}"
  DeleteRegKey HKLM "Software\\${PROGRAM_KEY}\\v${major_version}"
  RMDir /r "\$INSTDIR\\bin"
  RMDir /r "\$INSTDIR\\lib"
  RMDir /r "\$INSTDIR\\share"
  RMDir /r "\$INSTDIR\\gdb"
  RMDir /r "\$INSTDIR\\video"
  Delete "\$INSTDIR\\debug.bat"
  Delete "\$INSTDIR\\uninstall.exe"
  Delete "\$INSTDIR\\${PROGRAM_NAME}${PROGRAM_VERSION}.lnk"
  RMDir "\$INSTDIR"
  Delete "\$SMPROGRAMS\\${PRODUCT_ID}${SFX}\\*.*"
  RMDir "\$SMPROGRAMS\\${PRODUCT_ID}${SFX}"
  \${unregisterExtension} ".${STATEFILE_SUFFIX}" "${PROGRAM_NAME} Session"
SectionEnd


Function .onInit

  ReadRegStr \$R0 HKLM \
    "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\${PRODUCT_ID}-${WARCH}" \
    "UninstallString"
  StrCmp \$R0 "" done

  IfSilent silentuninst

  MessageBox MB_OKCANCEL|MB_ICONEXCLAMATION \
    "${PROGRAM_NAME} is already installed. Click 'OK' to remove the previous version or 'Cancel' to cancel this upgrade." \
    IDOK uninst
    Abort

  silentuninst:
    ExecWait '\$R0 /S _?=\$INSTDIR'

    ReadRegStr \$R1 HKLM \
      "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\${PRODUCT_ID}-${WARCH}" \
      "UninstallString"
    StrCmp \$R1 "" 0 done

    Delete "\$INSTDIR\\uninstall.exe"
    RMDir "\$INSTDIR"
    goto done

  uninst:
    ClearErrors
    ExecWait '\$R0 _?=\$INSTDIR'
    IfErrors uninstall_error

    ReadRegStr \$R1 HKLM \
      "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\${PRODUCT_ID}-${WARCH}" \
      "UninstallString"
    StrCmp \$R1 "" 0 done

    Delete "\$INSTDIR\\uninstall.exe"
    RMDir "\$INSTDIR"
    goto done

  uninstall_error:

    MessageBox MB_OK|MB_ICONEXCLAMATION \
      "Uninstaller did not complete successfully. Continue at your own risk..." \
      IDOK done

  done:

  \${If} \${AtMostWinXP}
    SectionSetFlags \${SecWASAPI} \${SF_RO}
  \${Else}
    SectionSetFlags \${SecWASAPI} 0
  \${EndIf}

FunctionEnd

Function .onInstSuccess

  \${If} \${AtMostWinXP}
    goto pa_no_wasapi
  \${EndIf}

  SectionGetFlags \${SecWASAPI} \$R0

  IntOp \$R0 \$R0 & \${SF_SELECTED}
  IntCmp \$R0 \${SF_SELECTED} pa_with_wasapi pa_no_wasapi

  pa_with_wasapi:
; VISTA .. 9, libportaudio with WASAPI is good.
  Delete "\$INSTDIR\\bin\\libportaudio-2.xp"
  goto endportaudio

; Windows XP lacks support for WASAPI, Windows10 on some system has issues
; http://tracker.ardour.org/view.php?id=6507
  pa_no_wasapi:
  Delete "\$INSTDIR\\bin\\libportaudio-2.dll"
  Rename "\$INSTDIR\\bin\\libportaudio-2.xp" "\$INSTDIR\\bin\\libportaudio-2.dll"

  endportaudio:

FunctionEnd
EOF

rm -f ${OUTFILE}
echo " === OutFile: $OUTFILE"

if test -n "$QUICKZIP" ; then
echo " === Building Windows Installer (fast zip)"
else
echo " === Building Windows Installer (lzma compression takes ages)"
fi
time makensis -V2 $NSISFILE
rm -rf $DESTDIR
ls -lgGh "$OUTFILE"
