#!/bin/bash

# we assuem this script is <ardour-src>/tools/x-win/package
pushd "`/usr/bin/dirname \"$0\"`" > /dev/null; this_script_dir="`pwd`"; popd > /dev/null
cd $this_script_dir/../..
test -f gtk2_ardour/wscript || exit 1

: ${XARCH=i686} # or x86_64
: ${ROOT=/home/ardour}
: ${MAKEFLAGS=-j4}
: ${TMPDIR=/var/tmp}
: ${SRCDIR=/var/tmp/winsrc}  # source-code tgz cache

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
BINVERSION=$(git describe | sed 's/-g.*$//' | sed 's/-/./')
if ! test -f build/gtk2_ardour/ardour-${BINVERSION}.exe; then
	echo "*** Please compile ardour ${ARDOURVERSION} first."
	exit 1
fi
./waf install

################################################################################

if test -z "$DESTDIR"; then
	DESTDIR=`mktemp -d`
	trap 'rm -rf $DESTDIR' exit SIGINT SIGTERM
fi

echo " === bundle to $DESTDIR"

ALIBDIR=$DESTDIR/lib/ardour3

rm -rf $DESTDIR
mkdir -p $DESTDIR/bin
mkdir -p $DESTDIR/share/
mkdir -p $ALIBDIR/surfaces
mkdir -p $ALIBDIR/backends
mkdir -p $ALIBDIR/panners
mkdir -p $ALIBDIR/vamp

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
cp `ls -t build/gtk2_ardour/ardour-*.exe | head -n1` $DESTDIR/bin/ardour.exe

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

mv $ALIBDIR/surfaces/ardourcp-*.dll $DESTDIR/bin/

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

