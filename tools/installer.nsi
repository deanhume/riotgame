Unicode True
!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "WinVer.nsh"
!include "x64.nsh"
!include "${PAYLOAD_MANIFEST}"

!define UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\RiotControl"

Name "Riot Control"
OutFile "${OUTPUT_FILE}"
InstallDir "$LOCALAPPDATA\Programs\Riot Control"
InstallDirRegKey HKCU "${UNINSTALL_KEY}" "InstallLocation"
RequestExecutionLevel user
SetCompressor /SOLID lzma
ManifestDPIAware True
ShowInstDetails show
ShowUninstDetails show
VIProductVersion "${RIOT_VERSION}.0"
VIAddVersionKey "ProductName" "Riot Control"
VIAddVersionKey "FileDescription" "Riot Control Setup"
VIAddVersionKey "FileVersion" "${RIOT_VERSION}"
VIAddVersionKey "ProductVersion" "${RIOT_VERSION}"
VIAddVersionKey "LegalCopyright" ""

!define MUI_ABORTWARNING
!define MUI_ICON "${BRANDING_DIR}\RiotControl.ico"
!define MUI_UNICON "${BRANDING_DIR}\RiotControl.ico"
!define MUI_WELCOMEFINISHPAGE_BITMAP "${BRANDING_DIR}\installer-sidebar.bmp"
!define MUI_UNWELCOMEFINISHPAGE_BITMAP "${BRANDING_DIR}\installer-sidebar.bmp"
!define MUI_WELCOMEPAGE_TEXT "Hold the line. Save the curry sauce.$\r$\n$\r$\nThis installs Riot Control for your Windows account, including everything needed to play offline. Administrator rights are not required.$\r$\n$\r$\nClose the game before installing or updating."
!define MUI_FINISHPAGE_RUN "$INSTDIR\RiotControl.exe"
!define MUI_FINISHPAGE_RUN_NOTCHECKED
!define MUI_UNCONFIRMPAGE_TEXT_TOP "Remove Riot Control from this computer?$\r$\n$\r$\nYour saved stars, upgrades and wardrobe will be kept."
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH
!insertmacro MUI_LANGUAGE "English"

!macro CheckGameClosed
    FindWindow $0 "GLFW30" "Riot Control"
    ${If} $0 != 0
        MessageBox MB_OK|MB_ICONEXCLAMATION "Please close Riot Control, then run setup or uninstall again." /SD IDOK
        SetErrorLevel 2
        Abort
    ${EndIf}
!macroend

Function .onInit
    ${IfNot} ${RunningX64}
    ${OrIfNot} ${AtLeastWin10}
        MessageBox MB_OK|MB_ICONSTOP "Riot Control requires Windows 10 or 11 (64-bit)." /SD IDOK
        SetErrorLevel 1
        Abort
    ${EndIf}
    SetShellVarContext current
    SetRegView 64
    !insertmacro CheckGameClosed
FunctionEnd

Function un.onInit
    SetShellVarContext current
    SetRegView 64
    !insertmacro CheckGameClosed
FunctionEnd

Section "Riot Control"
    ClearErrors
    SetOverwrite on
    !insertmacro InstallPayload
    SetOutPath "$INSTDIR"
    WriteUninstaller "$INSTDIR\Uninstall.exe"
    CreateDirectory "$SMPROGRAMS\Riot Control"
    CreateShortcut "$SMPROGRAMS\Riot Control\Riot Control.lnk" "$INSTDIR\RiotControl.exe"
    WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayName" "Riot Control"
    WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayVersion" "${RIOT_VERSION}"
    WriteRegStr HKCU "${UNINSTALL_KEY}" "Publisher" "Riot Control"
    WriteRegStr HKCU "${UNINSTALL_KEY}" "InstallLocation" "$INSTDIR"
    WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayIcon" "$INSTDIR\RiotControl.exe"
    WriteRegStr HKCU "${UNINSTALL_KEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
    WriteRegStr HKCU "${UNINSTALL_KEY}" "QuietUninstallString" '"$INSTDIR\Uninstall.exe" /S'
    WriteRegDWORD HKCU "${UNINSTALL_KEY}" "NoModify" 1
    WriteRegDWORD HKCU "${UNINSTALL_KEY}" "NoRepair" 1
    WriteRegDWORD HKCU "${UNINSTALL_KEY}" "EstimatedSize" ${INSTALL_SIZE}
    ${If} ${Errors}
        MessageBox MB_OK|MB_ICONSTOP "Installation could not finish. Check that the folder is writable and the game is closed, then run setup again." /SD IDOK
        SetErrorLevel 1
        Abort
    ${EndIf}
SectionEnd

Section "Uninstall"
    ClearErrors
    !insertmacro UninstallPayload
    ${If} ${Errors}
        MessageBox MB_OK|MB_ICONSTOP "Some game files could not be removed. Close the game and try uninstalling again." /SD IDOK
        SetErrorLevel 1
        Abort
    ${EndIf}
    Delete "$SMPROGRAMS\Riot Control\Riot Control.lnk"
    RMDir "$SMPROGRAMS\Riot Control"
    DeleteRegKey HKCU "${UNINSTALL_KEY}"
    Delete "$INSTDIR\Uninstall.exe"
    !insertmacro RemovePayloadDirectories
    SetOutPath "$TEMP"
    RMDir "$INSTDIR"
    ; Saves live outside the install folder in $LOCALAPPDATA\RiotControl and are not touched.
SectionEnd
