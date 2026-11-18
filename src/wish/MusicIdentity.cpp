#include "MusicIdentity.h"
#include "library/LegacyFactoryIdResolver.h"
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace harmony::wish {
std::string performanceFingerprint(const preview::Sequence& sequence,int numerator,int denominator) {
    if(sequence.events.empty() || numerator<1 || numerator>32 || denominator<1 || denominator>32)
        throw std::invalid_argument("invalid complete performance");
    std::ostringstream out;out.imbue(std::locale::classic());
    out<<"performance-v1:"<<numerator<<'/'<<denominator<<std::hexfloat;
    const double anchor=sequence.events.front().startQN;
    double previous=anchor;
    for(const auto& e:sequence.events) {
        const auto& c=e.chord;
        if(!std::isfinite(e.startQN)||!std::isfinite(e.durationQN)||e.startQN<previous||e.durationQN<=0 ||
           c.root<0||c.root>11||c.bass<0||c.bass>11||!c.intervals||((c.intervals|c.colorMask)&~0xfffu))
            throw std::invalid_argument("invalid complete performance event");
        previous=e.startQN;
        out<<'|'<<c.root<<':'<<c.bass<<':'<<int(c.quality)<<':'<<c.intervals<<':'<<c.colorMask
           <<':'<<c.exactIntervals<<'@'<<e.startQN-anchor<<'+'<<e.durationQN<<'^'<<e.topVoice.value_or(-1);
    }
    return out.str();
}
std::string sourceFingerprint(const ProgressionTemplate& item) {
    const auto realized=preview::buildTemplateSequence(item,{PitchClass::C,item.mode},preview::defaultTempoBPM);
    if(!realized)throw std::invalid_argument("invalid source music: "+realized.error);
    const auto projection=library::canonicalMusicalFingerprint(item);
    // Length prefix prevents a source projection from aliasing the realization suffix.
    return "source-v1:"+std::to_string(projection.size())+":"+projection+":"+
        performanceFingerprint(realized.sequence,item.meterNumerator,item.meterDenominator);
}
}
