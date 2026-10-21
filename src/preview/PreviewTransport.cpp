#include "PreviewTransport.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace harmony::preview {
namespace {
constexpr std::int64_t maxPreviewFrames =
    static_cast<std::int64_t>(600.0 * previewReferenceRate);

bool qnToReferenceSample(double qn, double tempo, std::int64_t& sample) noexcept {
    if (!std::isfinite(qn) || qn < 0.0 || !std::isfinite(tempo) || tempo <= 0.0)
        return false;
    const long double frames = static_cast<long double>(qn) * 60.0L /
        static_cast<long double>(tempo) * static_cast<long double>(previewReferenceRate);
    const auto limit = static_cast<long double>(std::numeric_limits<std::int64_t>::max());
    if (!std::isfinite(frames) || frames < 0.0L || frames > limit - 0.5L)
        return false;
    sample = static_cast<std::int64_t>(std::llround(frames));
    return true;
}

bool scaleReferenceSample(std::int64_t reference, double rate, std::int64_t& sample) noexcept {
    if (reference < 0 || !std::isfinite(rate) || rate < 8000.0 || rate > 192000.0)
        return false;
    const long double frames = static_cast<long double>(reference) *
        static_cast<long double>(rate) / static_cast<long double>(previewReferenceRate);
    const auto limit = static_cast<long double>(std::numeric_limits<std::int64_t>::max());
    if (!std::isfinite(frames) || frames < 0.0L || frames > limit - 0.5L)
        return false;
    sample = static_cast<std::int64_t>(std::llround(frames));
    return true;
}

bool validScope(PreviewScope scope) noexcept {
    return scope == PreviewScope::Full || scope == PreviewScope::Suffix;
}
} // namespace

PreparedPreviewResult::operator bool() const noexcept {
    return error.empty() && validPreparedPreview(clip);
}

PreparedPreviewResult preparePreview(const Sequence& sequence, PreviewScope scope) {
    PreparedPreviewResult result;
    const auto fail = [&result](const char* message) {
        result.error = message;
        return result;
    };
    if (!validScope(scope)) return fail("invalid preview scope");
    if (sequence.events.empty()) return fail("empty progression");
    if (sequence.events.size() > maximumPreviewChords) return fail("too many preview chords");

    auto& clip = result.clip;
    clip.tempoBPM = sanitizeTempo(sequence.tempoBPM);
    const auto range = previewRange(sequence, scope);
    std::int64_t rangeStart{}, rangeEnd{};
    if (!std::isfinite(range.startQN) || !std::isfinite(range.endQN) ||
        range.startQN < 0.0 || range.endQN <= range.startQN ||
        !qnToReferenceSample(range.startQN, clip.tempoBPM, rangeStart) ||
        !qnToReferenceSample(range.endQN, clip.tempoBPM, rangeEnd) ||
        rangeEnd <= rangeStart || rangeEnd - rangeStart > maxPreviewFrames)
        return fail("preview range exceeds supported time bounds");
    clip.startSample = rangeStart;
    clip.endSample = rangeEnd;

    ChordVoicer voicer;
    double previousStart = -1.0;
    for (const auto& event : sequence.events) {
        const double eventEndQN = event.startQN + event.durationQN;
        std::int64_t eventStart{}, eventEnd{};
        if (!std::isfinite(event.startQN) || event.startQN < 0.0 ||
            !std::isfinite(event.durationQN) || event.durationQN <= 0.0 ||
            !std::isfinite(eventEndQN) || event.startQN < previousStart ||
            event.chord.root < 0 || event.chord.root > 11 ||
            event.chord.bass < 0 || event.chord.bass > 11 || event.chord.intervals == 0 ||
            !qnToReferenceSample(event.startQN, clip.tempoBPM, eventStart) ||
            !qnToReferenceSample(eventEndQN, clip.tempoBPM, eventEnd) || eventEnd <= eventStart)
            return fail("invalid preview event");
        previousStart = event.startQN;

        // Voice the complete route in order. Suffix clipping must retain the same
        // voice-leading decision as the corresponding event in a full preview.
        const auto voicing = voicer.voice(event.chord, event.topVoice);
        const auto clippedStart = std::max(eventStart, rangeStart);
        const auto clippedEnd = std::min(eventEnd, rangeEnd);
        if (clippedEnd <= clippedStart) continue;
        if (clip.count >= maximumPreviewChords) return fail("too many preview chords");
        clip.chords[clip.count++] = {voicing, clippedStart, clippedEnd};
    }
    if (!validPreparedPreview(clip)) return fail("preview contains no playable chords");
    return result;
}

