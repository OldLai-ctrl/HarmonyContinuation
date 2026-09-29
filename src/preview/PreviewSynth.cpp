#include "PreviewSynth.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace harmony::preview {
void PreviewSynth::prepare(double rate,SynthConfig config) noexcept {
    sampleRate_=std::isfinite(rate)&&rate>=8000&&rate<=192000?rate:48000;
    config_=config;
    if (!std::isfinite(config_.attackSeconds)||config_.attackSeconds<=0) config_.attackSeconds=0.010;
    if (!std::isfinite(config_.releaseSeconds)||config_.releaseSeconds<=0) config_.releaseSeconds=0.100;
    if (!std::isfinite(config_.voiceGain)||config_.voiceGain<=0) config_.voiceGain=0.22f;
    reset();
}
void PreviewSynth::reset() noexcept {
    voices_={}; chords_.clear(); ends_.clear(); samplePosition_=lastEnd_=0; nextStart_=nextEnd_=0;
    running_=false; finished_=true; peakVoices_=0;
}
bool PreviewSynth::start(const Sequence& sequence) {
    reset();
    if (sequence.events.empty()) return false;
    tempo_=sanitizeTempo(sequence.tempoBPM);
    chords_.reserve(sequence.events.size());
    ChordVoicer voicer;
    for (const auto& event:sequence.events) {
        if (!std::isfinite(event.startQN)||event.startQN<0||!std::isfinite(event.durationQN)||event.durationQN<=0 ||
            event.chord.root<0 || event.chord.root>11 || event.chord.bass<0 || event.chord.bass>11 ||
            event.chord.intervals==0) { reset(); return false; }
        const auto startSample=qnToSamples(event.startQN,tempo_,sampleRate_);
        const auto endSample=qnToSamples(event.startQN+event.durationQN,tempo_,sampleRate_);
        if (endSample<=startSample || (!chords_.empty() && startSample<chords_.back().start)) { reset(); return false; }
        chords_.push_back({voicer.voice(event.chord),startSample,endSample});
        ends_.emplace_back(endSample,static_cast<int>(chords_.size()-1));
        lastEnd_=std::max(lastEnd_,endSample);
    }
    std::sort(ends_.begin(),ends_.end());
    running_=true; finished_=false;
    return true;
}
void PreviewSynth::stop() noexcept {
    running_=false;
    for (auto& voice:voices_) if (voice.active) voice.releasing=true;
}
double PreviewSynth::currentQN() const noexcept { return samplePosition_/sampleRate_*tempo_/60.0; }
void PreviewSynth::noteOn(int note,int owner) noexcept {
    Voice* chosen=nullptr;
    for (auto& voice:voices_) if (!voice.active) { chosen=&voice; break; }
    if (!chosen) chosen=&*std::min_element(voices_.begin(),voices_.end(),[](const auto& a,const auto& b){ return a.envelope<b.envelope; });
    *chosen={}; chosen->active=true; chosen->note=note; chosen->owner=owner;
    chosen->phaseStep=2*std::numbers::pi*440.0*std::pow(2.0,(note-69)/12.0)/sampleRate_;
    int active{}; for (const auto& voice:voices_) if (voice.active) ++active;
    peakVoices_=std::max(peakVoices_,active);
}
void PreviewSynth::releaseOwner(int owner) noexcept {
    for (auto& voice:voices_) if (voice.active && voice.owner==owner) voice.releasing=true;
}
void PreviewSynth::process(float* left,float* right,int samples) noexcept {
    if (!left||!right||samples<=0) return;
    for (int frame=0;frame<samples;++frame) {
        if (running_) {
            while (nextEnd_<ends_.size() && ends_[nextEnd_].first<=samplePosition_)
                releaseOwner(ends_[nextEnd_++].second);
            while (nextStart_<chords_.size() && chords_[nextStart_].start<=samplePosition_) {
                const auto& v=chords_[nextStart_].voicing;
                noteOn(v.bass,static_cast<int>(nextStart_));
                for (int i=0;i<v.upperCount;++i) noteOn(v.upper[i],static_cast<int>(nextStart_));
                ++nextStart_;
            }
        }
        double mix{}; int active{};
        for (auto& voice:voices_) if (voice.active) {
            if (voice.releasing) {
                voice.envelope-=1.0/(config_.releaseSeconds*sampleRate_);
                if (voice.envelope<=0) { voice.active=false; continue; }
            } else voice.envelope=std::min(1.0,voice.envelope+1.0/(config_.attackSeconds*sampleRate_));
            const double sine=std::sin(voice.phase);
            const double triangle=2.0/std::numbers::pi*std::asin(sine);
            mix+=(0.85*sine+0.15*triangle)*voice.envelope;
            voice.phase+=voice.phaseStep;
            if (voice.phase>=2*std::numbers::pi) voice.phase-=2*std::numbers::pi;
            ++active;
        }
        const auto sample=static_cast<float>(std::clamp(mix*config_.voiceGain/std::sqrt(static_cast<double>(std::max(1,active))),-0.95,0.95));
        left[frame]=sample; right[frame]=sample;
        ++samplePosition_;
        if (running_ && nextStart_==chords_.size() && samplePosition_>=lastEnd_) {
            running_=false;
            for (auto& voice:voices_) if (voice.active) voice.releasing=true;
        }
        if (!running_ && active==0) finished_=true;
    }
}
} // namespace harmony::preview
