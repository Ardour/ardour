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
: ${SRCDIR=/var/tmp/winsrc}  # source-code tgz cache

# TODO: grep from build/config.log instead
while [ $# -gt 0 ] ; do
	echo "arg = $1"
	case $1 in
		--mixbus)
			MIXBUS=1
			shift ;;
	esac
done

# see also wscript, video_tool_paths.cc, bundle_env_mingw.cc
# registry keys based on this are used there
PROGRAM_NAME=Ardour
PRODUCT_NAME=ardour
PROGRAM_VERSION=${major_version}

LOWERCASE_DIRNAME=ardour${major_version}
STATEFILE_SUFFIX=ardour # see filename_extensions.cc

if test -n "$MIXBUS"; then
	PROGRAM_NAME=Mixbus
	PRODUCT_NAME=mixbus
fi

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
export SRCDIR

if [ "$(id -u)" = "0" ]; then
	apt-get -y install nsis curl
fi


function download {
echo "--- Downloading.. $2"
test -f ${SRCDIR}/$1 || curl -k -L -o ${SRCDIR}/$1 $2
}

################################################################################
set -e

ARDOURVERSION=$(git describe | sed 's/-g.*$//')
ARDOURDATE=$(date -R)
BINVERSION=$(git describe | sed 's/-g.*$//;s/\-rc\([^-]*\)-/-rc\1./;s/-/./;s/-.*$//')
if ! test -f build/gtk2_ardour/ardour-${BINVERSION}.exe; then
	echo "*** Please compile ardour ${ARDOURVERSION} first."
	exit 1
fi

echo " === bundle to $DESTDIR"

./waf install

################################################################################

if test -z "$DESTDIR"; then
	DESTDIR=`mktemp -d`
	trap 'rm -rf $DESTDIR' exit SIGINT SIGTERM
fi

echo " === bundle to $DESTDIR"

ALIBDIR=$DESTDIR/lib/${LOWERCASE_DIRNAME}

rm -rf $DESTDIR
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
cp build/libs/timecode/timecode.dll $DESTDIR/bin/
cp build/libs/qm-dsp/qmdsp-*.dll $DESTDIR/bin/
cp build/libs/canvas/canvas-*.dll $DESTDIR/bin/
cp build/libs/pbd/pbd-*.dll $DESTDIR/bin/
cp build/libs/audiographer/audiographer-*.dll $DESTDIR/bin/
cp build/libs/fst/ardour-vst-scanner.exe $DESTDIR/bin/ || true
cp `ls -t build/gtk2_ardour/ardour-*.exe | head -n1` $DESTDIR/bin/${PRODUCT_EXE}

mkdir -p $DESTDIR/lib/gtk-2.0/engines
cp build/libs/clearlooks-newer/clearlooks.dll $DESTDIR/lib/gtk-2.0/engines/libclearlooks.la

cp $PREFIX/bin/*dll $DESTDIR/bin/
cp $PREFIX/lib/*dll $DESTDIR/bin/
rm -rf $DESTDIR/bin/libjack*.dll

cp `find build/libs/surfaces/ -iname "*.dll"` $ALIBDIR/surfaces/
cp `find build/libs/backends/ -iname "*.dll"` $ALIBDIR/backends/
cp `find build/libs/panners/ -iname "*.dll"` $ALIBDIR/panners/

cp -r build/libs/LV2 $ALIBDIR/
cp -r build/libs/vamp-plugins/*ardourvampplugins*.dll $ALIBDIR/vamp/libardourvampplugins.dll
cp $PREFIX/lib/suil-*/*.dll $ALIBDIR/suil/ || true

# lv2 core, classifications etc - TODO check if we need the complete LV2 ontology
if test -d $PREFIX/lib/lv2/lv2core.lv2 ; then
	cp -R $PREFIX/lib/lv2/lv2core.lv2 $ALIBDIR/LV2/
fi

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
cp gtk2_ardour/icons/ardour_bug.ico $DESTDIR/share/

