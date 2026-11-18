#pragma once
#include "midi/MidiImportWorkflow.h"
#include <condition_variable>
#include <mutex>
#include <thread>
namespace harmony::io {
struct CompletedMidiImport {std::uint64_t generation{};midi::ImportResult result;};
// Single replaceable request/result. Parsing is never performed on the UI or audio thread.
class AsyncMidiImport {
public:
    AsyncMidiImport();~AsyncMidiImport();
    AsyncMidiImport(const AsyncMidiImport&)=delete;
    std::uint64_t submit(std::vector<std::filesystem::path>,bool openEnded);
    std::optional<CompletedMidiImport> takeLatest();
    void cancel();
private:
    struct Task {std::uint64_t generation{};std::vector<std::filesystem::path> paths;bool open{};};
    std::mutex mutex_;std::condition_variable wake_;bool stop_{};std::uint64_t generation_{};
    std::optional<Task> pending_;std::optional<CompletedMidiImport> ready_;std::thread thread_;
    void run();
};
}
