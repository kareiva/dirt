;
; Dirt - Encrypting IRC proxy for all IRC clients
; Copyright (C) 2005-2013  Mathias Karlsson
;
; This program is free software; you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation; either version 2 of the License, or
; (at your option) any later version.
;
; See License.txt for more details.
;
; Mathias Karlsson <tmowhrekf at gmail dot com>
;

!include "WordFunc.nsh"
!insertmacro VersionConvert
!insertmacro VersionCompare

Name "Dirt"
OutFile upgrade.exe
SetCompressor /SOLID lzma
InstallDir $PROGRAMFILES\Dirt
;Icon res\upgrade.ico
;UninstallIcon res\upgrade.ico

Var val
Var handle
Var latest
Var url
Var version

Function .onInit
	ReadRegStr $0 SHCTX "Software\Dirt" "InstallDir"
	IfErrors finish
	StrCpy $INSTDIR $0
finish:
FunctionEnd

Section "Check for upgrades"
	DetailPrint "Scanning for upgrades..."
	NSISdl::download_quiet http://dirtirc.sourceforge.net/upgrade/latest.php $INSTDIR\latest.txt
	Pop $val
	StrCmp $val "success" 0 check_failed

	FileOpen $handle $INSTDIR\latest.txt r
	FileRead $handle $1
	FileRead $handle $2
	FileClose $handle
	StrCpy $latest $1 -1
	StrCpy $url $2 -1
	StrCmp $latest "" check_failed
	StrCmp $url "" check_failed
	IfErrors check_failed

	StrCpy $version "1.0.0 alpha 28"
	${VersionConvert} $latest "" $1
	${VersionConvert} $version "" $2
	${VersionCompare} $1 $2 $0
	StrCmp $0 "1" download_latest this_is_latest
	Goto finish ; shouldn't ever happen

check_failed:
	MessageBox MB_OK|MB_ICONSTOP "The upgrade files are offline." /SD IDOK
	Goto finish

download_failed:
	MessageBox MB_OK|MB_ICONSTOP "Failed to download the latest version." /SD IDOK
	Goto finish

this_is_latest:
	MessageBox MB_OK "Your version:$\t$version$\nLatest version:$\t$latest$\n$\nYou have the latest version." /SD IDOK
	Goto finish

download_latest:
	IfSilent 0 start_download
	System::Call 'kernel32::GetModuleFileNameA(i 0, t .R0, i 1024) i r1'
	ExecWait '"$R0"'
	Goto finish

start_download:
	MessageBox MB_YESNO "Your version:$\t$version$\nLatest version:$\t$latest$\n$\nThere is a new version available.$\nDo you want to download and install it?" IDNO finish
	NSISdl::download $url $INSTDIR\install.exe
	Pop $val
	StrCmp $val "success" 0 download_failed
	Exec '"$INSTDIR\install.exe" /S'
	Goto finish

finish:
	Delete $INSTDIR\latest.txt
	Quit
SectionEnd
