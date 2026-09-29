#pragma once
#include "ChordVoicer.h"
#include <array>
#include <cstdint>
#include <vector>

namespace harmony::preview {
struct SynthConfig { double attackSeconds{0.010}, releaseSeconds{0.100}; float voiceGain{0.22f}; };
struct ScheduledChord { Voicing voicing; std::int64_t start{}, end{}; };
class PreviewSynth {
public:
    static constexpr int maxVoices=16;
    void prepare(double sampleRate,SynthConfig config={}) noexcept;
    bool start(const Sequence&);
    bool replace(const Sequence& sequence) { reset(); return start(sequence); }
    void stop() noexcept;
    void reset() noexcept;
    void process(float* left,float* right,int samples) noexcept;
    std::int64_t currentSample() const noexcept { return samplePosition_; }
    double currentQN() const noexcept;
    bool finished() const noexcept { return finished_; }
    bool playing() const noexcept { return running_; }
    int peakVoices() const noexcept { return peakVoices_; }
    const std::vector<ScheduledChord>& schedule() const noexcept { return chords_; }
private:
    struct Voice { double phase{}, phaseStep{}, envelope{}; int note{}, owner{-1}; bool active{}, releasing{}; };
    std::array<Voice,maxVoices> voices_{};
    std::vector<ScheduledChord> chords_;
    std::vector<std::pair<std::int64_t,int>> ends_;
    double sampleRate_{48000}, tempo_{120};
    SynthConfig config_{};
    std::int64_t samplePosition_{};
    std::int64_t lastEnd_{};
    std::size_t nextStart_{}, nextEnd_{};
    bool running_{}, finished_{true};
    int peakVoices_{};
    void noteOn(int note,int owner) noexcept;
    void releaseOwner(int owner) noexcept;
};
} // namespace harmony::preview
