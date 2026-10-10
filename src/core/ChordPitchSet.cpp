#include "ChordPitchSet.h"
namespace harmony {
ChordPitchSet chordPitches(const ChordEvent& event) {
    auto base=event;
    if(const auto slash=base.name.find('/');slash!=std::string::npos)base.name.resize(slash);
    const auto normalized=normalizeChord(base);
    ChordPitchSet c{event.name,normalized.root?static_cast<int>(*normalized.root):-1,
        normalized.bass?static_cast<int>(*normalized.bass):-1,normalized.quality,normalized.intervalMask,normalized.colorMask};
    if(c.bass<0)c.bass=c.root;
    c.exactIntervals=event.extensions.mask.has_value() || event.extensions.pitches.has_value();
    if(const auto slash=event.name.find('/');slash!=std::string::npos) {
        ChordEvent bass; bass.name=event.name.substr(slash+1);
        if(const auto n=normalizeChord(bass);n.root)c.bass=static_cast<int>(*n.root);
    }
    return c;
}
ChordPitchSet chordPitches(const std::string& label,ChordQuality hint) {
    ChordEvent event;event.name=label;event.quality=hint;return chordPitches(event);
}
}
