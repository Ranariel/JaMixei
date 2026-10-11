cmake_minimum_required(VERSION 3.22)
set(MEUPAD_VERSION "1.4.0" CACHE STRING "Versao do Ja Mixei (o GitHub Actions passa 1.4.N)")
project(MeuPadStudio VERSION ${MEUPAD_VERSION} LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED JUCE_DIR)
  set(JUCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE)
endif()
add_subdirectory(${JUCE_DIR} JUCE EXCLUDE_FROM_ALL)
if(NOT DEFINED RUBBERBAND_DIR)
  set(RUBBERBAND_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/rubberband)
endif()

# ---------------------------------------------------------------------------
# LAME 3.100 (encoder MP3) compilado como biblioteca estatica
if(NOT DEFINED LAME_DIR)
  set(LAME_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/lame)
endif()
file(GLOB LAME_SOURCES ${LAME_DIR}/libmp3lame/*.c)
set(LAME_CFG ${CMAKE_CURRENT_BINARY_DIR}/lamecfg)
file(WRITE ${LAME_CFG}/config.h
"#pragma once
#define STDC_HEADERS 1
#define HAVE_STDINT_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_STRING_H 1
#define HAVE_STDLIB_H 1
#define HAVE_LIMITS_H 1
#define HAVE_ERRNO_H 1
#define HAVE_FCNTL_H 1
#define HAVE_MEMCPY 1
#define HAVE_STRCHR 1
#define PROTOTYPES 1
#define USE_FAST_LOG 1
#define PACKAGE \"lame\"
#define VERSION \"3.100\"
typedef float ieee754_float32_t;
#if defined(_MSC_VER)
#pragma warning(disable: 4244 4305 4996 4267 4018)
#endif
")
add_library(mp3lame STATIC ${LAME_SOURCES})
target_include_directories(mp3lame PUBLIC ${LAME_DIR}/include PRIVATE ${LAME_DIR}/libmp3lame ${LAME_CFG})
target_compile_definitions(mp3lame PRIVATE HAVE_CONFIG_H _CRT_SECURE_NO_WARNINGS)
set_target_properties(mp3lame PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
  set_property(TARGET mp3lame PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
  target_compile_options(mp3lame PRIVATE -w -O3)
endif()

juce_add_gui_app(MeuPadStudio
  PRODUCT_NAME "Ja Mixei"
  COMPANY_NAME "Ranariel"
  ICON_BIG ${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon.png
  ICON_SMALL ${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon.png
  VERSION ${MEUPAD_VERSION})

juce_generate_juce_header(MeuPadStudio)

# fontes embutidas (Inter e JetBrains Mono, licenca OFL)
juce_add_binary_data(MeuPadData NAMESPACE MeuPadData HEADER_NAME MeuPadData.h SOURCES
  Resources/fonts/Inter-Medium.ttf Resources/fonts/Inter-Bold.ttf
  Resources/fonts/JetBrainsMono-Medium.ttf Resources/fonts/JetBrainsMono-Bold.ttf
  Resources/fonts/NotoSansSC-Medium.ttf Resources/fonts/NotoSansSC-Bold.ttf)
set_target_properties(MeuPadData PROPERTIES POSITION_INDEPENDENT_CODE ON)

file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp)
if(MEUPAD_DBGINFO)
  set_source_files_properties(${APP_SOURCES} PROPERTIES COMPILE_OPTIONS "-g1")
endif()
target_sources(MeuPadStudio PRIVATE ${APP_SOURCES} ${RUBBERBAND_DIR}/single/RubberBandSingle.cpp)
target_include_directories(MeuPadStudio PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source ${RUBBERBAND_DIR})

target_compile_definitions(MeuPadStudio PRIVATE
  MEUPAD_VERSION="${MEUPAD_VERSION}"
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:MeuPadStudio,JUCE_PRODUCT_NAME>"
  JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:MeuPadStudio,JUCE_VERSION>"
  JUCE_USE_MP3AUDIOFORMAT=1 JUCE_USE_FLAC=1 JUCE_USE_OGGVORBIS=1 JUCE_WASAPI=1 JUCE_DIRECTSOUND=1
  JUCE_DISPLAY_SPLASH_SCREEN=0 USE_BUILTIN_FFT=1 NO_THREAD_CHECKS=1 NO_TIMING=1 _USE_MATH_DEFINES=1)

target_link_libraries(MeuPadStudio PRIVATE
  juce::juce_gui_extra juce::juce_audio_utils juce::juce_audio_formats juce::juce_dsp juce::juce_opengl juce::juce_cryptography
  juce::juce_recommended_config_flags mp3lame MeuPadData)

if(WIN32)
  target_link_libraries(MeuPadStudio PRIVATE mfuuid ole32 opengl32)
  # recursos proprios (versao em portugues do Brasil + icone) no lugar dos gerados pelo JUCE
  set(MEUPAD_ICON "${CMAKE_CURRENT_SOURCE_DIR}/Resources/app.ico")
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Resources/app.rc.in ${CMAKE_CURRENT_BINARY_DIR}/app.rc @ONLY)
  if(TARGET MeuPadStudio_rc_lib)
    set_property(TARGET MeuPadStudio_rc_lib PROPERTY SOURCES ${CMAKE_CURRENT_BINARY_DIR}/app.rc)
  else()
    target_sources(MeuPadStudio PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/app.rc)
  endif()
endif()
if(MSVC)
  target_compile_options(MeuPadStudio PRIVATE /bigobj /MP /utf-8)
  set_property(TARGET MeuPadStudio PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
if(MINGW)
  target_link_options(MeuPadStudio PRIVATE -static -static-libgcc -static-libstdc++)
endif()

# ---------------------------------------------------------------------------
# Testes do motor (opcional, para desenvolvimento)
option(MEUPAD_TESTS "Compilar testes do motor" OFF)
if(MEUPAD_TESTS)
  juce_add_console_app(MeuPadTests PRODUCT_NAME "MeuPadTests")
  juce_generate_juce_header(MeuPadTests)
  file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Source/Audio/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/Source/Model/*.cpp)
  target_sources(MeuPadTests PRIVATE tests/EngineTest.cpp ${CORE_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/Source/Session.cpp ${CMAKE_CURRENT_SOURCE_DIR}/Source/I18n.cpp ${RUBBERBAND_DIR}/single/RubberBandSingle.cpp)
  target_include_directories(MeuPadTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source ${RUBBERBAND_DIR})
  target_compile_definitions(MeuPadTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_USE_MP3AUDIOFORMAT=1 JUCE_USE_FLAC=1 JUCE_USE_OGGVORBIS=1 USE_BUILTIN_FFT=1 NO_THREAD_CHECKS=1 NO_TIMING=1)
  target_link_libraries(MeuPadTests PRIVATE juce::juce_audio_utils juce::juce_audio_formats juce::juce_dsp juce::juce_gui_extra juce::juce_recommended_config_flags mp3lame)
endif()
