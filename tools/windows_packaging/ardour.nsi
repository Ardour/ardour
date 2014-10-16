#!Nsis Installer Command Script
#
# This is an NSIS Installer Command Script generated automatically
# by the Fedora nsiswrapper program.  For more information see:
#
#   http://fedoraproject.org/wiki/MinGW
#
# To build an installer from the script you would normally do:
#
#   makensis this_script
#
# which will generate the output file 'installer.exe' which is a Windows
# installer containing your program.

SetCompressor /SOLID lzma
SetCompressorDictSize 32

!include MUI.nsh

!define MUI_ABORTWARNING
!define MUI_ICON ..\icons\icon\ardour.ico
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"
;!define MUI_HEADERIMAGE
;!define MUI_HEADERIMAGE_BITMAP header.bmp
;!define MUI_WELCOMEFINISHPAGE_BITMAP welcomefinish.bmp
;!define MUI_COMPONENTSPAGE_SMALLDESC

; Installer pages
!insertmacro MUI_PAGE_WELCOME

LicenseForceSelection off

!define MUI_LICENSEPAGE_BUTTON "$(^NextBtn)"
!define MUI_LICENSEPAGE_TEXT_BOTTOM "$(LICENSE_BOTTOM_TEXT)"
!insertmacro MUI_PAGE_LICENSE ..\COPYING

!insertmacro MUI_PAGE_DIRECTORY

!insertmacro MUI_PAGE_INSTFILES

!define MUI_FINISHPAGE_RUN "$INSTDIR\Ardour-3.0.exe"
!insertmacro MUI_PAGE_FINISH

; Uninstaller pages
!insertmacro MUI_UNPAGE_CONFIRM

!insertmacro MUI_UNPAGE_INSTFILES
ShowUninstDetails hide
!insertmacro MUI_UNPAGE_FINISH

!insertmacro MUI_LANGUAGE "English"

; Product definitions
;!define DUMMYINSTALL ; Define this to make it build quickly, not including any of the files or code in the sections, for quick testing of features of the installer and development thereof.
!define INSTDIR_KEY "SOFTWARE\Ardour-3.0"

; Product Information
Name "Ardour-3.0"
OutFile "Ardour-3.0-Setup.exe"
InstallDir "$PROGRAMFILES\Ardour"
InstallDirRegKey HKLM "${INSTDIR_KEY}" "Install_Dir"


ComponentText "Select which optional components you want to install."

DirText "Please select the installation folder."

Section "Ardour-3.0"
!ifndef DUMMYINSTALL
  SectionIn RO

  SetOutPath $INSTDIR
  File ardour-3.0.exe
  File *.dll
  File jack*.exe
  File ardour.ico
  File /r etc
  File /r jack
  File /r lib
  File /r share

!endif
SectionEnd

Section "Start Menu Shortcuts"
!ifndef DUMMY_INSTALL
  CreateDirectory "$SMPROGRAMS\Ardour-3.0"
  CreateShortCut "$SMPROGRAMS\Ardour-3.0\Uninstall Ardour-3.0.lnk" "$INSTDIR\Uninstall Ardour-3.0.exe" "" "$INSTDIR\Uninstall Ardour-3.0.exe" 0
  CreateShortCut "$SMPROGRAMS\Ardour-3.0\ardour-3.0.exe.lnk" "$INSTDIR\.\ardour-3.0.exe" "" "$INSTDIR\ardour.ico" 0
!endif
SectionEnd

Section "Desktop Icons"
!ifndef DUMMY_INSTALL
  CreateShortCut "$DESKTOP\Ardour-3.0.exe.lnk" "$INSTDIR\ardour-3.0.exe" "" "$INSTDIR\ardour.ico"
!endif
SectionEnd

Section "Uninstall"
!ifndef DUMMY_INSTALL
  Delete /rebootok "$DESKTOP\ardour-3.0.exe.lnk"
  Delete /rebootok "$SMPROGRAMS\Ardour-3.0\ardour-3.0.exe.lnk"
  Delete /rebootok "$SMPROGRAMS\Ardour-3.0\Uninstall Ardour-3.0.lnk"
  RMDir "$SMPROGRAMS\Ardour-3.0"

  ;RMDir "$INSTDIR\."
  Delete /rebootok "$INSTDIR\ardour-3.0.exe"
  Delete /rebootok "$INSTDIR\jack*.exe"
  Delete /rebootok "$INSTDIR\*.dll"
  Delete /rebootok "$INSTDIR\ardour.ico"
  RMDir /r "$INSTDIR\etc"
  RMDir /r "$INSTDIR\jack"
  RMDir /r "$INSTDIR\lib"
  RMDir /r "$INSTDIR\share"
  RMDir "$INSTDIR"
!endif
SectionEnd

Section -post
!ifndef DUMMY_INSTALL
  WriteUninstaller "$INSTDIR\Uninstall Ardour-3.0.exe"
!endif
SectionEnd
