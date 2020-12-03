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

Name Dirt
OutFile install.exe
LicenseData License.txt
SetCompressor /SOLID lzma
InstallDir $PROGRAMFILES\Dirt
;Icon res\install.ico
;UninstallIcon res\uninstall.ico

Page license
Page components
Page directory
Page instfiles
UninstPage uninstConfirm
UninstPage components
UninstPage instfiles

Section "-Dirt" section_dirt
	SetOutPath $INSTDIR
	ExecWait '"$INSTDIR\dirt.exe" --quit'
	ExecWait '"$INSTDIR\dirtirc.exe" --quit'
	Delete $INSTDIR\dirt.exe
	Delete $INSTDIR\dirt-upgrade.exe
	Delete $INSTDIR\dirt-install.exe
	Delete $INSTDIR\upgrade.exe
	Delete $INSTDIR\install.exe
	File dirtirc.exe
	File Manual.txt
	File ChangeLog.txt
	File License.txt
	File upgrade.exe
	WriteUninstaller $INSTDIR\uninstall.exe
	WriteRegStr SHCTX "Software\Dirt" "InstallDir" $INSTDIR
	ExecWait '"$INSTDIR\dirtirc.exe" --upgrade'
SectionEnd

Section "Start now" section_start
	ExecWait '"$INSTDIR\dirt.exe" --quit'
	ExecWait '"$INSTDIR\dirtirc.exe" --quit'
	Exec '"$INSTDIR\dirtirc.exe"'
SectionEnd

Section "Start automatically on login" section_autostart
	CreateShortCut $SMSTARTUP\Dirt.lnk $INSTDIR\dirtirc.exe
SectionEnd

Section "Create a start menu folder" section_startmenu
	Delete $SMPROGRAMS\Dirt.lnk
	CreateDirectory $SMPROGRAMS\Dirt
	CreateShortCut $SMPROGRAMS\Dirt\Dirt.lnk $INSTDIR\dirtirc.exe
	CreateShortCut "$SMPROGRAMS\Dirt\Dirt ChangeLog.lnk" $INSTDIR\ChangeLog.txt
	CreateShortCut "$SMPROGRAMS\Dirt\Dirt Manual.lnk" $INSTDIR\Manual.txt
	CreateShortCut "$SMPROGRAMS\Dirt\Dirt Upgrade.lnk" $INSTDIR\upgrade.exe
	SetOutPath $PROGRAMFILES
	CreateShortCut "$SMPROGRAMS\Dirt\Uninstall Dirt.lnk" $INSTDIR\uninstall.exe
SectionEnd

Section /o "Create a desktop shortcut" section_desktop
	CreateShortCut $DESKTOP\Dirt.lnk $INSTDIR\dirtirc.exe
SectionEnd

Section "un.Dirt"
	ExecWait '"$INSTDIR\dirt.exe" --quit'
	ExecWait '"$INSTDIR\dirtirc.exe" --quit'
	Delete $INSTDIR\dirt.exe
	Delete $INSTDIR\dirtirc.exe
	Delete $INSTDIR\Manual.txt
	Delete $INSTDIR\ChangeLog.txt
	Delete $INSTDIR\License.txt
	Delete $INSTDIR\dirt-upgrade.exe
	Delete $INSTDIR\dirt-install.exe
	Delete $INSTDIR\upgrade.exe
	Delete $INSTDIR\install.exe
	Delete $INSTDIR\uninstall.exe
	Delete $SMSTARTUP\Dirt.lnk
	Delete $SMPROGRAMS\Dirt.lnk
	Delete $DESKTOP\Dirt.lnk
	Delete $SMPROGRAMS\Dirt\*.lnk
	RMDir $SMPROGRAMS\Dirt
	RMDir $INSTDIR
	Delete $APPDATA\Dirt\dirt-default.ini
SectionEnd

Section "un.Registry keys"
	DeleteRegKey SHCTX "Software\Dirt"
SectionEnd

Section /o "un.Configuration and key files"
	Delete $INSTDIR\dirt.ini
	Delete $INSTDIR\dirt.key
	RMDir $INSTDIR
	Delete $APPDATA\Dirt\dirt.ini
	Delete $APPDATA\Dirt\dirt.key
	RMDir $APPDATA\Dirt
SectionEnd

Function .onInit
	IfSilent 0 registry
	SectionSetFlags "${section_autostart}" 0
	SectionSetFlags "${section_startmenu}" 0
	SectionSetFlags "${section_desktop}" 0
registry:
	ReadRegStr $0 SHCTX "Software\Dirt" "InstallDir"
	IfErrors finish
	StrCpy $INSTDIR $0
finish:
FunctionEnd

Function un.onInit
	ReadRegStr $0 SHCTX "Software\Dirt" "InstallDir"
	IfErrors finish
	StrCpy $INSTDIR $0
finish:
FunctionEnd
