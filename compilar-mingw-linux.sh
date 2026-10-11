#!/bin/bash
# Compila o .exe do Windows a partir do Linux (MinGW-w64), como foi feito na versao entregue.
# Requisitos (Ubuntu/Debian): sudo apt install cmake ninja-build git mingw-w64
# Para testar o motor de audio no Linux: adicione -DMEUPAD_TESTS=ON e rode o alvo MeuPadTests.
set -e
cd "$(dirname "$0")"
mkdir -p external
[ -d external/JUCE ] || git clone --depth 1 --branch 7.0.12 https://github.com/juce-framework/JUCE.git external/JUCE
if [ ! -d external/rubberband ]; then
  git clone https://github.com/breakfastquay/rubberband.git external/rubberband
  git -C external/rubberband checkout e4296ac80b1170018a110bc326fd0d45a0eb27d6
fi
if [ ! -d external/lame ]; then
  git clone https://github.com/lameproject/lame.git external/lame
  git -C external/lame checkout 1f5cc9487284d5950343aa5d4f70de433468070a
fi
cmake -S . -B build-win -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-win --target MeuPadStudio
x86_64-w64-mingw32-strip "build-win/MeuPadStudio_artefacts/Release/Ja Mixei.exe"
echo "Pronto: build-win/MeuPadStudio_artefacts/Release/Ja Mixei.exe"
