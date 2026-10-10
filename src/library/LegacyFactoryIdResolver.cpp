#include "LegacyFactoryIdResolver.h"
#include <algorithm>
#include <charconv>
#include <stdexcept>
#include <sstream>
#include <locale>
#include <unordered_map>

namespace harmony::library {
std::string canonicalFactoryId(std::string_view id) {
    for(const auto& m:legacyFactoryIds)if(m.requested==id)return std::string(m.canonical);
    return std::string(id);
}
FactoryReference resolveFactoryId(std::string_view requested,std::span<const ProgressionTemplate> entries) {
    FactoryReference r{std::string(requested),std::string(requested)};
    const auto find=[&](std::string_view id)->const ProgressionTemplate* {
        const auto it=std::find_if(entries.begin(),entries.end(),[&](const auto& t){return t.id==id;});
        return it==entries.end()?nullptr:&*it;
    };
    r.entry=find(requested);if(r.entry)return r;
    r.resolved=canonicalFactoryId(requested);r.entry=find(r.resolved);return r;
}
bool validateFactoryRedirectTargets(std::span<const ProgressionTemplate> entries,std::string& error) {
    for(const auto& m:legacyFactoryIds) {
        if(std::any_of(entries.begin(),entries.end(),[&](const auto& t){return t.id==m.requested;})) {error="V3 contains duplicate legacy row: "+std::string(m.requested);return false;}
        if(std::none_of(entries.begin(),entries.end(),[&](const auto& t){return t.id==m.canonical;})) {error="missing canonical target: "+std::string(m.canonical);return false;}
    }
    return true;
}
std::string canonicalMusicalFingerprint(const ProgressionTemplate& item) {
    std::ostringstream out;out.imbue(std::locale::classic());
    out<<static_cast<int>(item.mode)<<':'<<item.meterNumerator<<'/'<<item.meterDenominator;
    const auto degree=[&](const std::optional<ScaleDegree>& value) {
        if(value)out<<value->degree<<','<<value->alteration;else out<<'-';
    };
    for(const auto& event:item.full) {
        out<<'|';degree(event.degree);out<<':'<<static_cast<int>(event.quality)<<':';
        degree(event.target);
        ChordEvent base;base.root=PitchClass::C;base.quality=event.quality;base.name="C";
        const auto intervals=event.intervalMask?event.intervalMask:normalizeChord(base).intervalMask;
        out<<':'<<(event.roles&flag(Role::Borrowed))<<':'<<event.bassInterval.value_or(0)
           <<':'<<(intervals|event.colorMask)<<':'<<event.colorMask<<':';
        // Exact relative QN durations retain meaningful harmonic rhythm; labels,
        // style, intent, cadence and editorial/structural weights are excluded.
        if(event.durationQN)out<<std::hexfloat<<*event.durationQN<<std::defaultfloat;else out<<"open";
    }
    return out.str();
}
void mergeFactoryMetadata(ProgressionTemplate& target,const ProgressionTemplate& source) {
    const auto add=[](auto& values,const auto& value) {
        if(!value.empty()&&std::find(values.begin(),values.end(),value)==values.end())values.push_back(value);
    };
    for(const auto& value:source.aliases)add(target.aliases,value);
    for(const auto& value:{source.id,source.name,source.nameZh,source.nameEn})add(target.aliases,value);
    for(const auto& value:source.tags)add(target.tags,value);
    for(const auto& value:source.builtInTags)add(target.builtInTags,value);
    for(const auto& value:source.techniques)add(target.techniques,value);
    target.styles|=source.styles;
    for(const auto& [style,weight]:source.styleWeights) {
        auto it=std::find_if(target.styleWeights.begin(),target.styleWeights.end(),
                           [&](const auto& value){return value.first==style;});
        if(it==target.styleWeights.end())target.styleWeights.emplace_back(style,weight);
        else it->second=std::max(it->second,weight);
    }
    std::sort(target.styleWeights.begin(),target.styleWeights.end(),
              [](const auto& a,const auto& b){return a.first<b.first;});
    target.secondaryIntents|=source.secondaryIntents|(1u<<static_cast<unsigned>(source.intent));
    target.secondaryIntents&=~(1u<<static_cast<unsigned>(target.intent));
    target.loopable=target.loopable||source.loopable;
}
std::vector<ProgressionTemplate> canonicalFactoryEntries(std::vector<ProgressionTemplate> entries) {
    for(const auto& m:legacyFactoryIds) {
        const auto source=std::find_if(entries.begin(),entries.end(),[&](const auto& t){return t.id==m.requested;});
        const auto target=std::find_if(entries.begin(),entries.end(),[&](const auto& t){return t.id==m.canonical;});
        if(target==entries.end())throw std::runtime_error("missing canonical target: "+std::string(m.canonical));
        if(source==entries.end())continue;
        if(canonicalMusicalFingerprint(*source)!=canonicalMusicalFingerprint(*target))
            throw std::runtime_error("redirect musical mismatch: "+std::string(m.requested));
        mergeFactoryMetadata(*target,*source);
        entries.erase(source);
    }
    std::unordered_map<std::string,std::size_t> seen;
    std::vector<ProgressionTemplate> unique;
    for(auto& item:entries) {
        const auto key=canonicalMusicalFingerprint(item);
        const auto [it,inserted]=seen.emplace(key,unique.size());
        if(inserted)unique.push_back(std::move(item));
        else mergeFactoryMetadata(unique[it->second],item);
    }
    std::string error;if(!validateFactoryRedirectTargets(unique,error))throw std::runtime_error(error);
    return unique;
}
std::string resolveFingerprint(std::string_view value,int version) {
    if(version<3)return std::string(value);
    std::size_t at{};
    for(int i=0;i<4;++i){const auto end=value.find(':',at);if(end==value.npos)return std::string(value);at=end+1;}
    const auto colon=value.find(':',at);if(colon==value.npos)return std::string(value);
    std::size_t length{};const auto parsed=std::from_chars(value.data()+at,value.data()+colon,length);
    if(parsed.ec!=std::errc{}||parsed.ptr!=value.data()+colon||length>value.size()-colon-1)return std::string(value);
    const auto start=colon+1;const auto id=value.substr(start,length);const auto resolved=canonicalFactoryId(id);
    return std::string(value.substr(0,at))+std::to_string(resolved.size())+":"+resolved+std::string(value.substr(start+length));
}
} // namespace harmony::library
