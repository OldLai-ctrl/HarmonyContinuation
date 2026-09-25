cmake_minimum_required(VERSION 3.25)
project(HarmonyContinuation VERSION 0.0.1 LANGUAGES C CXX)
# On this Chinese MSVC installation Ninja records zero /showIncludes entries
# (localized compiler output does not match CMake's generated prefix). Header
# edits can then leave old object files using stale C++ class layouts. Keep
# explicit dependencies for our small set of project sources until the SDK
# toolchain supplies reliable localized dependency scanning.
file(GLOB_RECURSE HC_LOCAL_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.h")
file(GLOB_RECURSE HC_LOCAL_CPP CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")
set_source_files_properties(${HC_LOCAL_CPP} PROPERTIES OBJECT_DEPENDS "${HC_LOCAL_HEADERS}")
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Single-configuration build type" FORCE)
endif()

option(HC_BUILD_PLUGIN "Build SDK-backed VST3" ON)
set(VST3_SDK_ROOT "$ENV{VST3_SDK_ROOT}" CACHE PATH "Local complete VST3 SDK 3.8.x checkout with VSTGUI")
if(NOT VST3_SDK_ROOT AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../VST_SDK/vst3sdk/pluginterfaces/vst/ivstprocesscontext.h")
    set(VST3_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../VST_SDK/vst3sdk" CACHE PATH "Local VST3 SDK root" FORCE)
endif()
if(HC_BUILD_PLUGIN)
    if(NOT EXISTS "${VST3_SDK_ROOT}/pluginterfaces/vst/ivstprocesscontext.h")
        message(FATAL_ERROR "Provide VST3_SDK_ROOT pointing to an offline SDK 3.8.x checkout. See docs/HOST_SPIKE.md.")
    endif()
    set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES ON CACHE BOOL "" FORCE)
    set(SMTG_ENABLE_VSTGUI_SUPPORT ON CACHE BOOL "" FORCE)
    set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "" FORCE)
    set(VSTGUI_STANDALONE OFF CACHE BOOL "" FORCE)
    set(VSTGUI_TOOLS OFF CACHE BOOL "" FORCE)
    add_subdirectory("${VST3_SDK_ROOT}" sdk)
endif()

if(HC_BUILD_PLUGIN)
    smtg_add_vst3plugin(HarmonyContinuation
        src/plugin/Processor.cpp src/plugin/Controller.cpp src/plugin/PluginEntry.cpp
        src/plugin/HostContextAdapter.cpp src/plugin/DropCapture.cpp src/plugin/PluginView.cpp
        src/plugin/RecommendationWorker.cpp
        src/ui/MainView.cpp)
    if(WIN32)
        target_sources(HarmonyContinuation PRIVATE src/plugin/platform/windows/ClipboardInspector.cpp)
    endif()
    # The SDK helper's source-dir variables are scoped to its subdirectory;
    # supply the verified Windows module entry explicitly in this parent scope.
    target_sources(HarmonyContinuation PRIVATE "${VST3_SDK_ROOT}/public.sdk/source/main/dllmain.cpp")
    # The SDK's package helper assumes a multi-config IDE generator. Keep the
    # bundle layout correct with single-config Ninja as well.
    set_target_properties(HarmonyContinuation PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/VST3/$<CONFIG>/HarmonyContinuation.vst3/Contents/x86_64-win")
    target_link_libraries(HarmonyContinuation PRIVATE sdk vstgui_support harmony_xml harmony_dev_json)
    target_compile_features(HarmonyContinuation PRIVATE cxx_std_20)
    target_compile_definitions(HarmonyContinuation PRIVATE NOMINMAX)
    if(MSVC)
        target_compile_options(HarmonyContinuation PRIVATE /utf-8 /W4)
    endif()
endif()

add_library(harmony_core STATIC src/core/ChordNormalizer.cpp src/core/HarmonyAnalysis.cpp
    src/core/ProgressionMatcher.cpp src/core/ContinuationEngine.cpp)
target_include_directories(harmony_core PUBLIC src)
target_compile_features(harmony_core PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(harmony_core PRIVATE /W4 /permissive- /utf-8)
endif()

set(HC_DEVELOPMENT_TEMPLATES "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/progressions/templates.json")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${HC_DEVELOPMENT_TEMPLATES}")
file(READ "${HC_DEVELOPMENT_TEMPLATES}" HC_EMBEDDED_TEMPLATES)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file(tools/EmbeddedTemplates.h.in "${CMAKE_CURRENT_BINARY_DIR}/generated/EmbeddedTemplates.h" @ONLY)
set_property(SOURCE tools/TemplateJson.cpp APPEND PROPERTY OBJECT_DEPENDS
    "${CMAKE_CURRENT_BINARY_DIR}/generated/EmbeddedTemplates.h")
add_library(harmony_dev_json STATIC tools/ProgressionJson.cpp tools/TemplateJson.cpp)
target_link_libraries(harmony_dev_json PUBLIC harmony_core)
target_compile_features(harmony_dev_json PUBLIC cxx_std_20)
target_include_directories(harmony_dev_json PUBLIC tools "${CMAKE_CURRENT_BINARY_DIR}/generated")
add_library(harmony_library STATIC src/library/ProgressionLibrary.cpp)
target_include_directories(harmony_library PUBLIC src)
target_link_libraries(harmony_library PUBLIC harmony_dev_json)
target_compile_features(harmony_library PUBLIC cxx_std_20)
if(WIN32)
    target_link_libraries(harmony_library PUBLIC winsqlite3)
else()
    find_package(SQLite3 REQUIRED)
    target_link_libraries(harmony_library PUBLIC SQLite::SQLite3)
endif()
add_library(harmony_product STATIC src/session/PluginSessionState.cpp src/session/ProductServices.cpp)
target_link_libraries(harmony_product PUBLIC harmony_library)
target_include_directories(harmony_product PUBLIC src)
target_compile_features(harmony_product PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(harmony_product PRIVATE /W4 /permissive- /utf-8)
endif()
add_library(harmony_preview STATIC src/preview/PreviewSequence.cpp src/preview/ChordVoicer.cpp
    src/preview/PreviewSynth.cpp src/preview/OfflinePreviewRenderer.cpp)
target_link_libraries(harmony_preview PUBLIC harmony_core)
target_include_directories(harmony_preview PUBLIC src)
target_compile_features(harmony_preview PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(harmony_preview PRIVATE /W4 /permissive- /utf-8)
endif()
add_library(harmony_midi STATIC src/midi/MidiClip.cpp src/midi/StandardMidiFileWriter.cpp)
target_link_libraries(harmony_midi PUBLIC harmony_preview)
target_include_directories(harmony_midi PUBLIC src)
target_compile_features(harmony_midi PUBLIC cxx_std_20)
add_library(harmony_snapshot STATIC src/snapshot/RecommendationSnapshot.cpp)
target_link_libraries(harmony_snapshot PUBLIC harmony_midi)
target_include_directories(harmony_snapshot PUBLIC src)
target_compile_features(harmony_snapshot PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(harmony_midi PRIVATE /W4 /permissive- /utf-8)
    target_compile_options(harmony_snapshot PRIVATE /W4 /permissive- /utf-8)
endif()
add_library(harmony_demo_scenarios STATIC src/demo/DemoScenario.cpp)
target_link_libraries(harmony_demo_scenarios PUBLIC harmony_product)
target_include_directories(harmony_demo_scenarios PUBLIC src tools)
target_compile_features(harmony_demo_scenarios PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(harmony_demo_scenarios PRIVATE /utf-8)
endif()
if(HC_BUILD_PLUGIN AND WIN32)
    add_executable(HarmonyContinuationDemo WIN32 src/demo/DemoMain.cpp
        src/plugin/RecommendationWorker.cpp src/ui/MainView.cpp)
    target_link_libraries(HarmonyContinuationDemo PRIVATE harmony_demo_scenarios harmony_preview harmony_midi harmony_snapshot vstgui_support comctl32 shell32 winmm comdlg32)
    target_compile_features(HarmonyContinuationDemo PRIVATE cxx_std_20)
    target_compile_definitions(HarmonyContinuationDemo PRIVATE NOMINMAX
        HC_DEMO_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/demo")
    target_include_directories(HarmonyContinuationDemo PRIVATE "${VST3_SDK_ROOT}")
    add_dependencies(HarmonyContinuationDemo factory_database)
    if(MSVC)
        target_compile_options(HarmonyContinuationDemo PRIVATE /utf-8 /W4)
    endif()
endif()
add_executable(db_compiler tools/db_compiler.cpp)
target_link_libraries(db_compiler PRIVATE harmony_library)
add_executable(recommend_cli tools/recommend_cli.cpp)
target_link_libraries(recommend_cli PRIVATE harmony_library)
add_executable(recommend_bench tools/recommend_bench.cpp)
target_link_libraries(recommend_bench PRIVATE harmony_library)
file(GLOB HC_FACTORY_JSON CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/data/factory/*.json")
set(HC_FACTORY_DB "${CMAKE_CURRENT_BINARY_DIR}/factory.db")
add_custom_command(OUTPUT "${HC_FACTORY_DB}"
    COMMAND $<TARGET_FILE:db_compiler> "${CMAKE_CURRENT_SOURCE_DIR}/data/factory" "${HC_FACTORY_DB}"
    DEPENDS db_compiler ${HC_FACTORY_JSON}
    VERBATIM)
add_custom_target(factory_database ALL DEPENDS "${HC_FACTORY_DB}")
add_executable(preview_cli tools/preview_cli.cpp)
target_link_libraries(preview_cli PRIVATE harmony_preview harmony_demo_scenarios)
target_compile_definitions(preview_cli PRIVATE HC_FACTORY_DB_PATH="${HC_FACTORY_DB}"
    HC_DEMO_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/demo")
add_dependencies(preview_cli factory_database)
add_executable(midi_export_cli tools/midi_export_cli.cpp)
target_link_libraries(midi_export_cli PRIVATE harmony_snapshot harmony_demo_scenarios)
target_compile_definitions(midi_export_cli PRIVATE HC_FACTORY_DB_PATH="${HC_FACTORY_DB}"
    HC_DEMO_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/demo")
add_dependencies(midi_export_cli factory_database)
if(HC_BUILD_PLUGIN)
    target_link_libraries(HarmonyContinuation PRIVATE harmony_product)
    add_dependencies(HarmonyContinuation factory_database)
    add_custom_command(TARGET HarmonyContinuation POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/VST3/$<CONFIG>/HarmonyContinuation.vst3/Contents/Resources"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${HC_FACTORY_DB}"
            "${CMAKE_BINARY_DIR}/VST3/$<CONFIG>/HarmonyContinuation.vst3/Contents/Resources/factory.db")
endif()
add_executable(harmony_cli tools/harmony_cli.cpp)
target_link_libraries(harmony_cli PRIVATE harmony_dev_json)
add_executable(progression_match_cli tools/progression_match_cli.cpp)
target_link_libraries(progression_match_cli PRIVATE harmony_dev_json)
add_executable(progression_match_bench tools/progression_match_bench.cpp)
target_link_libraries(progression_match_bench PRIVATE harmony_dev_json)

if(HC_BUILD_PLUGIN)
    # Only the Cubase adapter needs the local SDK's bundled Expat reader.
    set(HC_EXPAT_SOURCE_DIR "${VST3_SDK_ROOT}/vstgui4/vstgui/thirdparty/expat")
    if(NOT EXISTS "${HC_EXPAT_SOURCE_DIR}/xmlparse.c")
        message(FATAL_ERROR "The configured local VSTGUI SDK does not provide its Expat sources.")
    endif()
    add_library(hc_embedded_expat STATIC
        "${HC_EXPAT_SOURCE_DIR}/xmlparse.c"
        "${HC_EXPAT_SOURCE_DIR}/xmlrole.c"
        "${HC_EXPAT_SOURCE_DIR}/xmltok.c")
    target_include_directories(hc_embedded_expat PUBLIC "${HC_EXPAT_SOURCE_DIR}")
    target_compile_definitions(hc_embedded_expat PRIVATE XML_NS XML_DTD XML_CONTEXT_BYTES=1024 HAVE_MEMMOVE)
    target_compile_definitions(hc_embedded_expat PUBLIC XML_STATIC)
    if(WIN32)
        target_compile_definitions(hc_embedded_expat PRIVATE COMPILED_FROM_DSP)
    endif()
    add_library(harmony_xml STATIC src/plugin/VstXmlDropAdapter.cpp src/plugin/VstXmlChordParser.cpp)
    target_link_libraries(harmony_xml PUBLIC harmony_core hc_embedded_expat)
    set_target_properties(harmony_xml PROPERTIES CXX_EXTENSIONS OFF)
    if(MSVC)
        target_compile_options(harmony_xml PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(harmony_xml PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(UILayoutTests tests/UILayoutTests.cpp)
    target_link_libraries(UILayoutTests PRIVATE harmony_product)
    add_test(NAME UILayoutTests COMMAND UILayoutTests)
    add_executable(MidiExportTests tests/MidiExportTests.cpp)
    target_link_libraries(MidiExportTests PRIVATE harmony_snapshot harmony_demo_scenarios)
    target_compile_definitions(MidiExportTests PRIVATE HC_FACTORY_DB_PATH="${HC_FACTORY_DB}"
        HC_DEMO_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/demo"
        HC_TEST_OUTPUT_DIR="${CMAKE_CURRENT_BINARY_DIR}")
    add_dependencies(MidiExportTests factory_database)
    add_test(NAME MidiExportTests COMMAND MidiExportTests)
    add_executable(PreviewTests tests/PreviewTests.cpp)
    target_link_libraries(PreviewTests PRIVATE harmony_preview harmony_product)
    add_test(NAME PreviewTests COMMAND PreviewTests)
    add_executable(ProductizationTests tests/ProductizationTests.cpp src/plugin/RecommendationWorker.cpp)
    target_link_libraries(ProductizationTests PRIVATE harmony_demo_scenarios)
    target_compile_definitions(ProductizationTests PRIVATE
        HC_FACTORY_DB_PATH="${HC_FACTORY_DB}"
        HC_DEMO_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/demo")
    add_dependencies(ProductizationTests factory_database)
    if(MSVC)
        target_compile_options(ProductizationTests PRIVATE /utf-8)
    endif()
    add_test(NAME ProductizationTests COMMAND ProductizationTests)
    add_executable(WeightBarGeometryTests tests/WeightBarGeometryTests.cpp)
    target_include_directories(WeightBarGeometryTests PRIVATE src)
    target_compile_features(WeightBarGeometryTests PRIVATE cxx_std_20)
    add_test(NAME WeightBarGeometryTests COMMAND WeightBarGeometryTests)

    add_executable(CoreTimelineTests tests/CoreTimelineTests.cpp)
    target_link_libraries(CoreTimelineTests PRIVATE harmony_core)
    add_test(NAME CoreTimelineTests COMMAND CoreTimelineTests)

    add_executable(HarmonyAnalysisTests tests/HarmonyAnalysisTests.cpp)
    target_link_libraries(HarmonyAnalysisTests PRIVATE harmony_dev_json)
    target_compile_definitions(HarmonyAnalysisTests PRIVATE HC_HARMONY_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/harmony")
    add_test(NAME HarmonyAnalysisTests COMMAND HarmonyAnalysisTests)

    add_executable(ProgressionMatcherTests tests/ProgressionMatcherTests.cpp)
    target_link_libraries(ProgressionMatcherTests PRIVATE harmony_dev_json)
    target_compile_definitions(ProgressionMatcherTests PRIVATE HC_HARMONY_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/harmony")
    add_test(NAME ProgressionMatcherTests COMMAND ProgressionMatcherTests)

    add_executable(ContinuationEngineTests tests/ContinuationEngineTests.cpp src/plugin/RecommendationWorker.cpp)
    target_link_libraries(ContinuationEngineTests PRIVATE harmony_library)
    target_compile_definitions(ContinuationEngineTests PRIVATE HC_FACTORY_DB_PATH="${HC_FACTORY_DB}")
    add_dependencies(ContinuationEngineTests factory_database)
    if(MSVC)
        target_compile_options(ContinuationEngineTests PRIVATE /utf-8)
    endif()
    add_test(NAME ContinuationEngineTests COMMAND ContinuationEngineTests)

    if(HC_BUILD_PLUGIN)
        add_executable(VstXmlParserTests tests/VstXmlParserTests.cpp)
        target_link_libraries(VstXmlParserTests PRIVATE harmony_xml)
        target_compile_definitions(VstXmlParserTests PRIVATE HC_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
        add_test(NAME VstXmlParserTests COMMAND VstXmlParserTests)

        add_executable(VstXmlChordParserTests tests/VstXmlChordParserTests.cpp)
        target_link_libraries(VstXmlChordParserTests PRIVATE harmony_xml)
        target_compile_definitions(VstXmlChordParserTests PRIVATE HC_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
        add_test(NAME VstXmlChordParserTests COMMAND VstXmlChordParserTests)
    endif()
endif()
