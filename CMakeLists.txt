cmake_minimum_required(VERSION 3.25)
project(HarmonyContinuation VERSION 0.14.10 LANGUAGES C CXX)
set(HC_PRERELEASE "" CACHE STRING "Product prerelease suffix (empty for an approved release)")
set(HC_FACTORY_LIBRARY_VERSION 8 CACHE STRING "Versioned Factory content; published V5/V7 remain frozen")
option(HC_BUILD_FACTORY_V6_EXPERIMENT "Enable the isolated Factory V6 compiler and validation targets" OFF)
set(HC_PRODUCT_VERSION "${PROJECT_VERSION}")
if(HC_PRERELEASE)
    string(APPEND HC_PRODUCT_VERSION "-${HC_PRERELEASE}")
endif()
set(HC_GIT_COMMIT_OVERRIDE "" CACHE STRING "Commit hash for restricted/offline build environments")
if(HC_GIT_COMMIT_OVERRIDE)
    set(HC_GIT_COMMIT "${HC_GIT_COMMIT_OVERRIDE}")
else()
    execute_process(COMMAND git -c "safe.directory=${CMAKE_CURRENT_SOURCE_DIR}" rev-parse --short=7 HEAD
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE HC_GIT_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
endif()
if(NOT HC_GIT_COMMIT)
    set(HC_GIT_COMMIT "unknown")
endif()
if(CMAKE_CONFIGURATION_TYPES)
    set(HC_BUILD_TYPE "MultiConfig")
elseif(CMAKE_BUILD_TYPE)
    set(HC_BUILD_TYPE "${CMAKE_BUILD_TYPE}")
else()
    set(HC_BUILD_TYPE "Debug")
endif()
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file(src/product/ProductVersionGenerated.h.in
    "${CMAKE_CURRENT_BINARY_DIR}/generated/ProductVersionGenerated.h" @ONLY)
set(HC_ZH_LOCALE "${CMAKE_CURRENT_SOURCE_DIR}/resources/locales/zh-CN.json")
set(HC_EN_LOCALE "${CMAKE_CURRENT_SOURCE_DIR}/resources/locales/en-US.json")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${HC_ZH_LOCALE}" "${HC_EN_LOCALE}")
file(READ "${HC_ZH_LOCALE}" HC_ZH_JSON)
file(READ "${HC_EN_LOCALE}" HC_EN_JSON)
string(JSON HC_LOCALE_COUNT LENGTH "${HC_ZH_JSON}")
string(JSON HC_EN_LOCALE_COUNT LENGTH "${HC_EN_JSON}")
if(NOT HC_LOCALE_COUNT EQUAL HC_EN_LOCALE_COUNT)
    message(FATAL_ERROR "zh-CN and en-US localization key counts differ")
endif()
set(HC_LOCALE_HEADER "#pragma once\n#include <array>\n#include <string_view>\nnamespace harmony::localization::generated {\nstruct Entry { std::string_view key, zhCN, enUS; };\ninline constexpr std::array<Entry, ${HC_LOCALE_COUNT}> entries{{\n")
math(EXPR HC_LOCALE_LAST "${HC_LOCALE_COUNT} - 1")
foreach(HC_LOCALE_INDEX RANGE 0 ${HC_LOCALE_LAST})
    string(JSON HC_LOCALE_KEY MEMBER "${HC_ZH_JSON}" ${HC_LOCALE_INDEX})
    string(JSON HC_ZH_VALUE GET "${HC_ZH_JSON}" "${HC_LOCALE_KEY}")
    string(JSON HC_EN_VALUE GET "${HC_EN_JSON}" "${HC_LOCALE_KEY}")
    foreach(HC_FIELD HC_LOCALE_KEY HC_ZH_VALUE HC_EN_VALUE)
        string(REPLACE "\\" "\\\\" ${HC_FIELD} "${${HC_FIELD}}")
        string(REPLACE "\"" "\\\"" ${HC_FIELD} "${${HC_FIELD}}")
    endforeach()
    string(APPEND HC_LOCALE_HEADER "    Entry{\"${HC_LOCALE_KEY}\", \"${HC_ZH_VALUE}\", \"${HC_EN_VALUE}\"},\n")
endforeach()
string(APPEND HC_LOCALE_HEADER "}};\n}\n")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/generated/LocalizationGenerated.h" "${HC_LOCALE_HEADER}")
add_library(harmony_version INTERFACE)
target_include_directories(harmony_version INTERFACE src "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_compile_features(harmony_version INTERFACE cxx_std_20)
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
set_source_files_properties(${HC_LOCAL_CPP} PROPERTIES OBJECT_DEPENDS
    "${HC_LOCAL_HEADERS};${CMAKE_CURRENT_BINARY_DIR}/generated/ProductVersionGenerated.h;${CMAKE_CURRENT_BINARY_DIR}/generated/LocalizationGenerated.h")
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

# RAII cleanup must run when C++ exceptions cross try/catch boundaries.
# Without /EHsc, failed updates can retain file locks and SQLite handles on MSVC.
if(MSVC)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/EHsc>)
endif()

if(HC_BUILD_PLUGIN)
    # The SDK's moduleinfotool reads PROJECT_VERSION. Include the prerelease
    # suffix there so the bundle header and class metadata agree.
    set(PROJECT_VERSION "${HC_PRODUCT_VERSION}")
    smtg_add_vst3plugin(HarmonyContinuation
        src/plugin/Processor.cpp src/plugin/Controller.cpp src/plugin/PluginEntry.cpp
        src/plugin/HostContextAdapter.cpp src/plugin/DropCapture.cpp src/plugin/PluginView.cpp
        src/plugin/RecommendationWorker.cpp src/plugin/HostAdapters.cpp
        src/ui/MainView.cpp src/ui/MainViewCompact.cpp src/ui/MainViewPreview.cpp src/ui/MainViewWish.cpp)
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
    src/core/ProgressionMatcher.cpp src/core/ContinuationEngine.cpp src/core/ContextualContinuation.cpp src/core/CreativeDiversity.cpp
    src/core/IntentCompletionEvaluator.cpp src/core/ChordPitchSet.cpp
    src/core/HarmonyConstraints.cpp src/core/HarmonicTendency.cpp src/preview/ChordVoicer.cpp)
