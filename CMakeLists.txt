cmake_minimum_required(VERSION 3.20)
project(Sep2026LoadingCrashFix CXX)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
	message(FATAL_ERROR "Oblivion, Fallout 3 and New Vegas are 32-bit: configure with a 32-bit toolchain (vcvars32.bat or -A Win32).")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Static runtime: the plugin carries no dependency on an installed VC++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
add_compile_options(/W4 /permissive- /utf-8)
add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)

add_library(decoder_block STATIC src/DecoderBlock.cpp)
target_include_directories(decoder_block PUBLIC src)
target_link_libraries(decoder_block PUBLIC ole32)

add_library(plugin_entry STATIC src/PluginEntry.cpp)
target_include_directories(plugin_entry PUBLIC src)

add_library(Sep2026LoadingCrashFix SHARED src/Plugin.cpp)
target_link_libraries(Sep2026LoadingCrashFix PRIVATE decoder_block plugin_entry)

enable_testing()

add_executable(DecoderBlockTest tests/DecoderBlockTest.cpp)
target_link_libraries(DecoderBlockTest PRIVATE decoder_block)
add_test(NAME DecoderBlockTest COMMAND DecoderBlockTest)

add_executable(PluginEntryTest tests/PluginEntryTest.cpp)
target_link_libraries(PluginEntryTest PRIVATE plugin_entry)
add_test(NAME PluginEntryTest COMMAND PluginEntryTest)

add_executable(ExportsTest tests/ExportsTest.cpp)
add_dependencies(ExportsTest Sep2026LoadingCrashFix)
add_test(NAME ExportsTest COMMAND ExportsTest $<TARGET_FILE:Sep2026LoadingCrashFix>)

add_executable(RenderTest tests/RenderTest.cpp)
target_link_libraries(RenderTest PRIVATE decoder_block strmiids)
set(MUSIC_TEST_FILE "C:/Steam/steamapps/common/Oblivion/Data/Music/Special/tes4title.mp3"
    CACHE FILEPATH "An MP3 for the DirectShow integration test")
if(EXISTS "${MUSIC_TEST_FILE}")
	add_test(NAME RenderTest COMMAND RenderTest "${MUSIC_TEST_FILE}")
else()
	message(WARNING "MUSIC_TEST_FILE not found - RenderTest is built but not registered with CTest")
endif()
