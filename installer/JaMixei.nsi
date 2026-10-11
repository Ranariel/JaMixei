; Instalador do Ja Mixei (NSIS) - criado por Ranariel
; Idiomas: Portugues (Brasil, padrao), English, Espanol, Chines simplificado
; Gera: Ja-Mixei-Instalador-<versao>.exe  (makensis /DAPPVER=1.4.N JaMixei.nsi)
; Atualizacao automatica: o programa roda o instalador com /S (silencioso)
Unicode true
!include "MUI2.nsh"
!include "x64.nsh"
!include "LogicLib.nsh"
!include "FileFunc.nsh"

!define APPNAME "Ja Mixei"
!ifndef APPVER
  !define APPVER "1.4.0"
!endif
!define EXENAME "Ja Mixei.exe"
!define UNINSTKEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\MeuPadStudio"

Name "${APPNAME}"
OutFile "Ja-Mixei-Instalador-${APPVER}.exe"
InstallDir "$PROGRAMFILES64\${APPNAME}"
InstallDirRegKey HKLM "Software\JaMixei" "InstallDir"
RequestExecutionLevel admin
SetCompressor /SOLID lzma
BrandingText "Ja Mixei ${APPVER}  -  Ranariel"

!define MUI_ICON "meupad.ico"
!define MUI_UNICON "meupad.ico"
!define MUI_ABORTWARNING
!define MUI_WELCOMEFINISHPAGE_BITMAP "welcome.bmp"
!define MUI_UNWELCOMEFINISHPAGE_BITMAP "welcome.bmp"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_RIGHT
!define MUI_HEADERIMAGE_BITMAP "header.bmp"
!define MUI_WELCOMEPAGE_TITLE "$(TxtWelcomeTitle)"
!define MUI_WELCOMEPAGE_TEXT "$(TxtWelcomeText)"
!define MUI_FINISHPAGE_RUN "$INSTDIR\${EXENAME}"
!define MUI_FINISHPAGE_RUN_TEXT "$(TxtRunNow)"
!define MUI_FINISHPAGE_SHOWREADME "$INSTDIR\LEIA-ME.txt"
!define MUI_FINISHPAGE_SHOWREADME_TEXT "$(TxtReadme)"

; lembra o idioma escolhido (o desinstalador usa o mesmo)
!define MUI_LANGDLL_REGISTRY_ROOT "HKLM"
!define MUI_LANGDLL_REGISTRY_KEY "Software\JaMixei"
!define MUI_LANGDLL_REGISTRY_VALUENAME "InstallerLanguage"
!define MUI_LANGDLL_WINDOWTITLE "Ja Mixei"
!define MUI_LANGDLL_INFO "Escolha o idioma / Choose a language / Elija un idioma / 选择语言"
!define MUI_LANGDLL_ALLLANGUAGES
!define MUI_LANGDLL_ALWAYSSHOW

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
Page custom ShortcutPage ShortcutPageLeave
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

; o primeiro idioma e o padrao
!insertmacro MUI_LANGUAGE "PortugueseBR"
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "Spanish"
!insertmacro MUI_LANGUAGE "SimpChinese"
!insertmacro MUI_RESERVEFILE_LANGDLL

; ---- textos traduzidos
LangString TxtWelcomeTitle ${LANG_PORTUGUESEBR} "Instalar o ${APPNAME} ${APPVER}"
LangString TxtWelcomeTitle ${LANG_ENGLISH}      "Install ${APPNAME} ${APPVER}"
LangString TxtWelcomeTitle ${LANG_SPANISH}      "Instalar ${APPNAME} ${APPVER}"
LangString TxtWelcomeTitle ${LANG_SIMPCHINESE}  "安装 ${APPNAME} ${APPVER}"

LangString TxtWelcomeText ${LANG_PORTUGUESEBR} "Sampler e remixador estilo MPC para Windows.$\r$\nCriado por Ranariel.$\r$\n$\r$\nO assistente vai instalar o programa, criar atalhos e associar os projetos .mpad.$\r$\n$\r$\nClique em Próximo para continuar."
LangString TxtWelcomeText ${LANG_ENGLISH}      "MPC-style sampler and remixer for Windows.$\r$\nCreated by Ranariel.$\r$\n$\r$\nThis wizard will install the program, create shortcuts and associate .mpad projects.$\r$\n$\r$\nClick Next to continue."
LangString TxtWelcomeText ${LANG_SPANISH}      "Sampler y remezclador estilo MPC para Windows.$\r$\nCreado por Ranariel.$\r$\n$\r$\nEl asistente instalará el programa, creará accesos directos y asociará los proyectos .mpad.$\r$\n$\r$\nHaga clic en Siguiente para continuar."
LangString TxtWelcomeText ${LANG_SIMPCHINESE}  "适用于 Windows 的 MPC 风格采样器和混音软件。$\r$\n作者：Ranariel。$\r$\n$\r$\n本向导将安装程序、创建快捷方式并关联 .mpad 项目。$\r$\n$\r$\n点击“下一步”继续。"

