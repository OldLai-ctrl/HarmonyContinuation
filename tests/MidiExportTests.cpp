#include "demo/DemoScenario.h"
#include "library/ProgressionLibrary.h"
#include "midi/StandardMidiFileWriter.h"
#include "preview/PreviewSynth.h"
#include "snapshot/RecommendationSnapshot.h"
#include "session/ProductServices.h"
#include <algorithm>
#include <array>
#include <filesystem>
#include <iostream>
#include <map>
#include <stdexcept>
#include <tuple>

namespace {
using namespace harmony;
int checks{};
void check(bool condition,const char* name) {if(!condition)throw std::runtime_error(name);++checks;}
struct ParsedEvent {std::int64_t tick{}; int status{},type{};std::vector<std::uint8_t> data;};
struct ParsedMidi {int format{},tracks{},ppq{};std::vector<std::vector<ParsedEvent>> events;};
struct Reader {
    const std::vector<std::uint8_t>& bytes;std::size_t at{};
    std::uint8_t u8() {if(at>=bytes.size())throw std::runtime_error("truncated MIDI");return bytes[at++];}
    std::uint16_t be16(){return static_cast<std::uint16_t>((u8()<<8)|u8());}
    std::uint32_t be32(){return (static_cast<std::uint32_t>(be16())<<16)|be16();}
    std::uint32_t vlq(){std::uint32_t n{};for(int i=0;i<4;++i){const auto b=u8();n=(n<<7)|(b&0x7f);if(!(b&0x80))return n;}throw std::runtime_error("VLQ overflow");}
    void magic(const char* expected){for(int i=0;i<4;++i)if(u8()!=static_cast<std::uint8_t>(expected[i]))throw std::runtime_error("wrong MIDI chunk");}
};
ParsedMidi parse(const std::vector<std::uint8_t>& bytes) {
    Reader r{bytes}; ParsedMidi result;
    r.magic("MThd");check(r.be32()==6,"header size");
    result.format=r.be16();result.tracks=r.be16();result.ppq=r.be16();
    for(int track=0;track<result.tracks;++track) {
        r.magic("MTrk");const auto length=r.be32();const auto end=r.at+length;
        if(end>bytes.size())throw std::runtime_error("track overrun");
        std::int64_t tick{};std::vector<ParsedEvent> events;
        while(r.at<end) {
            tick+=r.vlq();const int status=r.u8();
            ParsedEvent e;e.tick=tick;e.status=status;
            if(status==0xff) {
                e.type=r.u8();const auto count=r.vlq();
                for(std::uint32_t i=0;i<count;++i)e.data.push_back(r.u8());
            } else if((status&0xf0)==0x80||(status&0xf0)==0x90) {
                e.data={r.u8(),r.u8()};
            } else throw std::runtime_error("unsupported test MIDI event");
            events.push_back(std::move(e));
        }
        if(r.at!=end)throw std::runtime_error("bad track size");
        result.events.push_back(std::move(events));
    }
    if(r.at!=bytes.size())throw std::runtime_error("trailing MIDI bytes");
    return result;
}
ChordEvent chord(const char* name,double start,std::optional<double> duration) {
    ChordEvent c;c.name=name;c.startQN=start;c.durationQN=duration;c.openEnded=!duration;return c;
}
std::vector<int> pitches(const midi::ExportSequence& sequence,std::size_t index) {
    std::vector<int> result;
    const double start=sequence.markers[index].qn;
    for(const auto& note:sequence.notes)if(note.startQN==start)result.push_back(note.midiNote%12);
    std::sort(result.begin(),result.end());return result;
}
bool contains(const std::vector<int>& values,int pc){return std::find(values.begin(),values.end(),pc)!=values.end();}
}
int main() {
    try {
        ImportedProgressionSession input;
        check(input.replace({chord("C",32,4),chord("Am",36,2),chord("Dm",38,{})},TimelineCoordinateMode::AbsoluteProjectQN),"input");
        ContinuationCandidate candidate;candidate.id="test";candidate.primaryTemplate="source";
        candidate.intent=PhraseIntent::Resolve;candidate.key={PitchClass::C,Mode::Major};
        candidate.rankingScore=84;candidate.suggestedCurrentChordDurationQN=2;
        candidate.continuation={{"G",2},{"C",4}};
        const auto preview=preview::buildSequence(input,&candidate,120);
        check(preview && preview.sequence.totalQN==14,"golden preview");
        const auto clip=midi::buildClip(preview.sequence,midi::ArrangementMode::VoiceLed,midi::ExportScope::FullPhrase,{4,4},candidate.key,candidate.intent);
        check(clip && clip.sequence.chordCount==5 && clip.sequence.boundaryQN==8,"golden clip");
        const auto written=midi::writeToMemory(clip.sequence);
        check(written && written.bytes.size()>100,"write memory");
        const auto smf=parse(written.bytes);
        check(smf.format==1&&smf.tracks==2&&smf.ppq==480,"format1 header");
        check(smf.events[0].back().tick==6720&&smf.events[1].back().tick==6720,"14 QN final tick");
        std::vector<std::int64_t> labels;std::vector<std::string> chordTexts;int tempo{},meter{},key{},intent{},boundary{};
        for(const auto& e:smf.events[0])if(e.status==0xff) {
            if(e.type==0x51){check(e.data==std::vector<std::uint8_t>({0x07,0xa1,0x20}),"120 BPM tempo bytes");++tempo;}
            else if(e.type==0x58){check(e.data==std::vector<std::uint8_t>({4,2,24,8}),"4/4 meter bytes");++meter;}
            else if(e.type==0x59){check(e.data==std::vector<std::uint8_t>({0,0}),"C major key bytes");++key;}
            else if(e.type==0x01){const std::string text(e.data.begin(),e.data.end());
                if(text.starts_with("HarmonyContinuation Intent"))++intent;
                else {labels.push_back(e.tick);chordTexts.push_back(text);}}
            else if(e.type==0x06){check(e.tick==3840,"boundary tick");++boundary;}
        }
        check(tempo==1&&meter==1&&key==1&&intent==1&&boundary==1,"conductor metadata counts");
        check(labels==std::vector<std::int64_t>({0,1920,2880,3840,4800}),"five chord labels and timing");
        check(chordTexts==std::vector<std::string>({"C","Am","Dm","G","C"}),"chord label text");
        std::map<int,int> active;
        std::map<std::pair<std::int64_t,int>,std::vector<int>> sameTick;
        for(const auto& e:smf.events[1])if(e.status!=0xff) {
            const int pitch=e.data[0];check(pitch>=0&&pitch<=127,"MIDI range");
            sameTick[{e.tick,pitch}].push_back((e.status&0xf0)==0x80?0:1);
            if((e.status&0xf0)==0x80) {check(active[pitch]>0,"note off matched");--active[pitch];}
            else ++active[pitch];
        }
        check(std::all_of(active.begin(),active.end(),[](auto p){return p.second==0;}),"no stuck notes");
        for(const auto& [position,order]:sameTick)if(order.size()>1 &&
            std::find(order.begin(),order.end(),0)!=order.end() && std::find(order.begin(),order.end(),1)!=order.end())
            check(order.front()==0,"note off before same-tick note on");
        preview::PreviewSynth synth;synth.prepare(48000);check(synth.start(preview.sequence),"preview schedule");
        for(std::size_t i=0;i<preview.sequence.events.size();++i) {
            std::vector<int> exported;
            for(const auto& n:clip.sequence.notes)if(n.startQN==preview.sequence.events[i].startQN)exported.push_back(n.midiNote);
            const auto& v=synth.schedule()[i].voicing;
            std::vector<int> voiced{v.bass};for(int j=0;j<v.upperCount;++j)voiced.push_back(v.upper[j]);
            std::sort(exported.begin(),exported.end());std::sort(voiced.begin(),voiced.end());
            check(exported==voiced,"voice-led equals preview pitches");
            check(midi::qnToTicks(preview.sequence.events[i].startQN,480)==labels[i],"preview and MIDI boundary agree");
        }
        check(midi::writeToMemory(clip.sequence).bytes==written.bytes,"deterministic bytes");
        auto current=midi::buildClip(preview.sequence,midi::ArrangementMode::BlockChords,midi::ExportScope::CurrentOnly,{4,4});
        auto suffix=midi::buildClip(preview.sequence,midi::ArrangementMode::VoiceLed,midi::ExportScope::ContinuationOnly,{4,4});
        check(current && current.sequence.chordCount==3&&!current.sequence.boundaryQN,"current scope");
        check(suffix && suffix.sequence.chordCount==2&&suffix.sequence.boundaryQN==0&&suffix.sequence.markers.front().qn==0,"suffix scope relative zero");
        check(midi::qnToTicks(0.25,480)==120&&midi::qnToTicks(0.5,480)==240&&midi::qnToTicks(1.5,480)==720,"fractional QN ticks");
        const auto phraseOnly=preview::buildSequence(input,nullptr,120);
        check(phraseOnly&&phraseOnly.sequence.totalQN==9,"current-only OPEN fallback");
        auto invalid=clip.sequence;invalid.notes[0].midiNote=128;
        check(!midi::writeToMemory(invalid),"invalid pitch rejected");
        check(input.replace({chord("C",32,8),chord("Am",40,3),chord("A7",43,1),chord("Dm",44,{})},TimelineCoordinateMode::AbsoluteProjectQN),"long existing input");
        const auto kept=preview::buildSequence(input,&candidate,120);
        check(kept && kept.sequence.events[0].durationQN==8&&kept.sequence.events[1].durationQN==3&&
            kept.sequence.events[2].durationQN==1,"existing durations preserved");
        const auto longClip=midi::buildClip(kept.sequence,midi::ArrangementMode::BlockChords,midi::ExportScope::FullPhrase,{4,4});
        check(longClip && longClip.sequence.markers[1].qn==8&&longClip.sequence.markers[2].qn==11&&
            longClip.sequence.markers[3].qn==12,"existing MIDI starts preserved");
        auto testBlock=[&](const char* label,std::initializer_list<int> required,int bass=-1){
            ImportedProgressionSession one;check(one.replace({chord(label,0,{})},TimelineCoordinateMode::RelativeToSelection),"single chord input");
            const auto p=preview::buildSequence(one,nullptr,120);
            const auto b=midi::buildClip(p.sequence,midi::ArrangementMode::BlockChords,midi::ExportScope::CurrentOnly,{4,4});
            check(static_cast<bool>(b),"block chord build");const auto values=pitches(b.sequence,0);
            for(int pc:required)check(contains(values,pc),"block chord identity");
            if(bass>=0)check(b.sequence.notes.front().midiNote%12==bass,"block slash bass");
        };
        testBlock("C",{0,4,7});testBlock("Am",{9,0,4});testBlock("G7",{7,11,2,5});
        testBlock("Bm7b5",{11,2,5,9});testBlock("C#dim7",{1,4,7,10});testBlock("C/E",{0,4,7},4);
        MatchResult match;match.templateId="source";match.templateName="Source";match.similarity=0.9f;
        match.templateLabels={"C","Am"};match.alignmentTrace.push_back({0,0,AlignmentOp::Match});
        const auto snap=snapshot::capture(input,candidate,{match},120,4,4,candidate.key,Style::Pop,PhraseIntent::Resolve);
        const auto json=snapshot::serialize(snap);const auto decoded=snapshot::deserialize(json);
        check(decoded && decoded.value.schemaVersion==1&&decoded.value.candidate.continuation.size()==2&&
            decoded.value.match&&decoded.value.match->alignmentTrace.size()==1,"snapshot round trip");
        auto future=json;const auto pos=future.find("\"schemaVersion\":1");check(pos!=std::string::npos,"version field present");
        future[pos+16]='2';check(snapshot::deserialize(future).error=="UnsupportedVersion","future snapshot rejected");
        const auto path=std::filesystem::path(HC_TEST_OUTPUT_DIR)/"midi-test-snapshot.hcrec.json";
        std::string error;check(snapshot::saveFile(snap,path,error),"snapshot file save");
        check(static_cast<bool>(snapshot::loadFile(path)),"snapshot file load");std::filesystem::remove(path);
        const auto factory=library::loadFactory(HC_FACTORY_DB_PATH);
        check(factory&&factory.templates.size()==161,"factory unchanged");
        const auto& factoryItem=factory.templates.front();
        const KeySignature factoryKey{factoryItem.mode==Mode::Major?PitchClass::C:PitchClass::A,factoryItem.mode};
        const auto factoryPreview=midi::previewFromTemplate(factoryItem,factoryKey,120);
        check(factoryPreview&&factoryPreview.sequence.events.size()==factoryItem.full.size(),"factory browser preview pipeline");
        const auto factoryClip=midi::buildClip(factoryPreview.sequence,midi::ArrangementMode::VoiceLed,
            midi::ExportScope::CurrentOnly,{factoryItem.meterNumerator,factoryItem.meterDenominator},factoryKey,factoryItem.intent);
        check(factoryClip&&midi::writeToMemory(factoryClip.sequence),"factory browser MIDI pipeline");
        for(const auto& item:factory.templates) {
            const KeySignature itemKey{item.mode==Mode::Major?PitchClass::C:PitchClass::A,item.mode};
            const auto itemPreview=midi::previewFromTemplate(item,itemKey,120);
            if(!itemPreview)throw std::runtime_error("factory preview: "+item.id+": "+itemPreview.error);
            const auto itemClip=midi::buildClip(itemPreview.sequence,midi::ArrangementMode::VoiceLed,
                midi::ExportScope::CurrentOnly,{item.meterNumerator,item.meterDenominator},itemKey,item.intent);
            if(!itemClip)throw std::runtime_error("factory clip: "+item.id+": "+itemClip.error);
            const auto itemMidi=midi::writeToMemory(itemClip.sequence);
            if(!itemMidi)throw std::runtime_error("factory MIDI: "+item.id+": "+itemMidi.error);
            const auto parsedItem=parse(itemMidi.bytes);
            check(parsedItem.format==1&&parsedItem.events[1].back().tick==
                midi::qnToTicks(itemPreview.sequence.totalQN,480),"factory MIDI roundtrip");
        }
        ImportedProgressionSession userInput;
        check(userInput.replace({chord("C",0,4),chord("Am",4,2),chord("Dm",6,{})},TimelineCoordinateMode::RelativeToSelection),"user source");
        const auto userItem=session::makeUserProgression(userInput,candidate,{"Export test",Style::Pop,PhraseIntent::Resolve,{}});
        check(static_cast<bool>(userItem),"user template creation");
        const auto userPreview=midi::previewFromTemplate(userItem.item,candidate.key,120);
        check(userPreview&&userPreview.sequence.events.size()==userItem.item.full.size(),"user library preview pipeline");
        const auto userClip=midi::buildClip(userPreview.sequence,midi::ArrangementMode::VoiceLed,
            midi::ExportScope::CurrentOnly,{userItem.item.meterNumerator,userItem.item.meterDenominator},candidate.key,userItem.item.intent);
        check(userClip&&midi::writeToMemory(userClip.sequence),"user library MIDI pipeline");
        const CandidateIndex index(factory.templates);
        for(char letter='A';letter<='H';++letter) {
            const auto file=std::filesystem::path(HC_DEMO_FIXTURE_DIR)/(std::string("case_")+char(letter-'A'+'a')+".json");
            const auto loaded=demo::loadScenario(file);check(static_cast<bool>(loaded),"case loaded");
            AnalysisContext context;context.forcedKey=loaded.scenario.forcedKey;
            context.timeSigNumerator=loaded.scenario.meterNumerator;context.timeSigDenominator=loaded.scenario.meterDenominator;
            const auto rec=recommendContinuations(makeMatchQuery(loaded.scenario.chords,context),index,
                {loaded.scenario.style,loaded.scenario.intent});
            const ContinuationCandidate* selected{};
            for(const auto& group:rec.groups)if(!group.empty()&&!selected)selected=&group.front();
            check(selected!=nullptr,"case candidate");
            ImportedProgressionSession imported;check(imported.replace(loaded.scenario.chords,TimelineCoordinateMode::RelativeToSelection),"case import");
            const auto p=preview::buildSequence(imported,selected,loaded.scenario.tempo);check(static_cast<bool>(p),"case preview");
            for(const auto mode:{midi::ArrangementMode::BlockChords,midi::ArrangementMode::VoiceLed}) {
                const auto b=midi::buildClip(p.sequence,mode,midi::ExportScope::FullPhrase,
                    {loaded.scenario.meterNumerator,loaded.scenario.meterDenominator},selected->key,selected->intent);
                check(static_cast<bool>(b),"case clip");const auto bytes=midi::writeToMemory(b.sequence);
                check(static_cast<bool>(bytes),"case SMF");const auto parsed=parse(bytes.bytes);
                check(parsed.format==1&&parsed.events[1].back().tick==midi::qnToTicks(p.sequence.totalQN,480),"case parsed timing");
            }
        }
        std::cout<<checks<<" MIDI export checks passed\n";
    } catch(const std::exception& e){std::cerr<<"MidiExportTests: "<<e.what()<<'\n';return 1;}
}