# Versioned editorial supplement, bound to whole source music and timing.
set(HC_PHRASE_REVIEWS "${CMAKE_CURRENT_SOURCE_DIR}/data/phrase-flow/g12-reviews.json")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${HC_PHRASE_REVIEWS}")
file(READ "${HC_PHRASE_REVIEWS}" HC_PHRASE_JSON)
string(JSON HC_PHRASE_COUNT LENGTH "${HC_PHRASE_JSON}" reviews)
set(HC_PHRASE_HEADER "#pragma once\n#include <array>\n#include <string_view>\nnamespace harmony::phrase_review_data {\nstruct Review { std::string_view id, signature, ending, purpose, reason; bool continuingTail; double groupOneQN, groupTwoQN; };\ninline constexpr std::array<Review, ${HC_PHRASE_COUNT}> reviews{{\n")
math(EXPR HC_PHRASE_LAST "${HC_PHRASE_COUNT}-1")
foreach(HC_REVIEW_INDEX RANGE 0 ${HC_PHRASE_LAST})
    foreach(HC_REVIEW_FIELD id musicSignature ending purpose reason)
        string(JSON HC_REVIEW_${HC_REVIEW_FIELD} GET "${HC_PHRASE_JSON}" reviews ${HC_REVIEW_INDEX} ${HC_REVIEW_FIELD})
        string(REPLACE "\\" "\\\\" HC_REVIEW_${HC_REVIEW_FIELD} "${HC_REVIEW_${HC_REVIEW_FIELD}}")
        string(REPLACE "\"" "\\\"" HC_REVIEW_${HC_REVIEW_FIELD} "${HC_REVIEW_${HC_REVIEW_FIELD}}")
    endforeach()
    string(JSON HC_REVIEW_CONTINUING GET "${HC_PHRASE_JSON}" reviews ${HC_REVIEW_INDEX} continuingTail)
    set(HC_REVIEW_BOOL false)
    if(HC_REVIEW_CONTINUING)
        set(HC_REVIEW_BOOL true)
    endif()
    string(JSON HC_REVIEW_GROUPS LENGTH "${HC_PHRASE_JSON}" reviews ${HC_REVIEW_INDEX} groupingQN)
    set(HC_REVIEW_G1 0)
    set(HC_REVIEW_G2 0)
    if(HC_REVIEW_GROUPS EQUAL 2)
        string(JSON HC_REVIEW_G1 GET "${HC_PHRASE_JSON}" reviews ${HC_REVIEW_INDEX} groupingQN 0)
        string(JSON HC_REVIEW_G2 GET "${HC_PHRASE_JSON}" reviews ${HC_REVIEW_INDEX} groupingQN 1)
    endif()
    string(APPEND HC_PHRASE_HEADER "    Review{\"${HC_REVIEW_id}\", \"${HC_REVIEW_musicSignature}\", \"${HC_REVIEW_ending}\", \"${HC_REVIEW_purpose}\", \"${HC_REVIEW_reason}\", ${HC_REVIEW_BOOL}, ${HC_REVIEW_G1}, ${HC_REVIEW_G2}},\n")