LangString TxtRunNow ${LANG_PORTUGUESEBR} "Abrir o ${APPNAME} agora"
LangString TxtRunNow ${LANG_ENGLISH}      "Open ${APPNAME} now"
LangString TxtRunNow ${LANG_SPANISH}      "Abrir ${APPNAME} ahora"
LangString TxtRunNow ${LANG_SIMPCHINESE}  "立即打开 ${APPNAME}"

LangString TxtReadme ${LANG_PORTUGUESEBR} "Ver o guia rápido (LEIA-ME)"
LangString TxtReadme ${LANG_ENGLISH}      "View the quick guide (Portuguese)"
LangString TxtReadme ${LANG_SPANISH}      "Ver la guía rápida (en portugués)"
LangString TxtReadme ${LANG_SIMPCHINESE}  "查看快速指南（葡萄牙语）"

LangString TxtNeed64 ${LANG_PORTUGUESEBR} "O ${APPNAME} precisa do Windows 64 bits."
LangString TxtNeed64 ${LANG_ENGLISH}      "${APPNAME} requires 64-bit Windows."
LangString TxtNeed64 ${LANG_SPANISH}      "${APPNAME} necesita Windows de 64 bits."
LangString TxtNeed64 ${LANG_SIMPCHINESE}  "${APPNAME} 需要 64 位 Windows。"

LangString TxtShortcuts ${LANG_PORTUGUESEBR} "Atalhos"
LangString TxtShortcuts ${LANG_ENGLISH}      "Shortcuts"
LangString TxtShortcuts ${LANG_SPANISH}      "Accesos directos"
LangString TxtShortcuts ${LANG_SIMPCHINESE}  "快捷方式"

LangString TxtShortcutsSub ${LANG_PORTUGUESEBR} "Escolha onde criar atalhos."
LangString TxtShortcutsSub ${LANG_ENGLISH}      "Choose where to create shortcuts."
LangString TxtShortcutsSub ${LANG_SPANISH}      "Elija dónde crear accesos directos."
LangString TxtShortcutsSub ${LANG_SIMPCHINESE}  "选择在哪里创建快捷方式。"

LangString TxtDesktop ${LANG_PORTUGUESEBR} "Criar atalho na Área de Trabalho"
LangString TxtDesktop ${LANG_ENGLISH}      "Create a desktop shortcut"
LangString TxtDesktop ${LANG_SPANISH}      "Crear acceso directo en el Escritorio"
LangString TxtDesktop ${LANG_SIMPCHINESE}  "创建桌面快捷方式"

LangString TxtStartMenu ${LANG_PORTUGUESEBR} "Um atalho no Menu Iniciar sempre é criado."
LangString TxtStartMenu ${LANG_ENGLISH}      "A Start menu shortcut is always created."
LangString TxtStartMenu ${LANG_SPANISH}      "Siempre se crea un acceso directo en el menú Inicio."
LangString TxtStartMenu ${LANG_SIMPCHINESE}  "始终会在“开始”菜单中创建快捷方式。"

LangString TxtGuideLnk ${LANG_PORTUGUESEBR} "Guia rápido (LEIA-ME)"
LangString TxtGuideLnk ${LANG_ENGLISH}      "Quick guide (README)"
LangString TxtGuideLnk ${LANG_SPANISH}      "Guía rápida (LÉAME)"
LangString TxtGuideLnk ${LANG_SIMPCHINESE}  "快速指南（说明）"

LangString TxtUninstLnk ${LANG_PORTUGUESEBR} "Desinstalar ${APPNAME}"
LangString TxtUninstLnk ${LANG_ENGLISH}      "Uninstall ${APPNAME}"
LangString TxtUninstLnk ${LANG_SPANISH}      "Desinstalar ${APPNAME}"
LangString TxtUninstLnk ${LANG_SIMPCHINESE}  "卸载 ${APPNAME}"

LangString TxtFileType ${LANG_PORTUGUESEBR} "Projeto do Ja Mixei"
LangString TxtFileType ${LANG_ENGLISH}      "Ja Mixei project"
LangString TxtFileType ${LANG_SPANISH}      "Proyecto de Ja Mixei"
LangString TxtFileType ${LANG_SIMPCHINESE}  "Ja Mixei 项目"

