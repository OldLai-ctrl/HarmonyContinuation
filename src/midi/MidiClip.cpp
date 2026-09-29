#include "MidiClip.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace harmony::midi {
namespace {
bool validMeter(Meter m) { return m.numerator>=1 && m.numerator<=32 &&
    (m.denominator==1||m.denominator==2||m.denominator==4||m.denominator==8||m.denominator==16||m.denominator==32); }
int bassNote(int pc) {
    int best=36, distance=100;
    for (int n=36;n<=48;++n) if (n%12==pc && std::abs(n-43)<distance) { best=n; distance=std::abs(n-43); }
    return best;
}
}
std::int64_t qnToTicks(double qn,std::uint16_t ppq) noexcept {
    if (!std::isfinite(qn)||qn<0||ppq==0||qn>static_cast<double>(std::numeric_limits<std::int64_t>::max())/ppq) return -1;
    return static_cast<std::int64_t>(std::llround(qn*ppq));
}
preview::BuildResult previewFromTemplate(const ProgressionTemplate& item,KeySignature key,double tempo) {
    preview::BuildResult result;
    if (item.full.empty()||item.full.size()>64) {result.error="invalid library progression";return result;}
    ImportedProgressionSession imported; Progression chords; double at{};
    for (const auto& e:item.full) {
        const double duration=e.durationQN.value_or(4.0);
        if (!std::isfinite(duration)||duration<=0||duration>128) {result.error="invalid library duration";return result;}
        const auto concrete=realizeContinuation(e,key,duration);
        ChordEvent chord;chord.name=concrete.label;chord.quality=concrete.quality;
        chord.startQN=at;chord.durationQN=duration;chord.openEnded=false;
        chords.push_back(std::move(chord));at+=duration;
    }
    if (!imported.replace(std::move(chords),TimelineCoordinateMode::RelativeToSelection)) {
        result.error="invalid library order";return result;
    }
    return preview::buildSequence(imported,nullptr,tempo);
}
BuildResult buildClip(const preview::Sequence& preview,ArrangementMode mode,ExportScope scope,Meter meter,
                      std::optional<KeySignature> key,std::optional<PhraseIntent> intent,ExportConfig config) {
    BuildResult result; auto& out=result.sequence;
    out.mode=mode; out.scope=scope; out.meter=meter; out.key=key; out.intent=intent; out.tempoBPM=preview.tempoBPM;
    if (!validMeter(meter)||config.ppq==0||config.channel>15||config.upperVelocity==0||
        config.upperVelocity>127||config.bassVelocity==0||config.bassVelocity>127||
        preview.events.empty()||preview.recommendationBoundary>preview.events.size()) {
        result.error="invalid clip settings"; return result;
    }
    const std::size_t begin=scope==ExportScope::ContinuationOnly?preview.recommendationBoundary:0;
    const std::size_t end=scope==ExportScope::CurrentOnly?preview.recommendationBoundary:preview.events.size();
    if (begin>=end) { result.error="empty export scope"; return result; }
    const double offset=scope==ExportScope::ContinuationOnly?preview.events[begin].startQN:0;
    preview::ChordVoicer voicer;
    for (std::size_t i=0;i<preview.events.size();++i) {
        const auto& event=preview.events[i];
        if (!std::isfinite(event.startQN)||!std::isfinite(event.durationQN)||event.durationQN<=0||
            event.chord.root<0||event.chord.root>11||event.chord.bass<0||event.chord.bass>11||
            event.chord.intervals==0) { result.error="invalid preview event"; result.sequence={}; return result; }
        // Compute all voicings, even for suffix-only exports, to preserve the full preview's voice path.
        const auto voiced=mode==ArrangementMode::VoiceLed?voicer.voice(event.chord):preview::Voicing{};
        if (i<begin||i>=end) continue;
        const auto start=event.startQN-offset;
        if (start<0||!std::isfinite(start)) { result.error="invalid relative start"; result.sequence={}; return result; }
        out.markers.push_back({start,MarkerType::ChordLabel,event.chord.label});
        auto note=[&](int pitch,std::uint8_t velocity,bool bass) {
            out.notes.push_back({pitch,start,event.durationQN,velocity,config.channel,bass});
        };
        if (mode==ArrangementMode::VoiceLed) {
            note(voiced.bass,config.bassVelocity,true);
            for (int v=0;v<voiced.upperCount;++v) note(voiced.upper[v],config.upperVelocity,false);
        } else {
            note(bassNote(event.chord.bass),config.bassVelocity,true);
            for (int semitone=0;semitone<12;++semitone) if (event.chord.intervals&(1u<<semitone))
                note(60+(event.chord.root+semitone)%12,config.upperVelocity,false);
        }
        out.totalQN=std::max(out.totalQN,start+event.durationQN);
        ++out.chordCount;
    }
    if (scope!=ExportScope::CurrentOnly && preview.recommendationBoundary<preview.events.size()) {
        const double boundary=scope==ExportScope::ContinuationOnly?0:
            preview.events[preview.recommendationBoundary].startQN;
        out.boundaryQN=boundary;
        out.markers.push_back({boundary,MarkerType::RecommendedStart,"HarmonyContinuation: Recommended Start"});
    }
    return result;
}
std::string suggestedFilename(std::optional<PhraseIntent> intent,std::optional<KeySignature> key,int number) {
    constexpr const char* pitches[]{"C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B"};
    std::string result="HC_";
    switch (intent.value_or(PhraseIntent::Neutral)) {
        case PhraseIntent::Resolve: result+="Resolve"; break;
        case PhraseIntent::Develop: result+="Develop"; break;
        case PhraseIntent::Loop: result+="Loop"; break;
        case PhraseIntent::Color: result+="Color"; break;
        default: result+="Current"; break;
    }
    result+='_';
    if (key && static_cast<int>(key->tonic)<12) {
        result+=pitches[static_cast<int>(key->tonic)];
        result+=key->mode==Mode::Major?"Major":"Minor";
    } else result+="AutoKey";
    result+='_';
    if (number<1) number=1;
    if (number<10) result+='0';
    result+=std::to_string(number);
    result+=".mid";
    return result;
}
} // namespace harmony::midi
