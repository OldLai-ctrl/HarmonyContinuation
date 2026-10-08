#include "LegacyFactoryIdResolver.h"
#include <algorithm>
#include <charconv>
#include <stdexcept>

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
std::vector<ProgressionTemplate> canonicalFactoryEntries(std::vector<ProgressionTemplate> entries) {
    for(const auto& m:legacyFactoryIds) {
        const auto source=std::find_if(entries.begin(),entries.end(),[&](const auto& t){return t.id==m.requested;});
        const auto target=std::find_if(entries.begin(),entries.end(),[&](const auto& t){return t.id==m.canonical;});
        if(target==entries.end())throw std::runtime_error("missing canonical target: "+std::string(m.canonical));
        if(source==entries.end())continue;
        if(source->mode!=target->mode||source->full.size()!=target->full.size())throw std::runtime_error("redirect musical mismatch");
        for(std::size_t i=0;i<source->full.size();++i) {
            const auto& a=source->full[i];const auto& b=target->full[i];
            if(a.degree.has_value()!=b.degree.has_value()||(a.degree&&(a.degree->degree!=b.degree->degree||a.degree->alteration!=b.degree->alteration))||
               a.quality!=b.quality||a.durationQN!=b.durationQN||a.bassInterval!=b.bassInterval||a.intervalMask!=b.intervalMask||a.colorMask!=b.colorMask)
                throw std::runtime_error("redirect musical mismatch: "+std::string(m.requested));
        }
        // Keep distinct historical names discoverable without duplicate music.
        target->aliases.push_back(source->nameZh);target->aliases.push_back(source->nameEn);
        entries.erase(source);
    }
    std::string error;if(!validateFactoryRedirectTargets(entries,error))throw std::runtime_error(error);
    return entries;
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