bool validPreparedPreview(const PreparedPreview& clip) noexcept {
    if (clip.version != 1 || clip.count == 0 || clip.count > maximumPreviewChords ||
        !std::isfinite(clip.tempoBPM) || clip.tempoBPM < minimumTempoBPM ||
        clip.tempoBPM > maximumTempoBPM || clip.startSample < 0 ||
        clip.endSample <= clip.startSample || clip.endSample - clip.startSample > maxPreviewFrames)
        return false;

    std::int64_t previousStart = clip.startSample;
    for (std::size_t i = 0; i < clip.count; ++i) {
        const auto& chord = clip.chords[i];
        if (chord.start < clip.startSample || chord.start < previousStart ||
            chord.start >= clip.endSample || chord.end <= chord.start || chord.end > clip.endSample ||
            chord.voicing.bass < 0 || chord.voicing.bass > 127 ||
            chord.voicing.upperCount < 1 ||
            chord.voicing.upperCount > static_cast<int>(chord.voicing.upper.size()))
            return false;
        previousStart = chord.start;
        for (int noteIndex = 0; noteIndex < chord.voicing.upperCount; ++noteIndex) {
            const int note = chord.voicing.upper[static_cast<std::size_t>(noteIndex)];
            if (note < 0 || note > 127) return false;
        }
    }
    return true;
}

void PreviewTransport::prepare(double rate) noexcept {
    ratePrepared_ = std::isfinite(rate) && rate >= 8000.0 && rate <= 192000.0;
    sampleRate_ = ratePrepared_ ? rate : previewReferenceRate;
    for (std::size_t note = 0; note < phaseSteps_.size(); ++note) {
        phaseSteps_[note] = 2.0 * std::numbers::pi * 440.0 *
            std::pow(2.0, (static_cast<double>(note) - 69.0) / 12.0) / sampleRate_;
    }
    count_ = nextStart_ = nextEnd_ = 0;
    tempoBPM_ = defaultTempoBPM;
    startSample_ = endSample_ = samplePosition_ = 0;
    state_ = PlaybackState::Stopped;
    clearVoices();
}

bool PreviewTransport::load(const PreparedPreview& preview) noexcept {
    if (!ratePrepared_ || !validPreparedPreview(preview)) return false;
    std::int64_t scaledStart{}, scaledEnd{};
    if (!scaleReferenceSample(preview.startSample, sampleRate_, scaledStart) ||
        !scaleReferenceSample(preview.endSample, sampleRate_, scaledEnd) || scaledEnd <= scaledStart)
        return false;

    for (std::size_t i = 0; i < preview.count; ++i) {
        std::int64_t start{}, end{};
        if (!scaleReferenceSample(preview.chords[i].start, sampleRate_, start) ||
            !scaleReferenceSample(preview.chords[i].end, sampleRate_, end) ||
            start < scaledStart || end <= start || end > scaledEnd)
            return false;
    }

    count_ = preview.count;
    tempoBPM_ = preview.tempoBPM;
    startSample_ = scaledStart;
    endSample_ = scaledEnd;
    for (std::size_t i = 0; i < count_; ++i) {
        const auto& source = preview.chords[i];
        auto& target = chords_[i];
        target.voicing = source.voicing;
        scaleReferenceSample(source.start, sampleRate_, target.start);
        scaleReferenceSample(source.end, sampleRate_, target.end);
        endOrder_[i] = static_cast<std::uint32_t>(i);
    }
    // Fixed-size insertion sort matches PreviewSynth's end ordering and never
    // allocates, even when load is applied at an audio block boundary.
    for (std::size_t i = 1; i < count_; ++i) {
        const auto value = endOrder_[i];
        std::size_t j = i;
        while (j > 0) {
            const auto previous = endOrder_[j - 1];
            const auto& lhs = chords_[previous];
            const auto& rhs = chords_[value];
            if (lhs.end < rhs.end || (lhs.end == rhs.end && previous < value)) break;
            endOrder_[j] = previous;
            --j;
        }
        endOrder_[j] = value;
    }
    resetAtStart();
    state_ = PlaybackState::Stopped;
    return true;
}

