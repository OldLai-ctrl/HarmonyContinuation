#include "EnrichmentSnapshot.h"
#include "benchmark/BenchJson.h"
#include "preview/PreviewSequence.h"
#include "product/ProductVersion.h"
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace harmony::snapshot {
namespace {
using benchmark::json::Value;
Value chordOut(const ChordEvent& chord) {
    auto v=Value::map();
    v.object["name"]=chord.name;
    v.object["startQN"]=chord.startQN;
    v.object["durationQN"]=chord.durationQN?Value(*chord.durationQN):Value{};
    v.object["open"]=chord.openEnded;
    v.object["quality"]=static_cast<int>(chord.quality);
    v.object["root"]=chord.root?Value(static_cast<int>(*chord.root)):Value{};
    v.object["bass"]=chord.bass?Value(static_cast<int>(*chord.bass)):Value{};
    return v;
}
ChordEvent chordIn(const Value& v) {
    ChordEvent chord;
    chord.name=v.at("name").text();
    chord.startQN=v.at("startQN").finite();
    const auto& duration=v.at("durationQN");
    if(duration.type!=Value::Type::Null) chord.durationQN=duration.finite();
    chord.openEnded=v.at("open").boolean;
    chord.quality=static_cast<ChordQuality>(v.at("quality").integer(0,static_cast<int>(ChordQuality::Augmented)));
    const auto& root=v.at("root"),&bass=v.at("bass");
    if(root.type!=Value::Type::Null) chord.root=static_cast<PitchClass>(root.integer(0,11));
    if(bass.type!=Value::Type::Null) chord.bass=static_cast<PitchClass>(bass.integer(0,11));
    return chord;
}
Value chordsOut(const Progression& chords) {
    auto list=Value::list();
    for(const auto& chord:chords)list.array.push_back(chordOut(chord));
    return list;
}
Progression chordsIn(const Value& list) {
    Progression out;
    for(const auto& item:list.items())out.push_back(chordIn(item));
    return out;
}
void validate(const EnrichmentSnapshot& snap) {
    if(snap.schemaVersion!=EnrichmentSnapshot::currentSchemaVersion)
        throw std::runtime_error("UnsupportedVersion");
    if(snap.original.events.empty()||snap.original.events.size()>64||
       snap.candidate.progression.empty()||snap.candidate.progression.size()>64||
       snap.candidate.id.empty()||snap.candidate.operations.empty()||
       snap.meterNumerator<1||snap.meterNumerator>32||
       snap.meterDenominator<1||snap.meterDenominator>32||
       !std::isfinite(snap.tempoBPM)||snap.tempoBPM<20||snap.tempoBPM>400)
        throw std::runtime_error("invalid enrichment snapshot");
    ImportedProgressionSession transformed;
    if(!transformed.replace(snap.candidate.progression,TimelineCoordinateMode::RelativeToSelection))
        throw std::runtime_error("invalid transformed progression");
    const auto preview=preview::buildSequence(transformed,nullptr,snap.tempoBPM,snap.candidate.constraints);
    if(!preview)throw std::runtime_error(preview.error);
}
}
EnrichmentSnapshot captureEnrichment(const ImportedProgressionSession& imported,
    const enrichment::EnrichmentCandidate& candidate,double tempo,int numerator,int denominator,
    std::optional<KeySignature> key,std::optional<Style> style) {
    EnrichmentSnapshot snap;
    snap.productVersion=std::string(product::version);
    snap.original=imported;
    snap.candidate=candidate;
    snap.tempoBPM=preview::sanitizeTempo(tempo);
    snap.meterNumerator=numerator;snap.meterDenominator=denominator;
    snap.key=key;snap.style=style;
    if(!snap.original.events.empty()) {
        const auto anchor=snap.original.events.front().startQN;
        for(auto& chord:snap.original.events)chord.startQN-=anchor;
        for(auto& chord:snap.candidate.progression)chord.startQN-=anchor;
        for(auto& melody:snap.candidate.constraints.melody)melody.startQN-=anchor;
        snap.original.coordinateMode=TimelineCoordinateMode::RelativeToSelection;
    }
    validate(snap);
    return snap;
}
std::string serialize(const EnrichmentSnapshot& snap) {
    validate(snap);
    auto root=Value::map();
    root.object["schemaVersion"]=snap.schemaVersion;
    root.object["suggestionType"]="enrichment";
    root.object["productVersion"]=snap.productVersion;
    root.object["tempoBPM"]=snap.tempoBPM;
    auto meter=Value::list();meter.array.emplace_back(snap.meterNumerator);meter.array.emplace_back(snap.meterDenominator);
    root.object["meter"]=std::move(meter);
    auto key=Value::list();
    if(snap.key){key.array.emplace_back(static_cast<int>(snap.key->tonic));key.array.emplace_back(static_cast<int>(snap.key->mode));}
    root.object["key"]=std::move(key);
    root.object["style"]=snap.style?Value(static_cast<int>(*snap.style)):Value{};
    root.object["original"]=chordsOut(snap.original.events);
    auto candidate=Value::map();
    candidate.object["id"]=snap.candidate.id;
    candidate.object["group"]=static_cast<int>(snap.candidate.group);
    candidate.object["complexity"]=static_cast<int>(snap.candidate.complexity);
    candidate.object["score"]=snap.candidate.score;
    candidate.object["skeletonPreservation"]=snap.candidate.skeletonPreservation;
    candidate.object["styleCompatibility"]=snap.candidate.styleCompatibility;
    candidate.object["complexityScore"]=snap.candidate.complexityScore;
    candidate.object["fingerprint"]=snap.candidate.fingerprint;
    candidate.object["progression"]=chordsOut(snap.candidate.progression);
    candidate.object["melodyConstraints"]=encodeConstraints(snap.candidate.constraints);
    auto operations=Value::list();
    for(const auto& op:snap.candidate.operations){auto v=Value::map();
        v.object["type"]=static_cast<int>(op.type);
        v.object["technique"]=static_cast<int>(op.technique);
        v.object["sourceIndex"]=static_cast<int>(op.sourceIndex);
        v.object["before"]=op.before;v.object["after"]=op.after;v.object["reason"]=op.reason;
        operations.array.push_back(std::move(v));}
    candidate.object["operations"]=std::move(operations);
    root.object["candidate"]=std::move(candidate);
    return benchmark::json::dump(root);
}
EnrichmentDecodeResult deserializeEnrichment(std::string_view input) {
    EnrichmentDecodeResult result;
    try {
        const auto top=benchmark::json::parse(input);auto& snap=result.value;
        snap.schemaVersion=top.at("schemaVersion").integer(0,1000000);
        if(snap.schemaVersion!=EnrichmentSnapshot::currentSchemaVersion)
            throw std::runtime_error("UnsupportedVersion");
        if(top.at("suggestionType").text()!="enrichment")throw std::runtime_error("wrong suggestion type");
        snap.productVersion=top.at("productVersion").text();
        snap.tempoBPM=top.at("tempoBPM").finite();
        const auto& meter=top.at("meter").items();if(meter.size()!=2)throw std::runtime_error("invalid meter");
        snap.meterNumerator=meter[0].integer(1,32);snap.meterDenominator=meter[1].integer(1,32);
        const auto& key=top.at("key").items();if(!key.empty()){
            if(key.size()!=2)throw std::runtime_error("invalid key");
            snap.key=KeySignature{static_cast<PitchClass>(key[0].integer(0,11)),static_cast<Mode>(key[1].integer(0,1))};}
        const auto& style=top.at("style");if(style.type!=Value::Type::Null)
            snap.style=static_cast<Style>(style.integer(1,32));
        snap.original.events=chordsIn(top.at("original"));
        snap.original.coordinateMode=TimelineCoordinateMode::RelativeToSelection;
        const auto& value=top.at("candidate");auto& c=snap.candidate;
        c.id=value.at("id").text();
        c.group=static_cast<enrichment::Group>(value.at("group").integer(0,2));
        c.complexity=static_cast<enrichment::ComplexityLevel>(value.at("complexity").integer(0,2));
        c.score=static_cast<float>(value.at("score").finite());
        c.skeletonPreservation=static_cast<float>(value.at("skeletonPreservation").finite());
        c.styleCompatibility=static_cast<float>(value.at("styleCompatibility").finite());
        c.complexityScore=static_cast<float>(value.at("complexityScore").finite());
        c.fingerprint=value.at("fingerprint").text();
        c.progression=chordsIn(value.at("progression"));
        if(value.object.contains("melodyConstraints"))c.constraints=decodeConstraints(value.at("melodyConstraints").text());
        c.melodyCompatibility=evaluateMelody(c.progression,c.constraints);
        for(const auto& operation:value.at("operations").items()){
            enrichment::EnrichmentOperation op;
            op.type=static_cast<enrichment::OperationType>(operation.at("type").integer(0,5));
            op.technique=static_cast<enrichment::TechniqueID>(operation.at("technique").integer(0,12));
            op.sourceIndex=static_cast<std::size_t>(operation.at("sourceIndex").integer(0,64));
            op.before=operation.at("before").text();op.after=operation.at("after").text();
            op.reason=operation.at("reason").text();c.operations.push_back(std::move(op));}
        validate(snap);
    }catch(const std::exception& e){result.value={};result.error=e.what();}
    return result;
}
bool saveFile(const EnrichmentSnapshot& snap,const std::filesystem::path& path,std::string& error) {
    try{const auto json=serialize(snap);std::ofstream out(path,std::ios::binary|std::ios::trunc);
        if(!out)throw std::runtime_error("cannot open snapshot output");out<<json;
        if(!out)throw std::runtime_error("snapshot write failed");return true;
    }catch(const std::exception& e){error=e.what();return false;}
}
EnrichmentDecodeResult loadEnrichmentFile(const std::filesystem::path& path) {
    std::ifstream in(path,std::ios::binary);if(!in)return {{},"cannot open snapshot"};
    const std::string data(std::istreambuf_iterator<char>{in},{});return deserializeEnrichment(data);
}
} // namespace harmony::snapshot