LangString TxtAppLang ${LANG_PORTUGUESEBR} "pt"
LangString TxtAppLang ${LANG_ENGLISH}      "en"
LangString TxtAppLang ${LANG_SPANISH}      "es"
LangString TxtAppLang ${LANG_SIMPCHINESE}  "zh"

; ---- propriedades do arquivo (aparece em portugues no Windows)
VIProductVersion "${APPVER}.0"
VIAddVersionKey /LANG=${LANG_PORTUGUESEBR} "ProductName" "${APPNAME}"
VIAddVersionKey /LANG=${LANG_PORTUGUESEBR} "FileDescription" "Instalador do ${APPNAME}"
VIAddVersionKey /LANG=${LANG_PORTUGUESEBR} "FileVersion" "${APPVER}"
VIAddVersionKey /LANG=${LANG_PORTUGUESEBR} "ProductVersion" "${APPVER}"
VIAddVersionKey /LANG=${LANG_PORTUGUESEBR} "LegalCopyright" "© 2026 Ranariel"
VIAddVersionKey /LANG=${LANG_PORTUGUESEBR} "CompanyName" "Ranariel"

!include "nsDialogs.nsh"
Var DesktopCheck
Var WantDesktop

Function .onInit
  SetRegView 64
  StrCpy $LANGUAGE ${LANG_PORTUGUESEBR}   ; portugues do Brasil e o padrao
  !insertmacro MUI_LANGDLL_DISPLAY
  ${IfNot} ${RunningX64}
    MessageBox MB_ICONSTOP "$(TxtNeed64)"
    Abort
  ${EndIf}
  StrCpy $WantDesktop ${BST_CHECKED}
  ${If} ${Silent}
    ; atualizacao: so recria o atalho da area de trabalho se ele ja existia
    StrCpy $WantDesktop ${BST_UNCHECKED}
    ${If} ${FileExists} "$DESKTOP\${APPNAME}.lnk"
      StrCpy $WantDesktop ${BST_CHECKED}
    ${EndIf}
  ${EndIf}
FunctionEnd

; o programa pode ainda estar fechando: tenta apagar o .exe antigo por ate ~15 s
Function WaitForAppClosed
  StrCpy $2 0
  ${DoWhile} ${FileExists} "$INSTDIR\${EXENAME}"
    ClearErrors
    Delete "$INSTDIR\${EXENAME}"
    ${IfNot} ${Errors}
      ${Break}
    ${EndIf}
    IntOp $2 $2 + 1
    ${If} $2 > 30
      ${Break}
    ${EndIf}
    Sleep 500
  ${Loop}
FunctionEnd

Function ShortcutPage
  !insertmacro MUI_HEADER_TEXT "$(TxtShortcuts)" "$(TxtShortcutsSub)"
  nsDialogs::Create 1018
  Pop $0
  ${NSD_CreateCheckbox} 0 0 100% 12u "$(TxtDesktop)"
  Pop $DesktopCheck
  ${NSD_SetState} $DesktopCheck $WantDesktop
  ${NSD_CreateLabel} 0 20u 100% 24u "$(TxtStartMenu)"
  Pop $0
  nsDialogs::Show
FunctionEnd

Function ShortcutPageLeave
  ${NSD_GetState} $DesktopCheck $WantDesktop
FunctionEnd

