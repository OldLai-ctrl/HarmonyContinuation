#pragma once

#include "PreviewSynth.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace harmony::preview {

enum class PlaybackState : std::uint32_t { Stopped, Playing, Paused };

inline constexpr std::size_t maximumPreviewChords = 128;
inline constexpr double previewReferenceRate = 48000.0;

struct PreparedPreview {
    std::uint32_t version{1}, count{};
    double tempoBPM{120.0};
    std::int64_t startSample{}, endSample{};
    std::array<ScheduledChord, maximumPreviewChords> chords{};
};

struct PreparedPreviewResult {
    PreparedPreview clip;
    std::string error;
    explicit operator bool() const noexcept;
};

PreparedPreviewResult preparePreview(const Sequence&, PreviewScope = PreviewScope::Full);
bool validPreparedPreview(const PreparedPreview&) noexcept;

// Playback state is owned by one audio or worker thread. Callers marshal controls
// to that owner; this object deliberately contains no synchronization primitives.
class PreviewTransport {
public:
    void prepare(double rate) noexcept;
    bool load(const PreparedPreview&) noexcept;
    void play() noexcept;
    void pause() noexcept;
    void stop() noexcept;
    void seekQN(double) noexcept;
    void process(float* left, float* right, int samples) noexcept;

    PlaybackState state() const noexcept { return state_; }
    double positionQN() const noexcept;
    double startQN() const noexcept;
    double endQN() const noexcept;
    double tempoBPM() const noexcept { return tempoBPM_; }

private:
    struct RuntimeChord {
        Voicing voicing;
        std::int64_t start{}, end{};
    };
    struct Voice {
        double phase{}, phaseStep{}, envelope{};
        int note{}, owner{-1};
        bool active{}, releasing{};
    };

    static constexpr std::size_t maxVoices = 16;
    static constexpr double attackSeconds = 0.010;
    static constexpr double releaseSeconds = 0.100;
    static constexpr double voiceGain = 0.22;

    std::array<RuntimeChord, maximumPreviewChords> chords_{};
    std::array<std::uint32_t, maximumPreviewChords> endOrder_{};
    std::array<Voice, maxVoices> voices_{};
    std::array<double, 128> phaseSteps_{};
    std::size_t count_{}, nextStart_{}, nextEnd_{};
    double sampleRate_{previewReferenceRate}, tempoBPM_{120.0};
    std::int64_t samplePosition_{}, startSample_{}, endSample_{};
    PlaybackState state_{PlaybackState::Stopped};
    bool ratePrepared_{};

    void clearVoices() noexcept;
    void resetAtStart() noexcept;
    void applyEventsAt(std::int64_t sample) noexcept;
    void advanceVoices(std::int64_t samples) noexcept;
    void rebuildAt(std::int64_t sample) noexcept;
    void noteOn(int note, std::size_t owner) noexcept;
    void releaseOwner(std::size_t owner) noexcept;
};

} // namespace harmony::preview
