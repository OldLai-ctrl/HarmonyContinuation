#include "RecommendationSnapshot.h"
#include "preview/PreviewSequence.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <locale>
#include <map>
#include <sstream>
#include <stdexcept>

namespace harmony::snapshot {
namespace {
struct Value {
    enum class Kind { Null, Boolean, Number, String, Array, Object } kind{Kind::Null};
    bool boolean{}; double number{}; std::string string;
    std::vector<Value> array; std::map<std::string,Value> object;
    const Value& at(const char* key) const {
        if (kind!=Kind::Object) throw std::runtime_error("expected object");
        const auto it=object.find(key);
        if (it==object.end()) throw std::runtime_error(std::string("missing snapshot field: ")+key);
        return it->second;
    }
    int integer(int min,int max) const {
        if (kind!=Kind::Number||!std::isfinite(number)||std::floor(number)!=number||number<min||number>max)
            throw std::runtime_error("invalid snapshot integer");
        return static_cast<int>(number);
    }
    double finite() const {
        if (kind!=Kind::Number||!std::isfinite(number)) throw std::runtime_error("invalid snapshot number");
        return number;
    }
    const std::string& text() const { if (kind!=Kind::String) throw std::runtime_error("expected snapshot string"); return string; }
    const std::vector<Value>& list() const { if (kind!=Kind::Array) throw std::runtime_error("expected snapshot array"); return array; }
    bool flag() const { if (kind!=Kind::Boolean) throw std::runtime_error("expected snapshot boolean"); return boolean; }
};
struct Parser {
    std::string_view input; std::size_t at{};
    void space() { while (at<input.size()&&(input[at]==' '||input[at]=='\n'||input[at]=='\r'||input[at]=='\t')) ++at; }
    bool take(char c) { space(); if (at<input.size()&&input[at]==c) {++at;return true;} return false; }
    void need(char c) { if (!take(c)) throw std::runtime_error("invalid snapshot JSON syntax"); }
    std::string readString() {
        need('"'); std::string result;
        while (at<input.size()) {
            const unsigned char c=static_cast<unsigned char>(input[at++]);
            if (c=='"') return result;
            if (c<0x20) throw std::runtime_error("invalid snapshot control character");
            if (c!='\\') {result.push_back(static_cast<char>(c));continue;}
            if (at==input.size()) break;
            const char escape=input[at++];
            switch (escape) {
                case '"': result+='"'; break; case '\\': result+='\\'; break;
                case '/': result+='/'; break; case 'n': result+='\n'; break;
                case 'r': result+='\r'; break; case 't': result+='\t'; break;
                case 'u': {
                    if (at+4>input.size()||input[at]!='0'||input[at+1]!='0')
                        throw std::runtime_error("unsupported snapshot unicode escape");
                    const auto hex=[](char digit)->int {
                        if(digit>='0'&&digit<='9')return digit-'0';
                        if(digit>='a'&&digit<='f')return digit-'a'+10;
                        if(digit>='A'&&digit<='F')return digit-'A'+10;
                        return -1;
                    };
                    const int high=hex(input[at+2]),low=hex(input[at+3]);
                    if(high<0||low<0)throw std::runtime_error("invalid snapshot unicode escape");
                    result.push_back(static_cast<char>((high<<4)|low));at+=4;break;
                }
                default: throw std::runtime_error("unsupported snapshot escape");
            }
        }
        throw std::runtime_error("unterminated snapshot string");
    }
    Value read(int depth=0) {
        if (depth>16) throw std::runtime_error("snapshot nesting too deep");
        space(); if (at==input.size()) throw std::runtime_error("truncated snapshot");
        Value value;
        if (input[at]=='{') {
            value.kind=Value::Kind::Object; ++at;
            if (take('}')) return value;
            do {
                const auto key=readString(); need(':');
                if (!value.object.emplace(key,read(depth+1)).second) throw std::runtime_error("duplicate snapshot field");
                if (value.object.size()>256) throw std::runtime_error("snapshot object too large");
                if (take('}')) return value;
                need(',');
            } while (true);
        }
        if (input[at]=='[') {
            value.kind=Value::Kind::Array; ++at;
            if (take(']')) return value;
            do {
                if (value.array.size()>=256) throw std::runtime_error("snapshot array too large");
                value.array.push_back(read(depth+1));
                if (take(']')) return value;
                need(',');
            } while (true);
        }
        if (input[at]=='"') { value.kind=Value::Kind::String; value.string=readString(); return value; }
        if (input.substr(at,4)=="null") { at+=4; return value; }
        if (input.substr(at,4)=="true") { at+=4; value.kind=Value::Kind::Boolean; value.boolean=true; return value; }
        if (input.substr(at,5)=="false") { at+=5; value.kind=Value::Kind::Boolean; return value; }
        const auto parsed=std::from_chars(input.data()+at,input.data()+input.size(),value.number);
        if (parsed.ec!=std::errc{}||parsed.ptr==input.data()+at||!std::isfinite(value.number))
            throw std::runtime_error("invalid snapshot number");
        at=static_cast<std::size_t>(parsed.ptr-input.data()); value.kind=Value::Kind::Number;
        return value;
    }
};
void quoted(std::ostream& out,std::string_view value) {
    out << '"';
    constexpr char hex[]="0123456789abcdef";
    for (unsigned char c:value) {
        if (c=='"'||c=='\\') {out << '\\' << static_cast<char>(c);continue;}
        if (c=='\n') {out << "\\n";continue;}
        if (c=='\r') {out << "\\r";continue;}
        if (c=='\t') {out << "\\t";continue;}
        if (c<0x20) {out << "\\u00" << hex[c>>4] << hex[c&15];continue;}
        out << static_cast<char>(c);
    }
    out << '"';
}
void optNumber(std::ostream& out,std::optional<double> value) { if (value) out<<*value; else out<<"null"; }
void optInt(std::ostream& out,std::optional<std::int32_t> value) { if (value) out<<*value; else out<<"null"; }
void optPitch(std::ostream& out,std::optional<PitchClass> value) { if (value) out<<static_cast<int>(*value); else out<<"null"; }
void optString(std::ostream& out,const std::optional<std::string>& value) { if (value) quoted(out,*value); else out<<"null"; }
std::optional<double> optionalDouble(const Value& value) {return value.kind==Value::Kind::Null?std::nullopt:std::optional(value.finite());}
std::optional<std::int32_t> optionalInt(const Value& value) {return value.kind==Value::Kind::Null?std::nullopt:std::optional(static_cast<std::int32_t>(value.integer(-100000,100000)));}
std::optional<PitchClass> optionalPitch(const Value& value) {return value.kind==Value::Kind::Null?std::nullopt:std::optional(static_cast<PitchClass>(value.integer(0,11)));}
std::optional<std::string> optionalString(const Value& value) {return value.kind==Value::Kind::Null?std::nullopt:std::optional(value.text());}
void keyOut(std::ostream& out,KeySignature key) {out << '[' << static_cast<int>(key.tonic) << ',' << static_cast<int>(key.mode) << ']';}
KeySignature keyIn(const Value& value) {
    const auto& pair=value.list(); if (pair.size()!=2) throw std::runtime_error("invalid snapshot key");
    return {static_cast<PitchClass>(pair[0].integer(0,11)),static_cast<Mode>(pair[1].integer(0,1))};
}
void validate(const RecommendationSnapshot& s) {
    if (s.schemaVersion!=RecommendationSnapshot::currentSchemaVersion) throw std::runtime_error("UnsupportedVersion");
    if (s.imported.events.empty()||s.imported.events.size()>64||s.candidate.continuation.size()>64||
        s.candidate.id.empty()||s.imported.events.front().startQN!=0||
        s.meterNumerator<1||s.meterNumerator>32||s.meterDenominator<1||s.meterDenominator>32||
        !std::isfinite(s.tempoBPM)||s.tempoBPM<20||s.tempoBPM>400||
        !std::isfinite(s.candidate.rankingScore)||s.candidate.rankingScore<0||s.candidate.rankingScore>100)
        throw std::runtime_error("invalid snapshot data");
    const auto preview=preview::buildSequence(s.imported,&s.candidate,s.tempoBPM);
    if (!preview) throw std::runtime_error(preview.error);
}
} // namespace
RecommendationSnapshot capture(const ImportedProgressionSession& imported,const ContinuationCandidate& candidate,
    const std::vector<MatchResult>& matches,double tempo,int meterNumerator,int meterDenominator,
    std::optional<KeySignature> key,std::optional<Style> style,std::optional<PhraseIntent> intent) {
    RecommendationSnapshot s; s.imported=imported; s.candidate=candidate;
    s.tempoBPM=preview::sanitizeTempo(tempo); s.meterNumerator=meterNumerator; s.meterDenominator=meterDenominator;
    s.key=key?key:std::optional(candidate.key); s.style=style; s.intent=intent?intent:std::optional(candidate.intent);
    if (!s.imported.events.empty()) {
        const double anchor=s.imported.events.front().startQN;
        for (auto& event:s.imported.events) event.startQN-=anchor;
        s.imported.coordinateMode=TimelineCoordinateMode::RelativeToSelection;
    }
    const auto match=std::find_if(matches.begin(),matches.end(),[&](const auto& m){return m.templateId==candidate.primaryTemplate;});
    if (match!=matches.end()) s.match=*match;
    validate(s);
    return s;
}
std::string serialize(const RecommendationSnapshot& s) {
    validate(s);
    std::ostringstream out; out.imbue(std::locale::classic()); out << std::setprecision(17);
    out << "{\"schemaVersion\":1,\"tempoBPM\":" << s.tempoBPM << ",\"meter\":[" << s.meterNumerator << ',' << s.meterDenominator
        << "],\"key\":";
    if (s.key) keyOut(out,*s.key); else out<<"null";
    out << ",\"style\":"; if (s.style) out<<static_cast<StyleFlags>(*s.style); else out<<"null";
    out << ",\"intent\":"; if (s.intent) out<<static_cast<int>(*s.intent); else out<<"null";
    out << ",\"existing\":[";
    for (std::size_t i=0;i<s.imported.events.size();++i) {
        if (i) out<<',';
        const auto& e=s.imported.events[i];
        out << "{\"name\":"; quoted(out,e.name);
        out << ",\"startQN\":" << e.startQN << ",\"durationQN\":"; optNumber(out,e.durationQN);
        out << ",\"open\":" << (e.openEnded?"true":"false") << ",\"quality\":" << static_cast<int>(e.quality)
            << ",\"source\":" << static_cast<int>(e.source) << ",\"root\":"; optPitch(out,e.root);
        out << ",\"bass\":"; optPitch(out,e.bass);
        out << ",\"keyNote\":"; optInt(out,e.keyNoteValue);
        out << ",\"bassNote\":"; optInt(out,e.bassNoteValue);
        out << ",\"mask\":"; optString(out,e.extensions.mask);
        out << ",\"pitches\":"; optString(out,e.extensions.pitches);
        out << '}';
    }
    const auto& c=s.candidate;
    out << "],\"candidate\":{\"id\":"; quoted(out,c.id);
    out << ",\"primaryTemplate\":"; quoted(out,c.primaryTemplate);
    out << ",\"intent\":" << static_cast<int>(c.intent) << ",\"key\":"; keyOut(out,c.key);
    out << ",\"cadence\":" << static_cast<int>(c.cadence) << ",\"styles\":" << c.styles
        << ",\"rankingScore\":" << c.rankingScore << ",\"matchSimilarity\":" << c.matchSimilarity
        << ",\"supportCount\":" << c.supportCount << ",\"suggestedCurrentQN\":";
    optNumber(out,c.suggestedCurrentChordDurationQN);
    out << ",\"sources\":[";
    for (std::size_t i=0;i<c.supportingTemplates.size();++i) { if (i) out<<','; quoted(out,c.supportingTemplates[i]); }
    out << "],\"continuation\":[";
    for (std::size_t i=0;i<c.continuation.size();++i) {
        if (i) out<<',';
        const auto& e=c.continuation[i];
        out << "{\"label\":"; quoted(out,e.label);
        out << ",\"durationQN\":" << e.durationQN << ",\"degree\":" << e.degree.degree
            << ",\"alteration\":" << e.degree.alteration << ",\"quality\":" << static_cast<int>(e.quality)
            << ",\"roles\":" << e.roles << '}';
    }
    out << "]},\"match\":";
    if (!s.match) out<<"null";
    else {
        const auto& m=*s.match;
        out<<"{\"templateId\":"; quoted(out,m.templateId);
        out<<",\"templateName\":"; quoted(out,m.templateName);
        out<<",\"similarity\":"<<m.similarity<<",\"skeleton\":"<<m.subScores.skeletonHarmony
           <<",\"full\":"<<m.subScores.fullHarmony<<",\"labels\":[";
        for (std::size_t i=0;i<m.templateLabels.size();++i) {if(i)out<<',';quoted(out,m.templateLabels[i]);}
        out<<"],\"trace\":[";
        for (std::size_t i=0;i<m.alignmentTrace.size();++i) {
            if(i)out<<',';
            const auto& step=m.alignmentTrace[i];
            out<<'['<<(step.queryIndex?static_cast<int>(*step.queryIndex):-1)<<','
               <<(step.templateIndex?static_cast<int>(*step.templateIndex):-1)<<','
               <<static_cast<int>(step.operation)<<']';
        }
        out<<"]}";
    }
    out << '}';
    return out.str();
}
DecodeResult deserialize(std::string_view input) {
    DecodeResult result;
    try {
        if (input.empty()||input.size()>1024*1024) throw std::runtime_error("snapshot size invalid");
        Parser p{input}; const auto top=p.read(); p.space(); if(p.at!=input.size()) throw std::runtime_error("trailing snapshot data");
        auto& s=result.value;
        s.schemaVersion=top.at("schemaVersion").integer(0,1000000);
        if (s.schemaVersion!=RecommendationSnapshot::currentSchemaVersion) throw std::runtime_error("UnsupportedVersion");
        s.tempoBPM=top.at("tempoBPM").finite();
        const auto& meter=top.at("meter").list(); if(meter.size()!=2)throw std::runtime_error("invalid meter");
        s.meterNumerator=meter[0].integer(1,32); s.meterDenominator=meter[1].integer(1,32);
        const auto& key=top.at("key"); if(key.kind!=Value::Kind::Null) s.key=keyIn(key);
        const auto& style=top.at("style"); if(style.kind!=Value::Kind::Null) {
            const auto flags=style.integer(1,32);
            if(flags&(flags-1))throw std::runtime_error("invalid snapshot style");
            s.style=static_cast<Style>(flags);
        }
        const auto& intent=top.at("intent"); if(intent.kind!=Value::Kind::Null) s.intent=static_cast<PhraseIntent>(intent.integer(0,4));
        for (const auto& item:top.at("existing").list()) {
            ChordEvent e; e.name=item.at("name").text(); e.startQN=item.at("startQN").finite();
            e.durationQN=optionalDouble(item.at("durationQN")); e.openEnded=item.at("open").flag();
            e.quality=static_cast<ChordQuality>(item.at("quality").integer(0,static_cast<int>(ChordQuality::Augmented)));
            e.source=static_cast<ChordSource>(item.at("source").integer(0,2));
            e.root=optionalPitch(item.at("root")); e.bass=optionalPitch(item.at("bass"));
            e.keyNoteValue=optionalInt(item.at("keyNote")); e.bassNoteValue=optionalInt(item.at("bassNote"));
            e.extensions.mask=optionalString(item.at("mask")); e.extensions.pitches=optionalString(item.at("pitches"));
            s.imported.events.push_back(std::move(e));
        }
        s.imported.coordinateMode=TimelineCoordinateMode::RelativeToSelection; s.imported.revision=1;
        const auto& candidate=top.at("candidate"); auto& c=s.candidate;
        c.id=candidate.at("id").text(); c.primaryTemplate=candidate.at("primaryTemplate").text();
        c.intent=static_cast<PhraseIntent>(candidate.at("intent").integer(0,4)); c.key=keyIn(candidate.at("key"));
        c.cadence=static_cast<CadenceType>(candidate.at("cadence").integer(0,static_cast<int>(CadenceType::Unknown)));
        c.styles=static_cast<StyleFlags>(candidate.at("styles").integer(0,63));
        c.rankingScore=static_cast<float>(candidate.at("rankingScore").finite());
        c.matchSimilarity=static_cast<float>(candidate.at("matchSimilarity").finite());
        c.supportCount=candidate.at("supportCount").integer(1,100000);
        c.suggestedCurrentChordDurationQN=optionalDouble(candidate.at("suggestedCurrentQN"));
        for(const auto& id:candidate.at("sources").list()) c.supportingTemplates.push_back(id.text());
        for(const auto& item:candidate.at("continuation").list()) {
            ConcreteChordEvent e; e.label=item.at("label").text(); e.durationQN=item.at("durationQN").finite();
            e.degree.degree=item.at("degree").integer(0,7); e.degree.alteration=item.at("alteration").integer(-12,12);
            e.quality=static_cast<ChordQuality>(item.at("quality").integer(0,static_cast<int>(ChordQuality::Augmented)));
            e.roles=static_cast<RoleFlags>(item.at("roles").integer(0,0x7fffffff));
            c.continuation.push_back(std::move(e));
        }
        const auto& match=top.at("match");
        if(match.kind!=Value::Kind::Null) {
            MatchResult m; m.templateId=match.at("templateId").text(); m.templateName=match.at("templateName").text();
            m.similarity=static_cast<float>(match.at("similarity").finite());
            m.subScores.skeletonHarmony=static_cast<float>(match.at("skeleton").finite());
            m.subScores.fullHarmony=static_cast<float>(match.at("full").finite());
            for(const auto& label:match.at("labels").list())m.templateLabels.push_back(label.text());
            for(const auto& item:match.at("trace").list()) {
                const auto& a=item.list(); if(a.size()!=3)throw std::runtime_error("invalid alignment");
                AlignmentStep step; const int qi=a[0].integer(-1,4095),ti=a[1].integer(-1,4095);
                if(qi>=0)step.queryIndex=static_cast<std::size_t>(qi);
                if(ti>=0)step.templateIndex=static_cast<std::size_t>(ti);
                step.operation=static_cast<AlignmentOp>(a[2].integer(0,3));
                m.alignmentTrace.push_back(std::move(step));
            }
            s.match=std::move(m);
        }
        validate(s);
    } catch(const std::exception& e) { result.value={}; result.error=e.what(); }
    return result;
}
bool saveFile(const RecommendationSnapshot& s,const std::filesystem::path& path,std::string& error) {
    try {
        const auto json=serialize(s);
        std::ofstream out(path,std::ios::binary|std::ios::trunc);
        if(!out)throw std::runtime_error("cannot open snapshot output");
        out.write(json.data(),static_cast<std::streamsize>(json.size()));
        if(!out)throw std::runtime_error("snapshot write failed");
        return true;
    } catch(const std::exception& e) {error=e.what();return false;}
}
DecodeResult loadFile(const std::filesystem::path& path) {
    std::ifstream in(path,std::ios::binary);
    if(!in)return {{},"cannot open snapshot"};
    const std::string data(std::istreambuf_iterator<char>{in},{});
    return deserialize(data);
}
} // namespace harmony::snapshot
