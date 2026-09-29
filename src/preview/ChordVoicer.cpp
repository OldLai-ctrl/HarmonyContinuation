#include "ChordVoicer.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace harmony::preview {
namespace {
int closestBass(int pc) {
    int best=36, distance=100;
    for (int note=36;note<=48;++note) if (note%12==pc) {
        const int d=std::abs(note-43);
        if (d<distance) { best=note; distance=d; }
    }
    return best;
}
std::vector<int> priorityIntervals(const Chord& c) {
    std::vector<int> result;
    auto add=[&](int n) { if ((c.intervals&(1u<<n)) && std::find(result.begin(),result.end(),n)==result.end()) result.push_back(n); };
    // Core identity is selected before optional colors or fifths.
    switch (c.quality) {
        case ChordQuality::Minor: case ChordQuality::Minor7:
        case ChordQuality::Diminished: case ChordQuality::Diminished7:
        case ChordQuality::HalfDiminished7: add(3); break;
        case ChordQuality::Sus2: add(2); break;
        case ChordQuality::Sus4: add(5); break;
        default: add(4); break;
    }
    if (c.quality==ChordQuality::HalfDiminished7 || c.quality==ChordQuality::Diminished7 || c.quality==ChordQuality::Diminished) add(6);
    if (c.quality==ChordQuality::Augmented) add(8);
    if (c.quality==ChordQuality::Diminished7) add(9);
    if (c.quality==ChordQuality::Major7) add(11);
    if (c.quality==ChordQuality::Minor7 || c.quality==ChordQuality::Dominant7 || c.quality==ChordQuality::HalfDiminished7) add(10);
    for (int n:{1,2,3,9}) add(n); // altered ninth, ninth, thirteenth
    add(7); add(8); add(6); add(0);
    for (int n=0;n<12;++n) add(n);
    if (result.size()>4) result.resize(4);
    if (result.size()<3) { add(0); add(7); }
    return result;
}
}
Voicing ChordVoicer::voice(const Chord& chord) {
    const auto intervals=priorityIntervals(chord);
    Voicing best; best.bass=closestBass(chord.bass); best.upperCount=static_cast<int>(intervals.size());
    double bestCost=std::numeric_limits<double>::infinity();
    // Enumerate a few rotations and octave placements. Every option has the same pitch-class identity.
    for (int rotation=0;rotation<best.upperCount;++rotation) for (int base=48;base<=64;++base) {
        Voicing v; v.bass=best.bass; v.upperCount=best.upperCount;
        for (int i=0;i<v.upperCount;++i) {
            const int interval=intervals[(i+rotation)%v.upperCount];
            int note=base+(chord.root+interval-base%12+24)%12;
            if (i && note<=v.upper[i-1]) note+=12;
            v.upper[i]=note;
        }
        if (v.upper[v.upperCount-1]>84) continue;
        double cost{};
        if (hasPrevious_) {
            for (int i=0;i<std::min(v.upperCount,previous_.upperCount);++i) {
                const int leap=std::abs(v.upper[i]-previous_.upper[i]);
                cost+=leap+std::max(0,leap-7)*2;
            }
            cost+=std::abs(v.bass-previous_.bass)*0.35;
        } else {
            for (int i=0;i<v.upperCount;++i) cost+=std::abs(v.upper[i]-(60+i*4))*0.3;
        }
        cost+=(v.upper[v.upperCount-1]-v.upper[0])*0.1;
        for (int i=0;i<v.upperCount;++i) cost+=std::abs(v.upper[i]-65)*0.015;
        if (cost<bestCost) { bestCost=cost; best=v; }
    }
    previous_=best; hasPrevious_=true;
    return best;
}
} // namespace harmony::preview
