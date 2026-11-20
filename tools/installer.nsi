!include "MUI2.nsh"

!ifndef ROOT
  !error "ROOT must be defined"
!endif

Unicode true
Name "Video converter for Shadertoy"
OutFile "${ROOT}\dist\VideoConverterForShadertoy-1.0.0-Setup.exe"
InstallDir "$PROGRAMFILES64\Video converter for Shadertoy"
InstallDirRegKey HKLM "Software\VideoConverterForShadertoy" "InstallDir"
RequestExecutionLevel admin
Icon "${ROOT}\assets\icons\app-icon.ico"
UninstallIcon "${ROOT}\assets\icons\app-icon.ico"

!define MUI_ABORTWARNING
!define MUI_ICON "${ROOT}\assets\icons\app-icon.ico"
!define MUI_UNICON "${ROOT}\assets\icons\app-icon.ico"
!define MUI_WELCOMEPAGE_TITLE "Video converter for Shadertoy"
!define MUI_FINISHPAGE_NOAUTOCLOSE

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${ROOT}\third_party\ffmpeg\licenses\COPYING.LGPLv2.1"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

Section "Application"
  SetOutPath "$INSTDIR"
  File /r "${ROOT}\bin\x64\Release\*.*"

  WriteUninstaller "$INSTDIR\Uninstall.exe"

  CreateDirectory "$SMPROGRAMS\Video converter for Shadertoy"
  CreateShortcut "$SMPROGRAMS\Video converter for Shadertoy\Video converter for Shadertoy.lnk" "$INSTDIR\VideoConverterForShadertoy.exe" "" "$INSTDIR\VideoConverterForShadertoy.exe" 0
  CreateShortcut "$SMPROGRAMS\Video converter for Shadertoy\Uninstall.lnk" "$INSTDIR\Uninstall.exe"

  WriteRegStr HKLM "Software\VideoConverterForShadertoy" "InstallDir" "$INSTDIR"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\VideoConverterForShadertoy" "DisplayName" "Video converter for Shadertoy"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\VideoConverterForShadertoy" "DisplayVersion" "1.0.0"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\VideoConverterForShadertoy" "Publisher" "Asger Andersen"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\VideoConverterForShadertoy" "DisplayIcon" "$INSTDIR\VideoConverterForShadertoy.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\VideoConverterForShadertoy" "UninstallString" "$INSTDIR\Uninstall.exe"
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\VideoConverterForShadertoy" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\VideoConverterForShadertoy" "NoRepair" 1
SectionEnd

Section "Desktop shortcut"
  CreateShortcut "$DESKTOP\Video converter for Shadertoy.lnk" "$INSTDIR\VideoConverterForShadertoy.exe" "" "$INSTDIR\VideoConverterForShadertoy.exe" 0
SectionEnd

Section "Uninstall"
  Delete "$DESKTOP\Video converter for Shadertoy.lnk"
  Delete "$SMPROGRAMS\Video converter for Shadertoy\Video converter for Shadertoy.lnk"
  Delete "$SMPROGRAMS\Video converter for Shadertoy\Uninstall.lnk"
  RMDir "$SMPROGRAMS\Video converter for Shadertoy"

  Delete "$INSTDIR\Uninstall.exe"
  RMDir /r "$INSTDIR"

  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\VideoConverterForShadertoy"
  DeleteRegKey HKLM "Software\VideoConverterForShadertoy"
SectionEnd
