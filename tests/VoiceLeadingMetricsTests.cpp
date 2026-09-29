#include "preview/VoiceLeadingMetrics.h"
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace harmony;
Progression phrase(std::initializer_list<const char*> names) {
    Progression result;
    for(const auto* name:names) {
        ChordEvent event;
        event.name=name;
        event.startQN=static_cast<double>(result.size())*4;
        event.durationQN=4;
        event.openEnded=false;
        result.push_back(std::move(event));
    }
    return result;
}
void check(bool condition,const char* message) {
    if(!condition)throw std::runtime_error(message);
}
} // namespace
int main() {
    try {
        const auto common=preview::measureVoiceLeading(phrase({"Cmaj7","Am7"}));
        check(common && common->commonToneCount>=2 && common->commonToneRatio>0,
              "shared voiced tones are measured");
        const auto roots=preview::measureVoiceLeading(phrase({"C","G","Am"}));
        const auto inversion=preview::measureVoiceLeading(phrase({"C","G/B","Am"}));
        check(roots && inversion && roots->transitions.size()==2 && inversion->transitions.size()==2,
              "all adjacent chords measured");
        check(inversion->bassMidi[0]%12==0 && inversion->bassMidi[1]%12==11 &&
              inversion->bassMidi[2]%12==9,"inversion yields C B A bass");
        check(inversion->bassMotionSemitones<roots->bassMotionSemitones,
              "first inversion shortens the actual bass path");
        check(inversion->score>=0 && inversion->score<=1 && inversion->maximumVoiceLeap>=0 &&
              inversion->voiceCrossingPenalty==0,"bounded voice-leading metrics");
        check(!preview::measureVoiceLeading({}),"empty phrase rejected");
        std::cout << "voice-leading cases passed\n";
    } catch(const std::exception& e) {
        std::cerr << "VoiceLeadingMetricsTests: " << e.what() << '\n';
        return 1;
    }
}