# replace default cursor with square version (sans hotspot file)
cp gtk2_ardour/icons/cursor_square/* $DESTDIR/share/${LOWERCASE_DIRNAME}/icons/

# clean build-dir after depoyment
echo " === bundle completed, cleaning up"
./waf uninstall
echo " === complete"
du -sh $DESTDIR

( cd $DESTDIR ; find . ) > ${TMPDIR}/file_list.txt

################################################################################
### get video tools
if test -z "$NOVIDEOTOOLS"; then
	echo " === Including video-tools"
	HARVID_VERSION=$(curl -s -S http://ardour.org/files/video-tools/harvid_version.txt)
	XJADEO_VERSION=$(curl -s -S http://ardour.org/files/video-tools/xjadeo_version.txt)

	rsync -a -q --partial \
		rsync://ardour.org/video-tools/harvid_win-${HARVID_VERSION}.tar.xz \
		"${SRCDIR}/harvid_win-${HARVID_VERSION}.tar.xz"

	rsync -a -q --partial \
		rsync://ardour.org/video-tools/xjadeo_win-${XJADEO_VERSION}.tar.xz \
		"${SRCDIR}/xjadeo_win-${XJADEO_VERSION}.tar.xz"

	mkdir $DESTDIR/video
	tar -xf "${SRCDIR}/harvid_win-${HARVID_VERSION}.tar.xz" -C "$DESTDIR/video/"
	tar -xf "${SRCDIR}/xjadeo_win-${XJADEO_VERSION}.tar.xz" -C "$DESTDIR/video/"

	echo " === unzipped"
	du -sh $DESTDIR/video
	du -sh $DESTDIR
fi

################################################################################
### include static gdb - re-zipped binaries from
### http://sourceforge.net/projects/mingw/files/MinGW/Extension/gdb/gdb-7.6.1-1/gdb-7.6.1-1-mingw32-bin.tar.lzma
### http://sourceforge.net/projects/mingw-w64/files/Toolchains%20targetting%20Win64/Personal%20Builds/mingw-builds/4.9.1/threads-win32/sjlj/x86_64-4.9.1-release-win32-sjlj-rt_v3-rev1.7z
if ! grep " using ./waf configure" build/config.log | grep -q -- "--optimize"; then
	download gdb-static-win3264.tar.xz http://robin.linuxaudio.org/gdb-static-win3264.tar.xz
	cd ${SRCDIR}
	tar xf gdb-static-win3264.tar.xz
	cd - > /dev/null

	echo " === Creating debug.bat"
	cp -r ${SRCDIR}/gdb_$WARCH $DESTDIR/gdb
	cat > $DESTDIR/debug.bat << EOF
cd bin
START ..\\gdb\\bin\\gdb.exe -iex "set logging overwrite on" -iex "set height 0" -iex "set logging on %UserProfile%\\${PRODUCT_NAME}-debug.log" ${PRODUCT_EXE}
EOF
	OUTFILE="${TMPDIR}/${PRODUCT_NAME}-${ARDOURVERSION}-dbg-${WARCH}-Setup.exe"
	VERSIONINFO="Debug Version."
else
	OUTFILE="${TMPDIR}/${PRODUCT_NAME}-${ARDOURVERSION}-${WARCH}-Setup.exe"
	VERSIONINFO="Optimized Version."
fi

################################################################################
### Mixbus plugins, etc
if test -n "$MIXBUS"; then

	mkdir -p $ALIBDIR/LV2
	METERS_VERSION=$(curl -s -S http://gareus.org/x42/win/x42-meters.latest.txt)
	rsync -a -q --partial \
		rsync://gareus.org/x42/win/x42-meters-lv2-${WARCH}-${METERS_VERSION}.zip \
		"${SRCDIR}/x42-meters-lv2-${WARCH}-${METERS_VERSION}.zip"
	unzip -d "$ALIBDIR/LV2/" "${SRCDIR}/x42-meters-lv2-${WARCH}-${METERS_VERSION}.zip"

	SETBFREE_VERSION=$(curl -s -S http://gareus.org/x42/win/setBfree.latest.txt)
	rsync -a -q --partial \
		rsync://gareus.org/x42/win/setBfree-lv2-${WARCH}-${SETBFREE_VERSION}.zip \
		"${SRCDIR}/setBfree-lv2-${WARCH}-${SETBFREE_VERSION}.zip"
	unzip -d "$ALIBDIR/LV2/" "${SRCDIR}/setBfree-lv2-${WARCH}-${SETBFREE_VERSION}.zip"

	MIDIFILTER_VERSION=$(curl -s -S http://gareus.org/x42/win/x42-midifilter.latest.txt)
	rsync -a -q --partial \
		rsync://gareus.org/x42/win/x42-midifilter-lv2-${WARCH}-${MIDIFILTER_VERSION}.zip \
		"${SRCDIR}/x42-midifilter-lv2-${WARCH}-${MIDIFILTER_VERSION}.zip"
	unzip -d "$ALIBDIR/LV2/" "${SRCDIR}/x42-midifilter-lv2-${WARCH}-${MIDIFILTER_VERSION}.zip"

fi

################################################################################
echo " === Preparing Windows Installer"
NSISFILE=$DESTDIR/a3.nsis

if test "$WARCH" = "w64"; then
	PGF=PROGRAMFILES64
else
	PGF=PROGRAMFILES
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

Name "${PROGRAM_NAME}${PROGRAM_VERSION}"
OutFile "${OUTFILE}"
RequestExecutionLevel admin
InstallDir "\$${PGF}\\${PRODUCT_ID}"
InstallDirRegKey HKLM "Software\\${PROGRAM_NAME}\\${PRODUCT_ID}\\$WARCH" "Install_Dir"
!define MUI_ICON "share\\${PRODUCT_ICON}"

EOF

if test -n "$MIXBUS"; then

# TODO: proper welcome/finish text.
	cat >> $NSISFILE << EOF
!define MUI_FINISHPAGE_TITLE "Welcome to Mixbus"
!define MUI_FINISHPAGE_TEXT "Thank you for choosing Harrison Mixbus."
!define MUI_FINISHPAGE_LINK "Harrison Consoles Website"
!define MUI_FINISHPAGE_LINK_LOCATION "http://harrisonconsoles.com"
!define MUI_FINISHPAGE_NOREBOOTSUPPORT
EOF

else

	cat >> $NSISFILE << EOF
!define MUI_FINISHPAGE_TITLE "Welcome to Ardour"
!define MUI_FINISHPAGE_TEXT "This windows versions or Ardour is provided as-is.\$\\r\$\\nThe ardour community currently has no expertise in supporting windows users, and there are no developers focusing on windows specific issues either.\$\\r\$\\nIf you like Ardour, please consider helping out."
!define MUI_FINISHPAGE_LINK "Ardour Manual"
!define MUI_FINISHPAGE_LINK_LOCATION "http://manual.ardour.org"
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
  WriteRegStr HKLM "Software\\${PROGRAM_NAME}\\v${PROGRAM_VERSION}\\$WARCH" "Install_Dir" "\$INSTDIR"
  WriteRegStr HKLM "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\${PRODUCT_ID}" "DisplayName" "${PROGRAM_NAME}${PROGRAM_VERSION}"
  WriteRegStr HKLM "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\${PRODUCT_ID}" "UninstallString" '"\$INSTDIR\\uninstall.exe"'
  WriteRegDWORD HKLM "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\${PRODUCT_ID}" "NoModify" 1
  WriteRegDWORD HKLM "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\${PRODUCT_ID}" "NoRepair" 1
  WriteUninstaller "\$INSTDIR\uninstall.exe"
  CreateShortCut "\$INSTDIR\\${PROGRAM_NAME}${PROGRAM_VERSION}.lnk" "\$INSTDIR\\bin\\${PRODUCT_EXE}" "" "\$INSTDIR\\bin\\${PRODUCT_EXE}" 0
  \${registerExtension} "\$INSTDIR\\bin\\${STATEFILE_SUFFIX}" ".${PRODUCT_NAME}" "${PROGRAM_NAME} Session"
SectionEnd
EOF

if test -z "$NOVIDEOTOOLS"; then

	cat >> $NSISFILE << EOF
Section "Videotimeline Tools" SecVideo
  WriteRegStr HKLM "Software\\${PROGRAM_NAME}\\v${PROGRAM_VERSION}\\video" "Install_Dir" "\$INSTDIR\\video"
  SetOutPath \$INSTDIR
  File /r video
SectionEnd
EOF

fi

cat >> $NSISFILE << EOF
Section "Start Menu Shortcuts" SecMenu
  SetShellVarContext all
  CreateDirectory "\$SMPROGRAMS\\${PRODUCT_ID}"
  CreateShortCut "\$SMPROGRAMS\\${PRODUCT_ID}\\${PROGRAM_NAME}${PROGRAM_VERSION}.lnk" "\$INSTDIR\\bin\\${PRODUCT_EXE}" "" "\$INSTDIR\\bin\\${PRODUCT_EXE}" 0
EOF

if test -f "$DESTDIR/debug.bat"; then
	cat >> $NSISFILE << EOF
  CreateShortCut "\$SMPROGRAMS\\${PRODUCT_ID}\\${PROGRAM_NAME}${PROGRAM_VERSION} GDB.lnk" "\$INSTDIR\\debug.bat" "" "\$INSTDIR\\share\\ardour_bug.ico" 0
EOF
fi

if test -z "$NOVIDEOTOOLS"; then
	cat >> $NSISFILE << EOF
  IfFileExists "\$INSTDIR\\video\\xjadeo\\xjadeo.exe" 0 +2
  CreateShortCut "\$SMPROGRAMS\\${PRODUCT_ID}\\Video Monitor.lnk" "\$INSTDIR\\video\\xjadeo\\xjadeo.exe" "" "\$INSTDIR\\video\\xjadeo\\xjadeo.exe" 0
EOF
fi

cat >> $NSISFILE << EOF
  CreateShortCut "\$SMPROGRAMS\\${PRODUCT_ID}\\Uninstall.lnk" "\$INSTDIR\\uninstall.exe" "" "\$INSTDIR\\uninstall.exe" 0
SectionEnd
LangString DESC_SecMainProg \${LANG_ENGLISH} "${PROGRAM_NAME} ${ARDOURVERSION}\$\\r\$\\n${VERSIONINFO}\$\\r\$\\n${ARDOURDATE}"
EOF

if test -z "$NOVIDEOTOOLS"; then
	cat >> $NSISFILE << EOF
LangString DESC_SecVideo \${LANG_ENGLISH} "Video Tools\$\\r\$\\nxjadeo-${XJADEO_VERSION}\$\\r\$\\nharvid-${HARVID_VERSION}"
EOF
fi

cat >> $NSISFILE << EOF
LangString DESC_SecMenu \${LANG_ENGLISH} "Create Start-Menu Shortcuts (recommended)."
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
!insertmacro MUI_DESCRIPTION_TEXT \${SecMainProg} \$(DESC_SecMainProg)
EOF

if test -z "$NOVIDEOTOOLS"; then
	cat >> $NSISFILE << EOF
!insertmacro MUI_DESCRIPTION_TEXT \${SecVideo} \$(DESC_SecVideo)
EOF
fi

cat >> $NSISFILE << EOF
!insertmacro MUI_DESCRIPTION_TEXT \${SecMenu} \$(DESC_SecMenu)
!insertmacro MUI_FUNCTION_DESCRIPTION_END
Section "Uninstall"
  SetShellVarContext all
  DeleteRegKey HKLM "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\${PRODUCT_ID}"
  DeleteRegKey HKLM "Software\\${PROGRAM_NAME}\\v${PROGRAM_VERSION}"
  RMDir /r "\$INSTDIR\\bin"
  RMDir /r "\$INSTDIR\\lib"
  RMDir /r "\$INSTDIR\\share"
  RMDir /r "\$INSTDIR\\gdb"
  RMDir /r "\$INSTDIR\\video"
  Delete "\$INSTDIR\\debug.bat"
  Delete "\$INSTDIR\\uninstall.exe"
  Delete "\$INSTDIR\\${PROGRAM_NAME}${PROGRAM_VERSION}.lnk"
  RMDir "\$INSTDIR"
  Delete "\$SMPROGRAMS\\${PRODUCT_ID}\\*.*"
  RMDir "\$SMPROGRAMS\\${PRODUCT_ID}"
  \${unregisterExtension} ".${STATEFILE_SUFFIX}" "${PROGRAM_NAME} Session"
SectionEnd
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