void PreviewTransport::play() noexcept {
    if (count_ == 0 || !ratePrepared_) return;
    if (state_ == PlaybackState::Paused) {
        state_ = PlaybackState::Playing;
        return;
    }
    resetAtStart();
    state_ = PlaybackState::Playing;
}

void PreviewTransport::pause() noexcept {
    if (state_ == PlaybackState::Playing) state_ = PlaybackState::Paused;
}

void PreviewTransport::stop() noexcept {
    resetAtStart();
    state_ = PlaybackState::Stopped;
}

void PreviewTransport::seekQN(double qn) noexcept {
    if (count_ == 0) return;
    const auto low = startQN();
    const auto high = endQN();
    if (!std::isfinite(qn)) qn = low;
    qn = std::clamp(qn, low, high);
    const long double requested = static_cast<long double>(qn) * 60.0L /
        static_cast<long double>(tempoBPM_) * static_cast<long double>(sampleRate_);
    const auto limit = static_cast<long double>(std::numeric_limits<std::int64_t>::max());
    auto target = requested > limit - 0.5L ? endSample_ : static_cast<std::int64_t>(std::llround(requested));
    target = std::clamp(target, startSample_, endSample_);
    if (target >= endSample_) {
        samplePosition_ = endSample_;
        nextStart_ = nextEnd_ = count_;
        clearVoices();
        state_ = PlaybackState::Stopped;
        return;
    }

    samplePosition_ = target;
    if (state_ == PlaybackState::Playing || state_ == PlaybackState::Paused)
        rebuildAt(target);
    else {
        nextStart_ = nextEnd_ = 0;
        clearVoices();
    }
}

void PreviewTransport::process(float* left, float* right, int samples) noexcept {
    if (!left || !right || samples <= 0) return;
    if (state_ != PlaybackState::Playing || count_ == 0) {
        std::fill_n(left, samples, 0.0f);
        std::fill_n(right, samples, 0.0f);
        return;
    }

    const double attackStep = 1.0 / (attackSeconds * sampleRate_);
    const double releaseStep = 1.0 / (releaseSeconds * sampleRate_);
    for (int frame = 0; frame < samples; ++frame) {
        if (samplePosition_ >= endSample_) {
            samplePosition_ = endSample_;
            clearVoices();
            state_ = PlaybackState::Stopped;
            std::fill(left + frame, left + samples, 0.0f);
            std::fill(right + frame, right + samples, 0.0f);
            return;
        }

        applyEventsAt(samplePosition_);
        double mix{};
        int active{};
        for (auto& voice : voices_) if (voice.active) {
            if (voice.releasing) {
                voice.envelope -= releaseStep;
                if (voice.envelope <= 0.0) {
                    voice.active = false;
                    continue;
                }
            } else {
                voice.envelope = std::min(1.0, voice.envelope + attackStep);
            }
            const double sine = std::sin(voice.phase);
            const double triangle = 2.0 / std::numbers::pi * std::asin(sine);
            mix += (0.85 * sine + 0.15 * triangle) * voice.envelope;
            voice.phase += voice.phaseStep;
            if (voice.phase >= 2.0 * std::numbers::pi) voice.phase -= 2.0 * std::numbers::pi;
            ++active;
        }
        const auto sample = static_cast<float>(std::clamp(
            mix * voiceGain / std::sqrt(static_cast<double>(std::max(1, active))), -0.95, 0.95));
        left[frame] = sample;
        right[frame] = sample;
        ++samplePosition_;
    }

    if (samplePosition_ >= endSample_) {
        samplePosition_ = endSample_;
        clearVoices();
        state_ = PlaybackState::Stopped;
    }
}

