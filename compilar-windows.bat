@echo off
REM ==========================================================================
REM  Ja Mixei - compilacao no Windows (Visual Studio 2022 + CMake + Git)
REM  Requisitos: Visual Studio 2022 com "Desenvolvimento para desktop com C++",
REM              CMake (vem junto com o VS) e Git.
REM  Uso: de duplo clique, ou rode no "Developer Command Prompt for VS 2022".
REM ==========================================================================
setlocal
cd /d "%~dp0"

if not exist external mkdir external

if not exist external\JUCE (
    echo Baixando JUCE 7.0.12...
    git clone --depth 1 --branch 7.0.12 https://github.com/juce-framework/JUCE.git external\JUCE || goto :erro
)

if not exist external\rubberband (
    echo Baixando Rubber Band 4.0.0...
    git clone https://github.com/breakfastquay/rubberband.git external\rubberband || goto :erro
    git -C external\rubberband checkout e4296ac80b1170018a110bc326fd0d45a0eb27d6 || goto :erro
)

if not exist external\lame (
    echo Baixando LAME 3.100 - encoder MP3...
    git clone https://github.com/lameproject/lame.git external\lame || goto :erro
    git -C external\lame checkout 1f5cc9487284d5950343aa5d4f70de433468070a || goto :erro
)

cmake -S . -B build -G "Visual Studio 17 2022" -A x64 || goto :erro
cmake --build build --config Release --target MeuPadStudio || goto :erro

echo.
echo Pronto! O programa esta em:
echo   build\MeuPadStudio_artefacts\Release\Ja Mixei.exe
pause
exit /b 0

:erro
echo.
echo *** Falha na compilacao. Verifique se o Visual Studio 2022, CMake e Git estao instalados. ***
pause
exit /b 1