Section "Programa" SecMain
  SectionIn RO
  ; remove atalhos da versao antiga (nome MeuPad Studio)
  Delete "$SMPROGRAMS\MeuPad Studio\*.lnk"
  RMDir "$SMPROGRAMS\MeuPad Studio"
  Delete "$DESKTOP\MeuPad Studio.lnk"
  ReadRegStr $1 HKLM "Software\MeuPadStudio" "InstallDir"
  ${If} $1 != ""
    Delete "$1\MeuPad Studio.exe"
    Delete "$1\LEIA-ME.txt"
    Delete "$1\meupad.ico"
    Delete "$1\Desinstalar.exe"
    RMDir "$1"
    DeleteRegKey HKLM "Software\MeuPadStudio"
  ${EndIf}
  SetOutPath "$INSTDIR"
  Call WaitForAppClosed
  File "/oname=${EXENAME}" "payload/Ja Mixei.exe"
  File "/oname=LEIA-ME.txt" "payload/LEIA-ME.md"
  File "meupad.ico"

  ; atalhos (recria com os nomes no idioma escolhido)
  Delete "$SMPROGRAMS\${APPNAME}\*.lnk"
  CreateDirectory "$SMPROGRAMS\${APPNAME}"
  CreateShortCut "$SMPROGRAMS\${APPNAME}\${APPNAME}.lnk" "$INSTDIR\${EXENAME}" "" "$INSTDIR\${EXENAME}" 0
  CreateShortCut "$SMPROGRAMS\${APPNAME}\$(TxtGuideLnk).lnk" "$INSTDIR\LEIA-ME.txt"
  CreateShortCut "$SMPROGRAMS\${APPNAME}\$(TxtUninstLnk).lnk" "$INSTDIR\Desinstalar.exe"
  ${If} $WantDesktop == ${BST_CHECKED}
    CreateShortCut "$DESKTOP\${APPNAME}.lnk" "$INSTDIR\${EXENAME}" "" "$INSTDIR\${EXENAME}" 0
  ${EndIf}

  ; associacao de arquivos .mpad
  WriteRegStr HKCR ".mpad" "" "MeuPadStudio.Projeto"
  WriteRegStr HKCR "MeuPadStudio.Projeto" "" "$(TxtFileType)"
  WriteRegStr HKCR "MeuPadStudio.Projeto\DefaultIcon" "" "$INSTDIR\meupad.ico"
  WriteRegStr HKCR "MeuPadStudio.Projeto\shell\open\command" "" '"$INSTDIR\${EXENAME}" "%1"'
  System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, i 0, i 0)'

  ; idioma do programa = idioma escolhido na instalacao
  ; (na atualizacao silenciosa mantem o idioma que a pessoa ja usa)
  ReadRegStr $3 HKLM "Software\JaMixei" "Language"
  ${IfNot} ${Silent}
  ${OrIf} $3 == ""
    WriteRegStr HKLM "Software\JaMixei" "Language" "$(TxtAppLang)"
  ${EndIf}

  ; desinstalador + "Aplicativos instalados"
  WriteUninstaller "$INSTDIR\Desinstalar.exe"
  WriteRegStr HKLM "Software\JaMixei" "InstallDir" "$INSTDIR"
  WriteRegStr HKLM "${UNINSTKEY}" "DisplayName" "${APPNAME}"
  WriteRegStr HKLM "${UNINSTKEY}" "DisplayVersion" "${APPVER}"
  WriteRegStr HKLM "${UNINSTKEY}" "Publisher" "Ranariel"
  WriteRegStr HKLM "${UNINSTKEY}" "DisplayIcon" "$INSTDIR\${EXENAME}"
  WriteRegStr HKLM "${UNINSTKEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "${UNINSTKEY}" "UninstallString" '"$INSTDIR\Desinstalar.exe"'
  WriteRegDWORD HKLM "${UNINSTKEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNINSTKEY}" "NoRepair" 1
  WriteRegDWORD HKLM "${UNINSTKEY}" "EstimatedSize" 12000
SectionEnd

; atualizacao silenciosa pedida pelo proprio programa (/S /RELAUNCH): reabre o Ja Mixei
; pelo Explorer, para ele rodar como usuario normal (nao como administrador)
Function .onInstSuccess
  ${If} ${Silent}
    ${GetParameters} $0
    ClearErrors
    ${GetOptions} $0 "/RELAUNCH" $1
    ${IfNot} ${Errors}
      Exec '"$WINDIR\explorer.exe" "$INSTDIR\${EXENAME}"'
    ${EndIf}
  ${EndIf}
FunctionEnd

Function un.onInit
  SetRegView 64
  !insertmacro MUI_UNGETLANGUAGE
FunctionEnd

Section "Uninstall"
  Delete "$INSTDIR\${EXENAME}"
  Delete "$INSTDIR\LEIA-ME.txt"
  Delete "$INSTDIR\meupad.ico"
  Delete "$INSTDIR\Desinstalar.exe"
  RMDir "$INSTDIR"
  Delete "$SMPROGRAMS\${APPNAME}\*.lnk"
  RMDir "$SMPROGRAMS\${APPNAME}"
  Delete "$DESKTOP\${APPNAME}.lnk"
  ReadRegStr $0 HKCR ".mpad" ""
  ${If} $0 == "MeuPadStudio.Projeto"
    DeleteRegKey HKCR ".mpad"
  ${EndIf}
  DeleteRegKey HKCR "MeuPadStudio.Projeto"
  DeleteRegKey HKLM "${UNINSTKEY}"
  DeleteRegKey HKLM "Software\JaMixei"
  DeleteRegKey HKLM "Software\MeuPadStudio"
  System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, i 0, i 0)'
  ; projetos, configuracoes e auto-salvamentos do usuario sao mantidos
SectionEnd