endforeach()
string(APPEND HC_PHRASE_HEADER "}};\n}\n")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/generated/PhraseReviewGenerated.h" "${HC_PHRASE_HEADER}")
target_link_libraries(harmony_core PUBLIC harmony_version)
target_include_directories(harmony_core PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_include_directories(harmony_core PUBLIC src)
target_compile_features(harmony_core PUBLIC cxx_std_20)
add_library(harmony_color STATIC src/color/HarmonyColorAnalyzer.cpp src/color/ColorPreferenceReranker.cpp
    src/color/RelativeColorCurve.cpp)
target_link_libraries(harmony_color PUBLIC harmony_core)
target_compile_features(harmony_color PUBLIC cxx_std_20)
option(HC_BUILD_GENERATION_EXPERIMENT "Build the isolated G13 research CLI and checks" OFF)
# The product generation library is always available to the background worker.
    add_library(harmony_generation STATIC src/generation/PhraseGenerator.cpp)
    target_include_directories(harmony_generation PUBLIC src)
    target_compile_features(harmony_generation PUBLIC cxx_std_20)
    target_link_libraries(harmony_generation PUBLIC harmony_color harmony_preview)
    # Product builds embed only the separately reviewed CC0 aggregate. Research
    # priors remain external inputs to the isolated CLI, never product options.
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/data/generation-priors/billboard-cc0.tsv" HC_GENERATION_PRIOR_TEXT)
    # MSVC rejects a single narrow string literal above roughly 65,535 bytes
    # (error C2026), and the reviewed CC0 prior is larger than that. Emit it as
    # adjacent raw literals: the compiler concatenates them into the identical
    # byte sequence at translation time, so the parsed prior is unchanged.
    string(LENGTH "${HC_GENERATION_PRIOR_TEXT}" HC_PRIOR_CHARS)
    set(HC_PRIOR_LITERALS "")
    set(HC_PRIOR_OFFSET 0)
    set(HC_PRIOR_CHUNK_CHARS 8000)
    while(HC_PRIOR_OFFSET LESS HC_PRIOR_CHARS)
        string(SUBSTRING "${HC_GENERATION_PRIOR_TEXT}" ${HC_PRIOR_OFFSET} ${HC_PRIOR_CHUNK_CHARS} HC_PRIOR_PIECE)
        string(APPEND HC_PRIOR_LITERALS "R\"HC_PRIOR(${HC_PRIOR_PIECE})HC_PRIOR\"\n")
        math(EXPR HC_PRIOR_OFFSET "${HC_PRIOR_OFFSET} + ${HC_PRIOR_CHUNK_CHARS}")
    endwhile()
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/generated/GenerationPrior.h" "#pragma once\ninline constexpr const char* hcGenerationPrior=\n${HC_PRIOR_LITERALS};\n")
    target_sources(harmony_generation PRIVATE src/generation/ProductGeneration.cpp)
    target_include_directories(harmony_generation PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")

    if(MSVC)
        target_compile_options(harmony_generation PRIVATE /utf-8 /W4)
    endif()
if(MSVC)
    target_compile_options(harmony_color PRIVATE /utf-8 /W4)
endif()
add_library(harmony_enrichment STATIC src/enrichment/ProgressionEnrichmentEngine.cpp)
target_link_libraries(harmony_enrichment PUBLIC harmony_core)
target_include_directories(harmony_enrichment PUBLIC src)
target_compile_features(harmony_enrichment PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(harmony_enrichment PRIVATE /utf-8 /W4)
endif()
if(MSVC)
    target_compile_options(harmony_core PRIVATE /W4 /permissive- /utf-8)
endif()

set(HC_DEVELOPMENT_TEMPLATES "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/progressions/templates.json")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${HC_DEVELOPMENT_TEMPLATES}")
file(READ "${HC_DEVELOPMENT_TEMPLATES}" HC_EMBEDDED_TEMPLATES)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file(tools/EmbeddedTemplates.h.in "${CMAKE_CURRENT_BINARY_DIR}/generated/EmbeddedTemplates.h" @ONLY)
set_property(SOURCE tools/DevelopmentTemplates.cpp APPEND PROPERTY OBJECT_DEPENDS
    "${CMAKE_CURRENT_BINARY_DIR}/generated/EmbeddedTemplates.h")
add_library(harmony_dev_json STATIC tools/ProgressionJson.cpp tools/TemplateJson.cpp src/persistence/ChordData.cpp)
target_link_libraries(harmony_dev_json PUBLIC harmony_core)
target_compile_features(harmony_dev_json PUBLIC cxx_std_20)
target_include_directories(harmony_dev_json PUBLIC tools "${CMAKE_CURRENT_BINARY_DIR}/generated")
add_library(harmony_dev_fixtures STATIC tools/DevelopmentTemplates.cpp)
target_link_libraries(harmony_dev_fixtures PUBLIC harmony_dev_json)
add_library(harmony_library STATIC src/library/ProgressionLibrary.cpp src/library/LibraryStore.cpp src/library/LegacyFactoryIdResolver.cpp)
target_include_directories(harmony_library PUBLIC src)
target_link_libraries(harmony_library PUBLIC harmony_dev_json)
target_compile_features(harmony_library PUBLIC cxx_std_20)
if(WIN32)
    target_link_libraries(harmony_library PUBLIC winsqlite3 shell32 ole32)
else()
    find_package(SQLite3 REQUIRED)
    target_link_libraries(harmony_library PUBLIC SQLite::SQLite3)
endif()
add_library(harmony_product STATIC src/session/PluginSessionState.cpp src/session/ProductServices.cpp)
target_link_libraries(harmony_product PUBLIC harmony_library harmony_generation)
target_include_directories(harmony_product PUBLIC src)
target_compile_features(harmony_product PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(harmony_product PRIVATE /W4 /permissive- /utf-8)
endif()
add_library(harmony_preview STATIC src/preview/PreviewSequence.cpp
    src/preview/PreviewSynth.cpp src/preview/OfflinePreviewRenderer.cpp
    src/preview/VoiceLeadingMetrics.cpp src/preview/PreviewTransport.cpp src/preview/WindowsPreview.cpp)
if(WIN32)
    target_link_libraries(harmony_preview PUBLIC winmm)
endif()
target_link_libraries(harmony_preview PUBLIC harmony_core)
target_include_directories(harmony_preview PUBLIC src)
target_compile_features(harmony_preview PUBLIC cxx_std_20)

# Embed the reviewed sidecar; installed plugins must not depend on a source checkout path.
set(HC_WISH_CATALOG "${CMAKE_CURRENT_SOURCE_DIR}/data/mode-annotations/v2/factory-v5.hca")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${HC_WISH_CATALOG}")
file(STRINGS "${HC_WISH_CATALOG}" HC_WISH_LINES ENCODING UTF-8)
set(HC_WISH_HEADER "#pragma once\nnamespace harmony::wish::generated {\ninline constexpr const char* catalogLines[] = {\n")
foreach(HC_WISH_LINE IN LISTS HC_WISH_LINES)
    string(APPEND HC_WISH_HEADER "R\"HCANN(${HC_WISH_LINE}\n)HCANN\",\n")
endforeach()
string(APPEND HC_WISH_HEADER "};\n}\n")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/generated/WishCatalogData.h" "${HC_WISH_HEADER}")
add_library(harmony_wish STATIC src/wish/MusicIdentity.cpp src/wish/ModeAnnotations.cpp src/wish/WishSampler.cpp
    src/wish/WishCatalog.cpp src/wish/WishWorkflow.cpp)
target_link_libraries(harmony_wish PUBLIC harmony_library harmony_preview harmony_color)
target_include_directories(harmony_wish PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_compile_features(harmony_wish PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(harmony_wish PRIVATE /W4 /permissive- /utf-8)
endif()
target_link_libraries(harmony_enrichment PUBLIC harmony_preview)
target_link_libraries(harmony_color PUBLIC harmony_preview)
if(MSVC)
    target_compile_options(harmony_preview PRIVATE /W4 /permissive- /utf-8)
endif()
add_library(harmony_midi STATIC src/midi/MidiClip.cpp src/midi/StandardMidiFileWriter.cpp
    src/midi/StandardMidiFileReader.cpp src/midi/MidiHarmonyExtractor.cpp src/midi/MidiWorkflow.cpp
    src/midi/MidiImportWorkflow.cpp)
target_link_libraries(harmony_midi PUBLIC harmony_preview)
target_include_directories(harmony_midi PUBLIC src)
target_link_libraries(harmony_midi PUBLIC harmony_enrichment)
if(WIN32)
    target_link_libraries(harmony_midi PUBLIC comdlg32)
endif()
target_compile_features(harmony_midi PUBLIC cxx_std_20)
add_library(harmony_snapshot STATIC src/snapshot/RecommendationSnapshot.cpp src/snapshot/EnrichmentSnapshot.cpp)
target_link_libraries(harmony_snapshot PUBLIC harmony_midi harmony_enrichment harmony_library)
if(HC_BUILD_GENERATION_EXPERIMENT)
    add_executable(phrase_generate tools/phrase_generate.cpp)
    target_include_directories(phrase_generate PRIVATE tools)
    target_link_libraries(phrase_generate PRIVATE harmony_generation harmony_midi harmony_dev_json)
    add_executable(PhraseGenerationTests EXCLUDE_FROM_ALL tests/PhraseGenerationTests.cpp)
    target_include_directories(PhraseGenerationTests PRIVATE tools)
    target_link_libraries(PhraseGenerationTests PRIVATE harmony_generation harmony_midi harmony_library harmony_dev_json)
    if(MSVC)
        target_compile_options(phrase_generate PRIVATE /utf-8 /W4)
        target_compile_options(PhraseGenerationTests PRIVATE /utf-8 /W4)
    endif()
endif()
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
# Removed 2026-11-15: one-off constraint/tendency harnesses and the frozen benchmark
# baseline family (harmony_benchmark, its two CLIs, data/benchmark cases). They only
# compared the engine against hand-written expectations of earlier stages.
add_executable(db_compiler tools/db_compiler.cpp)
target_link_libraries(db_compiler PRIVATE harmony_library)
add_executable(library_manager tools/library_manager.cpp)
target_link_libraries(library_manager PRIVATE harmony_library)
if(MSVC)
    target_compile_options(library_manager PRIVATE /utf-8)
endif()
add_executable(attach_bench tools/attach_bench.cpp)
target_link_libraries(attach_bench PRIVATE harmony_wish harmony_dev_json psapi)
if(MSVC)
    target_compile_options(attach_bench PRIVATE /utf-8)
endif()
add_executable(recommend_cli tools/recommend_cli.cpp)
target_link_libraries(recommend_cli PRIVATE harmony_library)
file(GLOB HC_FACTORY_JSON CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/data/factory/*.json")
file(GLOB HC_FACTORY_V3_JSON CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/data/factory-v3/*.json")
set(HC_FACTORY_DB "${CMAKE_CURRENT_BINARY_DIR}/factory.db")
set(HC_FACTORY_ADDITIONS)
if(HC_FACTORY_LIBRARY_VERSION STREQUAL "3")
    list(APPEND HC_FACTORY_ADDITIONS "${CMAKE_CURRENT_SOURCE_DIR}/data/factory-v3")
endif()
if(HC_FACTORY_LIBRARY_VERSION GREATER_EQUAL 9)
    # Factory V9 and later are compiled outside CMake by tools/ingest (RS200 + Billboard 2.0
    # real-corpus material), so the build stages an existing database instead of
    # recompiling authored JSON. The annotation catalog is not extended for the
    # new entries; the V8 file is carried forward so authored memberships for the
    # legacy ids stay bound.
    set(HC_FACTORY_PREBUILT_DB "" CACHE FILEPATH "Prebuilt Factory database used when HC_FACTORY_LIBRARY_VERSION is 9 or later")
    set(HC_FACTORY_PREBUILT_HCA "" CACHE FILEPATH "Prebuilt Factory annotation file used when HC_FACTORY_LIBRARY_VERSION is 9 or later")
    set(HC_FACTORY_RUNTIME_HCA "${CMAKE_CURRENT_BINARY_DIR}/factory.hca")
    target_compile_definitions(harmony_library PRIVATE HC_BUNDLED_FACTORY_VERSION=${HC_FACTORY_LIBRARY_VERSION})
    if(NOT HC_FACTORY_PREBUILT_DB OR NOT EXISTS "${HC_FACTORY_PREBUILT_DB}")
        message(FATAL_ERROR "HC_FACTORY_LIBRARY_VERSION ${HC_FACTORY_LIBRARY_VERSION} requires -DHC_FACTORY_PREBUILT_DB=<compiled factory database>")
    endif()
    if(NOT HC_FACTORY_PREBUILT_HCA OR NOT EXISTS "${HC_FACTORY_PREBUILT_HCA}")
        message(FATAL_ERROR "HC_FACTORY_LIBRARY_VERSION ${HC_FACTORY_LIBRARY_VERSION} requires -DHC_FACTORY_PREBUILT_HCA=<factory annotation file>")
    endif()
    add_custom_command(OUTPUT "${HC_FACTORY_DB}" "${HC_FACTORY_RUNTIME_HCA}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${HC_FACTORY_PREBUILT_DB}" "${HC_FACTORY_DB}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${HC_FACTORY_PREBUILT_HCA}" "${HC_FACTORY_RUNTIME_HCA}"
        DEPENDS "${HC_FACTORY_PREBUILT_DB}" "${HC_FACTORY_PREBUILT_HCA}"
        COMMENT "Stage the prebuilt Factory V9 database and annotation catalog"
        VERBATIM)
    add_custom_target(factory_database ALL DEPENDS "${HC_FACTORY_DB}" "${HC_FACTORY_RUNTIME_HCA}")
elseif(HC_FACTORY_LIBRARY_VERSION GREATER_EQUAL 4 AND HC_FACTORY_LIBRARY_VERSION LESS 9)
    target_compile_definitions(harmony_library PRIVATE HC_BUNDLED_FACTORY_VERSION=${HC_FACTORY_LIBRARY_VERSION})
    add_executable(factory_v4_compiler tools/factory_v4_compiler.cpp)
    target_link_libraries(factory_v4_compiler PRIVATE harmony_library)
    if(MSVC)
        target_compile_options(factory_v4_compiler PRIVATE /utf-8 /W4)
    endif()
    file(GLOB HC_FACTORY_V4_JSON CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/data/factory-v4/*.json")
    set(HC_FACTORY_NEXT_ARGS)
    set(HC_FACTORY_COMPILED_DB "${HC_FACTORY_DB}")
    if(HC_FACTORY_LIBRARY_VERSION STREQUAL "6" OR HC_FACTORY_LIBRARY_VERSION STREQUAL "7" OR HC_FACTORY_LIBRARY_VERSION STREQUAL "8")
        set(HC_FACTORY_COMPILED_DB "${CMAKE_CURRENT_BINARY_DIR}/factory-v5.db")
    endif()
    if(HC_FACTORY_LIBRARY_VERSION STREQUAL "5" OR HC_FACTORY_LIBRARY_VERSION STREQUAL "6" OR HC_FACTORY_LIBRARY_VERSION STREQUAL "7" OR HC_FACTORY_LIBRARY_VERSION STREQUAL "8")
        file(GLOB HC_FACTORY_V5_JSON CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/data/factory-proposals/20261019-pop-variants/*.json")
        list(APPEND HC_FACTORY_NEXT_ARGS "${CMAKE_CURRENT_SOURCE_DIR}/data/factory-proposals/20261019-pop-variants" 5)
    endif()
    add_custom_command(OUTPUT "${HC_FACTORY_COMPILED_DB}" "${CMAKE_CURRENT_BINARY_DIR}/factory-v4-maintenance.tsv"
        COMMAND $<TARGET_FILE:factory_v4_compiler> "${CMAKE_CURRENT_SOURCE_DIR}/data/factory"
            "${CMAKE_CURRENT_SOURCE_DIR}/data/factory-v3" "${CMAKE_CURRENT_SOURCE_DIR}/data/factory-v4"
            "${HC_FACTORY_COMPILED_DB}" "${CMAKE_CURRENT_BINARY_DIR}/factory-v4-maintenance.tsv" ${HC_FACTORY_NEXT_ARGS}
        DEPENDS factory_v4_compiler ${HC_FACTORY_JSON} ${HC_FACTORY_V3_JSON} ${HC_FACTORY_V4_JSON} ${HC_FACTORY_V5_JSON}
        VERBATIM)
else()
add_custom_command(OUTPUT "${HC_FACTORY_DB}"
    COMMAND $<TARGET_FILE:db_compiler> "${CMAKE_CURRENT_SOURCE_DIR}/data/factory" "${HC_FACTORY_DB}" "${HC_FACTORY_LIBRARY_VERSION}" ${HC_FACTORY_ADDITIONS}
    DEPENDS db_compiler ${HC_FACTORY_JSON} ${HC_FACTORY_V3_JSON}
    VERBATIM)
endif()
if(HC_FACTORY_LIBRARY_VERSION LESS 9 AND NOT TARGET factory_database)
    add_custom_target(factory_database ALL DEPENDS "${HC_FACTORY_DB}")
endif()
if(HC_FACTORY_LIBRARY_VERSION STREQUAL "6" OR HC_FACTORY_LIBRARY_VERSION STREQUAL "7" OR HC_FACTORY_LIBRARY_VERSION STREQUAL "8" OR (HC_BUILD_FACTORY_V6_EXPERIMENT AND HC_FACTORY_LIBRARY_VERSION STREQUAL "5"))
    set(HC_FACTORY_V6_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/data/factory-v6")
    set(HC_FACTORY_V6_CANDIDATES "${HC_FACTORY_V6_SOURCE_DIR}/candidates.json")
    set(HC_FACTORY_V6_METADATA "${HC_FACTORY_V6_SOURCE_DIR}/metadata.json")
    set(HC_FACTORY_V6_APPROVED_IDS "${HC_FACTORY_V6_SOURCE_DIR}/approved-ids.txt")
    set(HC_FACTORY_V6_INCREMENT_DIR "${HC_FACTORY_V6_SOURCE_DIR}/increments/20261025-empty-modes")
    set(HC_FACTORY_V6_BASE_HCA "${CMAKE_CURRENT_SOURCE_DIR}/data/mode-annotations/v2/factory-v5.hca")
    set(HC_FACTORY_V6_PROPOSAL_DB "${CMAKE_CURRENT_BINARY_DIR}/factory-v6-proposal.db")
    set(HC_FACTORY_V6_EXPERIMENT_DB "${CMAKE_CURRENT_BINARY_DIR}/factory-v6-experiment.db")
    set(HC_FACTORY_V6_HCA "${CMAKE_CURRENT_BINARY_DIR}/factory-v6-initial.hca")
    set(HC_FACTORY_V6_BASE_DB "${HC_FACTORY_DB}")
    if(HC_FACTORY_LIBRARY_VERSION STREQUAL "6" OR HC_FACTORY_LIBRARY_VERSION STREQUAL "7" OR HC_FACTORY_LIBRARY_VERSION STREQUAL "8")
        set(HC_FACTORY_V6_BASE_DB "${HC_FACTORY_COMPILED_DB}")
    endif()
    if(HC_FACTORY_LIBRARY_VERSION STREQUAL "6")
        set(HC_FACTORY_V6_EXPERIMENT_DB "${HC_FACTORY_DB}")
        set(HC_FACTORY_V6_HCA "${CMAKE_CURRENT_BINARY_DIR}/factory.hca")
        set(HC_FACTORY_RUNTIME_HCA "${HC_FACTORY_V6_HCA}")
    elseif(HC_FACTORY_LIBRARY_VERSION STREQUAL "7" OR HC_FACTORY_LIBRARY_VERSION STREQUAL "8")
        set(HC_FACTORY_V6_EXPERIMENT_DB "${CMAKE_CURRENT_BINARY_DIR}/factory-v6.db")
        set(HC_FACTORY_V6_HCA "${CMAKE_CURRENT_BINARY_DIR}/factory-v6.hca")
    endif()
    set(HC_FACTORY_V6_MEDIA_DIR "${CMAKE_CURRENT_BINARY_DIR}/factory-v6-media")
    set(HC_FACTORY_V6_SUMMARY "${CMAKE_CURRENT_BINARY_DIR}/factory-v6-build-summary.txt")
    add_executable(factory_v6_compiler tools/factory_v6_compiler.cpp)
    target_link_libraries(factory_v6_compiler PRIVATE harmony_library harmony_wish)
    if(MSVC)
        target_compile_options(factory_v6_compiler PRIVATE /utf-8 /W4 /permissive-)
    endif()
    add_custom_command(OUTPUT "${HC_FACTORY_V6_EXPERIMENT_DB}" "${HC_FACTORY_V6_HCA}" "${HC_FACTORY_V6_PROPOSAL_DB}" "${HC_FACTORY_V6_SUMMARY}"
        COMMAND $<TARGET_FILE:factory_v6_compiler> "${HC_FACTORY_V6_BASE_DB}" "${HC_FACTORY_V6_CANDIDATES}"
            "${HC_FACTORY_V6_METADATA}" "${HC_FACTORY_V6_APPROVED_IDS}" "${HC_FACTORY_V6_PROPOSAL_DB}"
            "${HC_FACTORY_V6_EXPERIMENT_DB}" "${HC_FACTORY_V6_BASE_HCA}" "${HC_FACTORY_V6_HCA}"
            "${HC_FACTORY_V6_SUMMARY}" "${HC_FACTORY_V6_INCREMENT_DIR}/candidates.json"
            "${HC_FACTORY_V6_INCREMENT_DIR}/metadata.json"
        DEPENDS factory_v6_compiler "${HC_FACTORY_V6_BASE_DB}" "${HC_FACTORY_V6_CANDIDATES}"
            "${HC_FACTORY_V6_METADATA}" "${HC_FACTORY_V6_APPROVED_IDS}" "${HC_FACTORY_V6_BASE_HCA}"
            "${HC_FACTORY_V6_INCREMENT_DIR}/candidates.json" "${HC_FACTORY_V6_INCREMENT_DIR}/metadata.json"
        COMMENT "Compile reviewed Factory V6 database and matching annotations"
        VERBATIM)
    add_custom_target(factory_v6_database DEPENDS "${HC_FACTORY_V6_EXPERIMENT_DB}" "${HC_FACTORY_V6_HCA}")
    if(HC_FACTORY_LIBRARY_VERSION STREQUAL "6" AND NOT TARGET factory_database)
        add_custom_target(factory_database ALL DEPENDS "${HC_FACTORY_DB}" "${HC_FACTORY_V6_HCA}")
    elseif(HC_FACTORY_LIBRARY_VERSION STREQUAL "7" OR HC_FACTORY_LIBRARY_VERSION STREQUAL "8")
        set(HC_FACTORY_V7_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/data/factory-v7")
        set(HC_FACTORY_V7_CANDIDATES "${HC_FACTORY_V7_SOURCE_DIR}/candidates.json")
        set(HC_FACTORY_V7_METADATA "${HC_FACTORY_V7_SOURCE_DIR}/metadata.json")
        set(HC_FACTORY_V7_APPROVED_IDS "${HC_FACTORY_V7_SOURCE_DIR}/approved-ids.txt")
        set(HC_FACTORY_V7_PROPOSAL_DB "${CMAKE_CURRENT_BINARY_DIR}/factory-v7-proposal.db")
        set(HC_FACTORY_V7_HCA "${CMAKE_CURRENT_BINARY_DIR}/factory.hca")
        set(HC_FACTORY_V7_COMPILED_DB "${HC_FACTORY_DB}")
        if(HC_FACTORY_LIBRARY_VERSION STREQUAL "8")
            set(HC_FACTORY_V7_COMPILED_DB "${CMAKE_CURRENT_BINARY_DIR}/factory-v7.db")
            set(HC_FACTORY_V7_HCA "${CMAKE_CURRENT_BINARY_DIR}/factory-v7.hca")
        endif()
        set(HC_FACTORY_RUNTIME_HCA "${HC_FACTORY_V7_HCA}")
        set(HC_FACTORY_V7_SUMMARY "${CMAKE_CURRENT_BINARY_DIR}/factory-v7-build-summary.txt")
        add_custom_command(OUTPUT "${HC_FACTORY_V7_COMPILED_DB}" "${HC_FACTORY_V7_PROPOSAL_DB}" "${HC_FACTORY_V7_HCA}" "${HC_FACTORY_V7_SUMMARY}"
            COMMAND $<TARGET_FILE:factory_v6_compiler> --v7-incremental
                "${HC_FACTORY_V6_EXPERIMENT_DB}" "${HC_FACTORY_V7_CANDIDATES}" "${HC_FACTORY_V7_METADATA}"
                "${HC_FACTORY_V7_APPROVED_IDS}" "${HC_FACTORY_V7_PROPOSAL_DB}" "${HC_FACTORY_V7_COMPILED_DB}"
                "${HC_FACTORY_V6_HCA}" "${HC_FACTORY_V7_HCA}" "${HC_FACTORY_V7_SUMMARY}"
            DEPENDS factory_v6_compiler "${HC_FACTORY_V6_EXPERIMENT_DB}" "${HC_FACTORY_V6_HCA}"
                "${HC_FACTORY_V7_CANDIDATES}" "${HC_FACTORY_V7_METADATA}" "${HC_FACTORY_V7_APPROVED_IDS}"
            COMMENT "Compile reviewed Factory V7 database and matching annotations from the V6 source chain"
            VERBATIM)
        add_custom_target(factory_v7_database DEPENDS "${HC_FACTORY_V7_COMPILED_DB}" "${HC_FACTORY_V7_PROPOSAL_DB}"
            "${HC_FACTORY_V7_HCA}" "${HC_FACTORY_V7_SUMMARY}")
        if(HC_FACTORY_LIBRARY_VERSION STREQUAL "8")
            set(HC_FACTORY_V8_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/data/factory-v8")
            set(HC_FACTORY_RUNTIME_HCA "${CMAKE_CURRENT_BINARY_DIR}/factory.hca")
            add_custom_command(OUTPUT "${HC_FACTORY_DB}" "${HC_FACTORY_RUNTIME_HCA}"
                COMMAND $<TARGET_FILE:factory_v6_compiler> --v8-incremental
                    "${HC_FACTORY_V7_COMPILED_DB}" "${HC_FACTORY_V8_SOURCE_DIR}/candidates.json"
                    "${HC_FACTORY_V8_SOURCE_DIR}/metadata.json" "${HC_FACTORY_V8_SOURCE_DIR}/approved-ids.txt" "${HC_FACTORY_DB}"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${HC_FACTORY_V7_HCA}" "${HC_FACTORY_RUNTIME_HCA}"
                DEPENDS factory_v6_compiler "${HC_FACTORY_V7_COMPILED_DB}" "${HC_FACTORY_V7_HCA}"
                    "${HC_FACTORY_V8_SOURCE_DIR}/candidates.json" "${HC_FACTORY_V8_SOURCE_DIR}/metadata.json"
                    "${HC_FACTORY_V8_SOURCE_DIR}/approved-ids.txt"
                COMMENT "Compile complete reviewed open-phrase increment; preserve V7 and existing mode annotations"
                VERBATIM)
            if(NOT TARGET factory_database)
                add_custom_target(factory_database ALL DEPENDS "${HC_FACTORY_DB}" "${HC_FACTORY_RUNTIME_HCA}")
            endif()
        else()
            if(NOT TARGET factory_database)
                add_custom_target(factory_database ALL DEPENDS "${HC_FACTORY_DB}" "${HC_FACTORY_V7_HCA}")
            endif()
        endif()
        add_dependencies(factory_database factory_v7_database)
    endif()
elseif(HC_BUILD_FACTORY_V6_EXPERIMENT)
    message(FATAL_ERROR "The isolated Factory V6 targets require HC_FACTORY_LIBRARY_VERSION=5")
endif()
set(HC_FACTORY_LEGACY_DB "${CMAKE_CURRENT_BINARY_DIR}/factory-v2.db")
add_custom_command(OUTPUT "${HC_FACTORY_LEGACY_DB}"
    COMMAND $<TARGET_FILE:db_compiler> "${CMAKE_CURRENT_SOURCE_DIR}/data/factory" "${HC_FACTORY_LEGACY_DB}" 2
    DEPENDS db_compiler ${HC_FACTORY_JSON}
    VERBATIM)
add_custom_target(factory_legacy_database DEPENDS "${HC_FACTORY_LEGACY_DB}")
if(HC_FACTORY_LIBRARY_VERSION GREATER_EQUAL 4 AND HC_FACTORY_LIBRARY_VERSION LESS 9)
    set(HC_FACTORY_V3_REFERENCE_DB "${CMAKE_CURRENT_BINARY_DIR}/factory-v3.db")
    add_custom_command(OUTPUT "${HC_FACTORY_V3_REFERENCE_DB}"
        COMMAND $<TARGET_FILE:db_compiler> "${CMAKE_CURRENT_SOURCE_DIR}/data/factory"
            "${HC_FACTORY_V3_REFERENCE_DB}" 3 "${CMAKE_CURRENT_SOURCE_DIR}/data/factory-v3"
        DEPENDS db_compiler ${HC_FACTORY_JSON} ${HC_FACTORY_V3_JSON}
        VERBATIM)
    add_custom_target(factory_reference_databases DEPENDS "${HC_FACTORY_LEGACY_DB}" "${HC_FACTORY_V3_REFERENCE_DB}")
    if(HC_FACTORY_LIBRARY_VERSION STREQUAL "5" OR HC_FACTORY_LIBRARY_VERSION STREQUAL "6" OR HC_FACTORY_LIBRARY_VERSION STREQUAL "7" OR HC_FACTORY_LIBRARY_VERSION STREQUAL "8")
        set(HC_FACTORY_V4_REFERENCE_DB "${CMAKE_CURRENT_BINARY_DIR}/factory-v4.db")
        add_custom_command(OUTPUT "${HC_FACTORY_V4_REFERENCE_DB}"
            COMMAND $<TARGET_FILE:factory_v4_compiler> "${CMAKE_CURRENT_SOURCE_DIR}/data/factory"
                "${CMAKE_CURRENT_SOURCE_DIR}/data/factory-v3" "${CMAKE_CURRENT_SOURCE_DIR}/data/factory-v4"
                "${HC_FACTORY_V4_REFERENCE_DB}" "${CMAKE_CURRENT_BINARY_DIR}/factory-v4-reference-maintenance.tsv"
            DEPENDS factory_v4_compiler ${HC_FACTORY_JSON} ${HC_FACTORY_V3_JSON} ${HC_FACTORY_V4_JSON}
            VERBATIM)
        add_custom_target(factory_v4_reference DEPENDS "${HC_FACTORY_V4_REFERENCE_DB}")
        add_dependencies(factory_reference_databases factory_v4_reference)
        if(HC_FACTORY_LIBRARY_VERSION STREQUAL "6" OR HC_FACTORY_LIBRARY_VERSION STREQUAL "7" OR HC_FACTORY_LIBRARY_VERSION STREQUAL "8")
            add_custom_target(factory_v5_reference DEPENDS "${HC_FACTORY_COMPILED_DB}")
            add_dependencies(factory_reference_databases factory_v5_reference)
        endif()
        if(HC_FACTORY_LIBRARY_VERSION STREQUAL "7" OR HC_FACTORY_LIBRARY_VERSION STREQUAL "8")
            add_custom_target(factory_v6_reference DEPENDS "${HC_FACTORY_V6_EXPERIMENT_DB}")
            add_dependencies(factory_reference_databases factory_v6_reference)
        endif()
        if(HC_FACTORY_LIBRARY_VERSION STREQUAL "8")
            add_dependencies(factory_reference_databases factory_v7_database)
        endif()
    endif()
    add_dependencies(library_manager factory_reference_databases)
endif()
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
    target_link_libraries(HarmonyContinuation PRIVATE harmony_product harmony_enrichment harmony_snapshot harmony_host harmony_io harmony_color harmony_wish)
    if(WIN32)
        target_link_libraries(HarmonyContinuation PRIVATE winmm comdlg32)
    endif()
    add_dependencies(HarmonyContinuation factory_database)
    add_custom_command(TARGET HarmonyContinuation POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/VST3/$<CONFIG>/HarmonyContinuation.vst3/Contents/Resources"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${HC_FACTORY_DB}"
            "${CMAKE_BINARY_DIR}/VST3/$<CONFIG>/HarmonyContinuation.vst3/Contents/Resources/factory.db"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${HC_ZH_LOCALE}" "${HC_EN_LOCALE}"
            "${CMAKE_BINARY_DIR}/VST3/$<CONFIG>/HarmonyContinuation.vst3/Contents/Resources")
    if(HC_FACTORY_LIBRARY_VERSION GREATER_EQUAL 6)
        add_custom_command(TARGET HarmonyContinuation POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${HC_FACTORY_RUNTIME_HCA}"
                "${CMAKE_BINARY_DIR}/VST3/$<CONFIG>/HarmonyContinuation.vst3/Contents/Resources/factory.hca")
    endif()
endif()
add_executable(harmony_cli tools/harmony_cli.cpp)
target_link_libraries(harmony_cli PRIVATE harmony_dev_json)
add_executable(progression_match_cli tools/progression_match_cli.cpp)
target_link_libraries(progression_match_cli PRIVATE harmony_dev_json)

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
    if(HC_FACTORY_LIBRARY_VERSION STREQUAL "4" OR HC_FACTORY_LIBRARY_VERSION STREQUAL "5")
        add_executable(FactoryV4Tests tests/FactoryV4Tests.cpp)
        target_link_libraries(FactoryV4Tests PRIVATE harmony_product harmony_midi harmony_snapshot)
        target_compile_definitions(FactoryV4Tests PRIVATE HC_FACTORY_DB_PATH="${HC_FACTORY_DB}" HC_FACTORY_V4_PATH="${CMAKE_CURRENT_BINARY_DIR}/factory-v4.db")
        add_dependencies(FactoryV4Tests factory_database factory_reference_databases)
        if(MSVC)
            target_compile_options(FactoryV4Tests PRIVATE /utf-8 /W4)
        endif()
        add_test(NAME FactoryV${HC_FACTORY_LIBRARY_VERSION}Focused COMMAND FactoryV4Tests)
    endif()
    if(HC_FACTORY_LIBRARY_VERSION STREQUAL "3")
    add_executable(FactoryCatalogTests tests/FactoryCatalogTests.cpp)
    target_link_libraries(FactoryCatalogTests PRIVATE harmony_library harmony_midi)
    target_compile_definitions(FactoryCatalogTests PRIVATE
        HC_FACTORY_DB_PATH="${HC_FACTORY_DB}" HC_FACTORY_V2_PATH="${HC_FACTORY_LEGACY_DB}")
    add_dependencies(FactoryCatalogTests factory_database factory_legacy_database)
    add_test(NAME FactoryV3Catalog COMMAND FactoryCatalogTests)
    endif()
    add_executable(LibraryCompatibilityTests tests/LibraryCompatibilityTests.cpp)
    target_link_libraries(LibraryCompatibilityTests PRIVATE harmony_snapshot harmony_product)
    target_compile_definitions(LibraryCompatibilityTests PRIVATE HC_FACTORY_SOURCE="${CMAKE_CURRENT_SOURCE_DIR}/data/factory")
    if(MSVC)
        target_compile_options(LibraryCompatibilityTests PRIVATE /utf-8 /W4)
    endif()
    foreach(HC_BRIDGE_GROUP resolver snapshot user factory)
        add_test(NAME LibraryV3_${HC_BRIDGE_GROUP} COMMAND LibraryCompatibilityTests ${HC_BRIDGE_GROUP})
    endforeach()
    add_executable(LibraryStoreTests tests/LibraryStoreTests.cpp)
    target_link_libraries(LibraryStoreTests PRIVATE harmony_library)
    target_compile_definitions(LibraryStoreTests PRIVATE HC_FACTORY_DB_PATH="${HC_FACTORY_LEGACY_DB}")
    add_dependencies(LibraryStoreTests factory_legacy_database)
    add_test(NAME LibraryStoreTests COMMAND LibraryStoreTests)
    add_executable(UILayoutTests tests/UILayoutTests.cpp)
    target_link_libraries(UILayoutTests PRIVATE harmony_product)
    add_test(NAME UILayoutTests COMMAND UILayoutTests)
    add_executable(IntentCompletionTests tests/IntentCompletionTests.cpp)
    target_link_libraries(IntentCompletionTests PRIVATE harmony_core)
    add_test(NAME IntentCompletionTests COMMAND IntentCompletionTests)
    add_executable(VoiceLeadingMetricsTests tests/VoiceLeadingMetricsTests.cpp)
    target_link_libraries(VoiceLeadingMetricsTests PRIVATE harmony_preview)
    add_test(NAME VoiceLeadingMetricsTests COMMAND VoiceLeadingMetricsTests)
    add_executable(MidiExportTests tests/MidiExportTests.cpp)
    target_link_libraries(MidiExportTests PRIVATE harmony_snapshot harmony_demo_scenarios)
    target_compile_definitions(MidiExportTests PRIVATE HC_FACTORY_DB_PATH="${HC_FACTORY_LEGACY_DB}"
        HC_DEMO_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/demo"
        HC_TEST_OUTPUT_DIR="${CMAKE_CURRENT_BINARY_DIR}")
    add_dependencies(MidiExportTests factory_legacy_database)
    add_test(NAME MidiExportTests COMMAND MidiExportTests)
    # Existing facilities have no draw-round/annotation coverage; one focused target reuses their APIs.
    add_executable(WishTests tests/WishTests.cpp tests/WishAnnotationChecks.cpp tests/WishSamplerChecks.cpp tests/WishWorkflowChecks.cpp)
    target_link_libraries(WishTests PRIVATE harmony_wish harmony_snapshot)
    # The reviewed wish annotations are bound to the Factory V5 catalog (658), so the
    # contract must read the V5 database even when the product ships a newer factory.
    # V6/V7/V8 builds emit factory-v5.db as the intermediate step of factory.db.
    set(HC_WISH_FACTORY_DB "${HC_FACTORY_DB}")
    if(HC_FACTORY_LIBRARY_VERSION GREATER_EQUAL 6)
        set(HC_WISH_FACTORY_DB "${CMAKE_CURRENT_BINARY_DIR}/factory-v5.db")
    endif()
    target_compile_definitions(WishTests PRIVATE
        HC_WISH_FACTORY_DB_PATH="${HC_WISH_FACTORY_DB}"
        HC_WISH_ANNOTATION_PATH="${CMAKE_CURRENT_SOURCE_DIR}/data/mode-annotations/v2/factory-v5.hca")
    add_dependencies(WishTests factory_database)
    if(MSVC)
        target_compile_options(WishTests PRIVATE /W4 /utf-8)
    endif()
    add_test(NAME WishCoreFocused COMMAND WishTests)
    add_executable(MidiImportTests tests/MidiImportTests.cpp)
    target_link_libraries(MidiImportTests PRIVATE harmony_midi harmony_product)
    target_compile_definitions(MidiImportTests PRIVATE HC_TEST_OUTPUT_DIR="${CMAKE_CURRENT_BINARY_DIR}")
    if(MSVC)
        target_compile_options(MidiImportTests PRIVATE /utf-8 /W4)
    endif()
    add_test(NAME MidiImportTests COMMAND MidiImportTests)
    add_executable(MidiProfileTests tests/MidiProfileTests.cpp)
    target_link_libraries(MidiProfileTests PRIVATE harmony_midi)
    target_compile_definitions(MidiProfileTests PRIVATE HC_TEST_OUTPUT_DIR="${CMAKE_CURRENT_BINARY_DIR}")
    add_test(NAME MidiProfileTests COMMAND MidiProfileTests)
    add_executable(PreviewTests tests/PreviewTests.cpp)
    target_link_libraries(PreviewTests PRIVATE harmony_preview harmony_product)
    add_test(NAME PreviewTests COMMAND PreviewTests)
    add_executable(ProductizationTests tests/ProductizationTests.cpp src/plugin/RecommendationWorker.cpp)
    target_link_libraries(ProductizationTests PRIVATE harmony_demo_scenarios harmony_enrichment)
    target_compile_definitions(ProductizationTests PRIVATE
        HC_FACTORY_DB_PATH="${HC_FACTORY_LEGACY_DB}"
        HC_DEMO_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/demo")
    add_dependencies(ProductizationTests factory_legacy_database)
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
    target_link_libraries(ProgressionMatcherTests PRIVATE harmony_dev_fixtures)
    target_compile_definitions(ProgressionMatcherTests PRIVATE HC_HARMONY_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/harmony")
    add_test(NAME ProgressionMatcherTests COMMAND ProgressionMatcherTests)

    add_executable(ContinuationEngineTests tests/ContinuationEngineTests.cpp src/plugin/RecommendationWorker.cpp)
    # RecommendationWorker.cpp calls appendProductGeneration(), defined in harmony_generation.
    target_link_libraries(ContinuationEngineTests PRIVATE harmony_library harmony_enrichment harmony_generation)
    target_compile_definitions(ContinuationEngineTests PRIVATE HC_FACTORY_DB_PATH="${HC_FACTORY_LEGACY_DB}")
    add_dependencies(ContinuationEngineTests factory_legacy_database)
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

add_library(harmony_host STATIC src/host/HostEnvironment.cpp)
target_link_libraries(harmony_host PUBLIC harmony_version)
add_library(harmony_io STATIC src/io/AsyncMidiImport.cpp src/io/FileDropRouter.cpp)
target_link_libraries(harmony_io PUBLIC harmony_midi)
if(MSVC)
    target_compile_options(harmony_host PRIVATE /utf-8 /W4)
    target_compile_options(harmony_io PRIVATE /utf-8 /W4)
endif()

if(WIN32)
 foreach(HC_WORKER_TARGET HarmonyContinuation ProductizationTests ContinuationEngineTests)
  if(TARGET ${HC_WORKER_TARGET})
   target_link_libraries(${HC_WORKER_TARGET} PRIVATE bcrypt)
  endif()
 endforeach()
endif()