double PreviewTransport::positionQN() const noexcept {
    if (count_ == 0 || sampleRate_ <= 0.0) return 0.0;
    return static_cast<double>(samplePosition_) / sampleRate_ * tempoBPM_ / 60.0;
}

double PreviewTransport::startQN() const noexcept {
    if (sampleRate_ <= 0.0) return 0.0;
    return static_cast<double>(startSample_) / sampleRate_ * tempoBPM_ / 60.0;
}

double PreviewTransport::endQN() const noexcept {
    if (sampleRate_ <= 0.0) return 0.0;
    return static_cast<double>(endSample_) / sampleRate_ * tempoBPM_ / 60.0;
}

void PreviewTransport::clearVoices() noexcept { voices_ = {}; }

void PreviewTransport::resetAtStart() noexcept {
    samplePosition_ = startSample_;
    nextStart_ = nextEnd_ = 0;
    clearVoices();
}

void PreviewTransport::applyEventsAt(std::int64_t sample) noexcept {
    while (nextEnd_ < count_ && chords_[endOrder_[nextEnd_]].end <= sample)
        releaseOwner(endOrder_[nextEnd_++]);
    while (nextStart_ < count_ && chords_[nextStart_].start <= sample) {
        const auto& voicing = chords_[nextStart_].voicing;
        noteOn(voicing.bass, nextStart_);
        for (int i = 0; i < voicing.upperCount; ++i)
            noteOn(voicing.upper[static_cast<std::size_t>(i)], nextStart_);
        ++nextStart_;
    }
}

void PreviewTransport::advanceVoices(std::int64_t samples) noexcept {
    if (samples <= 0) return;
    const double attackStep = 1.0 / (attackSeconds * sampleRate_);
    const double releaseStep = 1.0 / (releaseSeconds * sampleRate_);
    const double tau = 2.0 * std::numbers::pi;
    for (auto& voice : voices_) if (voice.active) {
        std::int64_t phaseSamples = samples;
        if (voice.releasing) {
            const auto untilInactive = static_cast<std::int64_t>(std::ceil(voice.envelope / releaseStep));
            phaseSamples = std::min(samples, std::max<std::int64_t>(0, untilInactive - 1));
            if (samples >= untilInactive) {
                voice.active = false;
                voice.envelope = 0.0;
            } else {
                voice.envelope = std::max(0.0, voice.envelope - releaseStep * static_cast<double>(samples));
            }
        } else {
            voice.envelope = std::min(1.0, voice.envelope + attackStep * static_cast<double>(samples));
        }
        voice.phase = std::fmod(voice.phase + voice.phaseStep * static_cast<double>(phaseSamples), tau);
        if (voice.phase < 0.0) voice.phase += tau;
    }
}

void PreviewTransport::rebuildAt(std::int64_t target) noexcept {
    clearVoices();
    nextStart_ = nextEnd_ = 0;
    samplePosition_ = startSample_;
    while (true) {
        applyEventsAt(samplePosition_);
        if (samplePosition_ >= target) break;
        auto next = target;
        if (nextStart_ < count_) next = std::min(next, chords_[nextStart_].start);
        if (nextEnd_ < count_) next = std::min(next, chords_[endOrder_[nextEnd_]].end);
        if (next <= samplePosition_) break;
        advanceVoices(next - samplePosition_);
        samplePosition_ = next;
    }
    samplePosition_ = target;
}

void PreviewTransport::noteOn(int note, std::size_t owner) noexcept {
    Voice* chosen{};
    for (auto& voice : voices_) if (!voice.active) { chosen = &voice; break; }
    if (!chosen) chosen = &*std::min_element(voices_.begin(), voices_.end(),
        [](const auto& a, const auto& b) { return a.envelope < b.envelope; });
    *chosen = {};
    chosen->active = true;
    chosen->note = note;
    chosen->owner = static_cast<int>(owner);
    chosen->phaseStep = phaseSteps_[static_cast<std::size_t>(note)];
}

void PreviewTransport::releaseOwner(std::size_t owner) noexcept {
    for (auto& voice : voices_)
        if (voice.active && voice.owner == static_cast<int>(owner)) voice.releasing = true;
}

} // namespace harmony::preview
