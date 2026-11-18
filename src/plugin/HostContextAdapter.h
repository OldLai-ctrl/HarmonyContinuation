#pragma once
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include <atomic>
#include <array>
#include <cstdint>
#include <string>
#include "host/HostEnvironment.h"

namespace harmony::plugin {
// All fields are atomic: no undefined data races in the bounded seqlock reader.
// Only process() writes. Main-thread notify() reads on explicit UI request.
struct HostSnapshot {
    std::array<std::uint64_t, 12> words{};
    std::uint64_t generation{};
};
class HostContextAdapter {
public:
    void capture(const Steinberg::Vst::ProcessContext* context, bool midiEventsAvailable=false) noexcept;
    bool read(HostSnapshot& snapshot) const noexcept;
    static host::HostTimelineContext timeline(const HostSnapshot&) noexcept;
    static std::string describe(const HostSnapshot& snapshot);
    static std::string summarize(const HostSnapshot& snapshot);
private:
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
    std::atomic<std::uint64_t> revision_{};
    std::atomic<std::uint64_t> generation_{};
    std::array<std::atomic<std::uint64_t>, 12> words_{};
};
}
