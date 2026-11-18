#include "library/LibraryStore.h"
#include "library/ProgressionLibrary.h"
#include "wish/MusicIdentity.h"
#include "wish/WishCatalog.h"
#include "TemplateJson.h"
#include <chrono>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#include <winsqlite/winsqlite3.h>
#else
#include <sqlite3.h>
#endif

// Where the editor-attach stall comes from, measured on the real Factory database:
//   load    = readAll (parse every payload) + per-template checkTemplate (serialize + parse again)
//   bind    = wish annotation binding
//   identity= the string MainView::setLibrary builds from every template
// Run: attach_bench <factory.db> [repeats]
namespace {
#if defined(_WIN32)
double workingSetMb() {
    PROCESS_MEMORY_COUNTERS counters{};
    if (!GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters))) return 0;
    return static_cast<double>(counters.WorkingSetSize) / (1024.0 * 1024.0);
}
#endif
double millis(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}
std::string identity(const std::vector<harmony::ProgressionTemplate>& factory, int version) {
    std::string text = "V" + std::to_string(version) + "|";
    for (const auto& item : factory) {
        text += std::to_string(item.id.size()) + ":" + item.id + ":" + harmony::wish::sourceFingerprint(item) + ":";
        text += std::to_string(item.name.size()) + ":" + item.name;
        text += std::to_string(item.nameEn.size()) + ":" + item.nameEn;
        text += std::to_string(item.nameZh.size()) + ":" + item.nameZh + "\n";
    }
    return text;
}
}

int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "usage: attach_bench <factory.db> [repeats]\n"); return 2; }
    const std::filesystem::path path = argv[1];
    const int repeats = argc > 2 ? std::atoi(argv[2]) : 3;
    auto start = std::chrono::steady_clock::now();
    auto selection = harmony::library::loadAvailableFactory(path);
    const double coldLoad = millis(start);
    if (!selection.library) { std::fprintf(stderr, "load failed: %s\n", selection.library.error.c_str()); return 1; }
    const auto& factory = selection.library;
    start = std::chrono::steady_clock::now();
    const auto annotations = harmony::wish::annotationsForFactory(factory, selection.path, factory.libraryVersion);
    const double coldAnnotations = millis(start);
    start = std::chrono::steady_clock::now();
    auto text = identity(factory.templates, factory.libraryVersion);
    const double coldIdentity = millis(start);
    // Phase split: parse every stored payload (readAll), then re-parse each parsed template
    // exactly as checkTemplate does. Nothing here feeds the plugin; it only locates the cost.
    double parseMs = 0, rereadMs = 0, sqlMs = 0, slimMs = 0;
    {
        sqlite3* raw{};
        const auto utf8 = path.u8string();
        if (sqlite3_open_v2(reinterpret_cast<const char*>(utf8.c_str()), &raw, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) return 1;
        std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db(raw, sqlite3_close);
        sqlite3_stmt* st{};
        start = std::chrono::steady_clock::now();
        std::vector<std::string> payloads;
        if (sqlite3_prepare_v2(raw, "SELECT payload FROM progressions ORDER BY id", -1, &st, nullptr) == SQLITE_OK) {
            while (sqlite3_step(st) == SQLITE_ROW) {
                const auto* payload = reinterpret_cast<const char*>(sqlite3_column_text(st, 0));
                payloads.emplace_back(payload ? payload : "");
            }
            sqlite3_finalize(st);
        }
        sqlMs = millis(start);
        std::vector<harmony::ProgressionTemplate> parsed;
        start = std::chrono::steady_clock::now();
        for (const auto& payload : payloads) {
            auto result = harmony::dev::parseTemplateJson("[" + payload + "]");
            if (!result) { std::fprintf(stderr, "parse failed: %s\n", result.error.c_str()); return 1; }
            parsed.push_back(std::move(result.templates.front()));
        }
        parseMs = millis(start);
        // Same payloads with the typed chord blob removed: isolates decodeTemplateData.
        start = std::chrono::steady_clock::now();
        for (const auto& payload : payloads) {
            const auto cut = payload.find(",\"typedData\"");
            const auto slim = cut == std::string::npos ? payload : payload.substr(0, cut) + "}";
            auto result = harmony::dev::parseTemplateJson("[" + slim + "]");
            if (!result) { std::fprintf(stderr, "slim parse failed: %s\n", result.error.c_str()); return 1; }
        }
        slimMs = millis(start);
        start = std::chrono::steady_clock::now();
        for (const auto& item : parsed) {
            auto again = harmony::dev::parseTemplateJson("[" + harmony::dev::serializeTemplateJson(item) + "]");
            if (!again) { std::fprintf(stderr, "roundtrip failed: %s\n", again.error.c_str()); return 1; }
        }
        rereadMs = millis(start);
    }
    double copyMs = 0, identityMs = 0;
    for (int i = 0; i < repeats; ++i) {
        start = std::chrono::steady_clock::now();
        auto copy = factory.templates;
        copyMs += millis(start);
        start = std::chrono::steady_clock::now();
        auto again = identity(copy, factory.libraryVersion);
        identityMs += millis(start);
    }
    std::printf("library=%s entries=%zu version=%d\n", path.string().c_str(), factory.templates.size(), factory.libraryVersion);
    std::printf("cold load          %8.1f ms   (sql select %.1f + json parse %.1f [no typedData %.1f] + roundtrip %.1f)\n",
                coldLoad, sqlMs, parseMs, slimMs, rereadMs);
    std::printf("cold annotations   %8.1f ms (catalog=%zu error=%s)\n", coldAnnotations, annotations.catalog.entries.size(),
                annotations.error.empty() ? "none" : annotations.error.c_str());
    std::printf("cold identity      %8.1f ms (chars=%zu)\n", coldIdentity, text.size());
    std::printf("warm copy          %8.1f ms (avg of %d)\n", copyMs / repeats, repeats);
    std::printf("warm identity      %8.1f ms (avg of %d)\n", identityMs / repeats, repeats);
    std::printf("attach cold total  %8.1f ms\n", coldLoad + coldAnnotations + coldIdentity);
    std::printf("attach warm total  %8.1f ms\n", copyMs / repeats + identityMs / repeats);
#if defined(_WIN32)
    std::printf("working set       %8.1f MB (library resident)\n", workingSetMb());
#endif
    return 0;
}
