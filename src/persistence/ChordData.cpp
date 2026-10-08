#include "ChordData.h"
#include <limits>

namespace harmony::persistence {
using benchmark::json::Value;
namespace {
template<class T> Value optNumber(const std::optional<T>& v) { return v?Value(static_cast<double>(*v)):Value{}; }
Value optString(const std::optional<std::string>& v) { return v?Value(*v):Value{}; }
std::optional<std::string> textIn(const Value& v) { return v.type==Value::Type::Null?std::nullopt:std::optional(v.text()); }
std::optional<std::int32_t> intIn(const Value& v) {
    return v.type==Value::Type::Null?std::nullopt:std::optional(v.integer(std::numeric_limits<int>::min(),std::numeric_limits<int>::max()));
}
Value strings(const std::vector<std::string>& input) { auto v=Value::list();for(const auto& s:input)v.array.emplace_back(s);return v; }
void stringsIn(const Value& v,std::vector<std::string>& out) { out.clear();for(const auto& s:v.items())out.push_back(s.text()); }
Value degreeOut(const std::optional<ScaleDegree>& d) {
    if(!d)return {};auto v=Value::list();v.array.emplace_back(d->degree);v.array.emplace_back(d->alteration);return v;
}
std::optional<ScaleDegree> degreeIn(const Value& v) {
    if(v.type==Value::Type::Null)return {};const auto& a=v.items();if(a.size()!=2)throw std::runtime_error("invalid degree");
    return ScaleDegree{a[0].integer(1,7),a[1].integer(-12,12)};
}
}
Value chordOut(const ChordEvent& e) {
    auto v=Value::map();
    v.object["name"]=e.name;v.object["startQN"]=e.startQN;v.object["durationQN"]=optNumber(e.durationQN);
    v.object["open"]=e.openEnded;v.object["quality"]=static_cast<int>(e.quality);v.object["source"]=static_cast<int>(e.source);
    v.object["root"]=optNumber(e.root);v.object["bass"]=optNumber(e.bass);
    v.object["keyNote"]=optNumber(e.keyNoteValue);v.object["bassNote"]=optNumber(e.bassNoteValue);
    v.object["rawId"]=optString(e.rawId);v.object["rawName"]=optString(e.rawName);
    v.object["rawKeyNote"]=optString(e.rawKeyNote);v.object["rawBassNote"]=optString(e.rawBassNote);
    v.object["rawProjectTime"]=optString(e.rawProjectTime);v.object["rawTimeDomain"]=optString(e.rawTimeDomain);
    v.object["mask"]=optString(e.extensions.mask);v.object["pitches"]=optString(e.extensions.pitches);
    v.object["type"]=optString(e.extensions.type);v.object["color"]=optString(e.extensions.color);
    return v;
}
ChordEvent chordIn(const Value& v) {
    ChordEvent e;e.name=v.at("name").text();e.startQN=v.at("startQN").finite();
    if(const auto& d=v.at("durationQN");d.type!=Value::Type::Null)e.durationQN=d.finite();
    if(v.at("open").type!=Value::Type::Boolean)throw std::runtime_error("invalid open flag");
    e.openEnded=v.at("open").boolean;
    e.quality=static_cast<ChordQuality>(v.at("quality").integer(0,static_cast<int>(ChordQuality::Augmented)));
    if(const auto* s=v.find("source"))e.source=static_cast<ChordSource>(s->integer(0,2));
    for(const auto name:{"root","bass"})if(const auto& p=v.at(name);p.type!=Value::Type::Null) {
        const auto pitch=static_cast<PitchClass>(p.integer(0,11));if(std::string_view(name)=="root")e.root=pitch;else e.bass=pitch;
    }
    if(const auto* x=v.find("keyNote"))e.keyNoteValue=intIn(*x);
    if(const auto* x=v.find("bassNote"))e.bassNoteValue=intIn(*x);
    const auto read=[&](const char* key,std::optional<std::string>& to){if(const auto* x=v.find(key))to=textIn(*x);};
    read("rawId",e.rawId);read("rawName",e.rawName);read("rawKeyNote",e.rawKeyNote);read("rawBassNote",e.rawBassNote);
    read("rawProjectTime",e.rawProjectTime);read("rawTimeDomain",e.rawTimeDomain);
    read("mask",e.extensions.mask);read("pitches",e.extensions.pitches);read("type",e.extensions.type);read("color",e.extensions.color);
    if(e.name.empty()||e.name.size()>512||e.startQN<0||e.startQN>1e9||
       (e.durationQN&&(*e.durationQN<=0||*e.durationQN>128)))throw std::runtime_error("invalid persisted chord");
    return e;
}
std::string encodeChords(const Progression& input) {
    if(input.size()>64)throw std::runtime_error("too many persisted chords");
    auto v=Value::list();for(const auto& e:input){auto one=chordOut(e);(void)chordIn(one);v.array.push_back(std::move(one));}
    return benchmark::json::dump(v);
}
Progression decodeChords(std::string_view input) {
    const auto v=benchmark::json::parse(input);if(v.items().size()>64)throw std::runtime_error("too many persisted chords");
    Progression out;for(const auto& e:v.items())out.push_back(chordIn(e));return out;
}
std::string encodeTemplateData(const ProgressionTemplate& t) {
    auto v=Value::map();v.object["events"]=benchmark::json::parse(encodeChords(t.rawChords));
    auto key=Value::list();if(t.rawChordKey){key.array.emplace_back(static_cast<int>(t.rawChordKey->tonic));key.array.emplace_back(static_cast<int>(t.rawChordKey->mode));}
    v.object["key"]=std::move(key);auto full=Value::list();
    for(const auto& e:t.full) {
        auto one=Value::map();one.object["degree"]=degreeOut(e.degree);one.object["target"]=degreeOut(e.target);
        one.object["quality"]=static_cast<int>(e.quality);one.object["function"]=static_cast<int>(e.function);
        one.object["roles"]=static_cast<int>(e.roles);one.object["weight"]=e.structuralWeight;
        one.object["durationQN"]=optNumber(e.durationQN);one.object["sourceIndex"]=static_cast<int>(e.sourceIndex);
        one.object["bassInterval"]=optNumber(e.bassInterval);one.object["intervalMask"]=static_cast<int>(e.intervalMask);
        one.object["colorMask"]=static_cast<int>(e.colorMask);one.object["suffix"]=e.displaySuffix;
        full.array.push_back(std::move(one));
    }
    v.object["full"]=std::move(full);v.object["tags"]=strings(t.tags);v.object["aliases"]=strings(t.aliases);
    v.object["builtInTags"]=strings(t.builtInTags);v.object["techniques"]=strings(t.techniques);
    return benchmark::json::dump(v);
}
void decodeTemplateData(std::string_view input,ProgressionTemplate& t) {
    const auto v=benchmark::json::parse(input);t.rawChords=decodeChords(benchmark::json::dump(v.at("events")));
    const auto& key=v.at("key").items();t.rawChordKey.reset();
    if(!key.empty()){if(key.size()!=2)throw std::runtime_error("invalid chord key");t.rawChordKey=KeySignature{static_cast<PitchClass>(key[0].integer(0,11)),static_cast<Mode>(key[1].integer(0,1))};}
    if(!t.rawChords.empty()&&(t.rawChords.size()!=t.full.size()||!t.rawChordKey))throw std::runtime_error("raw/projection mismatch");
    const auto& full=v.at("full").items();if(full.size()!=t.full.size()||full.size()>64)throw std::runtime_error("invalid stored match events");
    for(std::size_t i=0;i<full.size();++i) {
        const auto& x=full[i];auto& e=t.full[i];e.degree=degreeIn(x.at("degree"));e.target=degreeIn(x.at("target"));
        e.quality=static_cast<ChordQuality>(x.at("quality").integer(0,static_cast<int>(ChordQuality::Augmented)));
        e.function=static_cast<HarmonicFunction>(x.at("function").integer(0,6));e.roles=x.at("roles").integer(0,2047);
        e.structuralWeight=static_cast<float>(x.at("weight").finite());e.sourceIndex=x.at("sourceIndex").integer(0,4095);
        const auto& d=x.at("durationQN");e.durationQN=d.type==Value::Type::Null?std::nullopt:std::optional(d.finite());
        const auto& b=x.at("bassInterval");e.bassInterval=b.type==Value::Type::Null?std::nullopt:std::optional(b.integer(0,11));
        e.intervalMask=static_cast<std::uint16_t>(x.at("intervalMask").integer(0,4095));e.colorMask=static_cast<std::uint16_t>(x.at("colorMask").integer(0,4095));
        e.displaySuffix=x.at("suffix").text();
        if(!std::isfinite(e.structuralWeight)||e.structuralWeight<0||e.structuralWeight>1||
           (e.durationQN&&(*e.durationQN<=0||*e.durationQN>128))||e.displaySuffix.size()>64)throw std::runtime_error("invalid realization metadata");
    }
    stringsIn(v.at("tags"),t.tags);stringsIn(v.at("aliases"),t.aliases);stringsIn(v.at("builtInTags"),t.builtInTags);stringsIn(v.at("techniques"),t.techniques);
}
} // namespace harmony::persistence