cp -r $PREFIX/share/ardour3 $DESTDIR/share/
cp -r $PREFIX/etc/ardour3/* $DESTDIR/share/ardour3/

cp COPYING $DESTDIR/share/
cp gtk2_ardour/icons/ardour.ico $DESTDIR/share/

# replace default cursor with square version (sans hotspot file)
cp gtk2_ardour/icons/cursor_square/*.png $DESTDIR/share/icons/

# clean build-dir after depoyment
./waf uninstall
echo " === complete"
du -sh $DESTDIR

################################################################################
### include static gdb - re-zipped binaries from
### http://sourceforge.net/projects/mingw/files/MinGW/Extension/gdb/gdb-7.6.1-1/gdb-7.6.1-1-mingw32-bin.tar.lzma
### http://sourceforge.net/projects/mingw-w64/files/Toolchains%20targetting%20Win64/Personal%20Builds/mingw-builds/4.9.1/threads-win32/sjlj/x86_64-4.9.1-release-win32-sjlj-rt_v3-rev1.7z
if ! grep " using ./waf configure" build/config.log | grep -q -- "--optimize"; then
	download gdb-static-win3264.tar.xz http://robin.linuxaudio.org/gdb-static-win3264.tar.xz
	cd ${SRCDIR}
	tar xf gdb-static-win3264.tar.xz
	cd - > /dev/null

	echo " === Creating ardbg.bat"
	cp -r ${SRCDIR}/gdb_$WARCH $DESTDIR/gdb
	cat > $DESTDIR/ardbg.bat << EOF
cd bin
START ..\\gdb\\bin\\gdb.exe ardour.exe
EOF
	OUTFILE="${TMPDIR}/ardour-${ARDOURVERSION}-dbg-${WARCH}-Setup.exe"
else
	OUTFILE="${TMPDIR}/ardour-${ARDOURVERSION}-${WARCH}-Setup.exe"
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
!include MUI2.nsh
Name "Ardour3"
OutFile "${OUTFILE}"
RequestExecutionLevel admin
InstallDir "\$${PGF}\\ardour3"
InstallDirRegKey HKLM "Software\\Ardour\\ardour3\\$WARCH" "Install_Dir"

!define MUI_ICON "share\\ardour.ico"
!define MUI_FINISHPAGE_TITLE "Welcome to Ardour"
!define MUI_FINISHPAGE_TEXT "This windows versions or Ardour is provided as-is.\$\\r\$\\nThe ardour community currently has no expertise in supporting windows users, and there are no developers focusing on windows specific issues either.\$\\r\$\\nIf you like Ardour, please consider helping out."
!define MUI_FINISHPAGE_LINK "Ardour Manual"
!define MUI_FINISHPAGE_LINK_LOCATION "http://manual.ardour.org"
#this would run as admin - see http://forums.winamp.com/showthread.php?t=353366
#!define MUI_FINISHPAGE_RUN "\$INSTDIR\\bin\\ardour.exe"
!define MUI_FINISHPAGE_NOREBOOTSUPPORT

!define MUI_ABORTWARNING
!insertmacro MUI_PAGE_LICENSE "share\\COPYING"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Section "Ardour3 (required)" SecArdour
  SectionIn RO
  SetOutPath \$INSTDIR
  File /r bin
  File /r lib
  File /r share
  File /nonfatal ardbg.bat
  File /nonfatal /r gdb
  WriteRegStr HKLM SOFTWARE\\Ardour\\ardour3\\$WARCH "Install_Dir" "\$INSTDIR"
  WriteRegStr HKLM "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\ardour3" "DisplayName" "Ardour3"
  WriteRegStr HKLM "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\ardour3" "UninstallString" '"\$INSTDIR\\uninstall.exe"'
  WriteRegDWORD HKLM "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\ardour3" "NoModify" 1
  WriteRegDWORD HKLM "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\ardour3" "NoRepair" 1
  WriteUninstaller "\$INSTDIR\uninstall.exe"
  CreateShortCut "\$INSTDIR\\Ardour3.lnk" "\$INSTDIR\\bin\\ardour.exe" "" "\$INSTDIR\\bin\\ardour.exe" 0
SectionEnd
Section "Start Menu Shortcuts" SecMenu
  SetShellVarContext all
  CreateDirectory "\$SMPROGRAMS\\ardour3"
  CreateShortCut "\$SMPROGRAMS\\ardour3\\Ardour3.lnk" "\$INSTDIR\\bin\\ardour.exe" "" "\$INSTDIR\\bin\\ardour.exe" 0
EOF

if test -f "$DESTDIR/ardbg.bat"; then
	cat >> $NSISFILE << EOF
  CreateShortCut "\$SMPROGRAMS\\ardour3\\Ardour3 GDB.lnk" "\$INSTDIR\\ardbg.bat" "" "\$INSTDIR\\ardbg.bat" 0
EOF
fi

cat >> $NSISFILE << EOF
  CreateShortCut "\$SMPROGRAMS\\ardour3\\Uninstall.lnk" "\$INSTDIR\\uninstall.exe" "" "\$INSTDIR\\uninstall.exe" 0
SectionEnd
LangString DESC_SecArdour \${LANG_ENGLISH} "Ardour ${ARDOURVERSION}\$\\r\$\\nDebug Version.\$\\r\$\\n${ARDOURDATE}"
LangString DESC_SecMenu \${LANG_ENGLISH} "Create Start-Menu Shortcuts (recommended)."
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
!insertmacro MUI_DESCRIPTION_TEXT \${SecArdour} \$(DESC_SecArdour)
!insertmacro MUI_DESCRIPTION_TEXT \${SecMenu} \$(DESC_SecMenu)
!insertmacro MUI_FUNCTION_DESCRIPTION_END
Section "Uninstall"
  SetShellVarContext all
  DeleteRegKey HKLM "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\ardour"
  DeleteRegKey HKLM SOFTWARE\\Ardour\\ardour3
  RMDir /r "\$INSTDIR\\bin"
  RMDir /r "\$INSTDIR\\lib"
  RMDir /r "\$INSTDIR\\share"
  RMDir /r "\$INSTDIR\\gdb"
  Delete "\$INSTDIR\\ardbg.bat"
  Delete "\$INSTDIR\\uninstall.exe"
  Delete "\$INSTDIR\\Ardour3.lnk"
  RMDir "\$INSTDIR"
  Delete "\$SMPROGRAMS\\ardour3\\*.*"
  RMDir "\$SMPROGRAMS\\ardour3"
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
ls -lh "$OUTFILE"
