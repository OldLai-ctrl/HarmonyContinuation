#include "generation/PhraseGenerator.h"
#include "ProgressionJson.h"
#include "library/ProgressionLibrary.h"
#include "midi/MidiImportWorkflow.h"
#include "midi/MidiWorkflow.h"
#include "benchmark/BenchJson.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
#include <tuple>
using namespace harmony;
using namespace harmony::generation;
namespace {
std::size_t checks{};
void check(bool pass,const std::string& what){++checks;if(!pass)throw std::runtime_error(what);}
unsigned mask(const ChordPitchSet& p){return p.intervals|p.colorMask;}
unsigned absolute(const ChordPitchSet& p){unsigned out{};for(int i=0;i<12;++i)if(mask(p)&(1u<<i))out|=1u<<((p.root+i)%12);return out|(1u<<p.bass);}
Progression input(std::initializer_list<const char*> names) {
    Progression out;for(const auto* name:names){ChordEvent e;e.name=name;e.startQN=4.*out.size();e.durationQN=4;e.openEnded=false;out.push_back(e);}
    out.back().openEnded=true;out.back().durationQN.reset();return out;
}
std::string fingerprint(const Progression& events) {
    std::ostringstream s;s<<std::setprecision(17);for(const auto& e:events){const auto p=chordPitches(e);s<<p.root<<'/'<<p.bass<<'/'<<mask(p)<<'@'<<e.startQN<<':'<<e.durationQN.value_or(0)<<':'<<e.openEnded<<';';}return s.str();
}
std::string json(const Progression& events) {
    std::ostringstream s;s<<std::setprecision(17)<<'[';bool first=true;
    constexpr const char* names[]{"C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B"};
    for(const auto& e:events){if(!first)s<<',';first=false;const auto p=chordPitches(e);std::string label=e.name;
        if(const auto slash=label.find('/');slash!=label.npos)label.resize(slash);
        if(p.bass!=p.root)label+="/"+std::string(names[p.bass]);
        std::ostringstream hex;hex<<"0x"<<std::hex<<mask(p);
        s<<"{\"name\":"<<std::quoted(label)<<",\"start\":"<<e.startQN<<",\"mask\":"<<std::quoted(hex.str());
        if(e.durationQN&&!e.openEnded)s<<",\"duration\":"<<*e.durationQN;s<<'}';}
    return s.str()+']';
}
Progression sourceInput(const ProgressionTemplate& source,const std::filesystem::path& file) {
    const auto key=source.rawChordKey.value_or(KeySignature{PitchClass::C,source.mode});const auto route=preview::templateRoute(source,key);check(bool(route),route.error);
    const auto seq=preview::buildSequence(route.imported,nullptr,120);check(bool(seq),seq.error);
    const auto clip=midi::buildClip(seq.sequence,midi::ArrangementMode::VoiceLed,midi::ExportScope::FullPhrase,{route.meterNumerator,route.meterDenominator},key,{});check(bool(clip),clip.error);
    midi::ExportConfig config;config.profile=midi::ExportProfile::DAWClip;const auto bytes=midi::writeToMemory(clip.sequence,config);check(bool(bytes),bytes.error);
    std::string error;check(midi::writeToFile(bytes.bytes,file,error),error);ImportedProgressionSession imported;
    check(midi::applyImport(midi::importFile(file,false),imported),"complete source MIDI reimport");return imported.events;
}
void validate(const GenerationRequest& request,const GenerationResult& result) {
    check(bool(result),result.error);check(!result.phrases.empty(),"no complete generated phrase");
    check(result.expanded<=request.maximumExpansions,"expansion budget");std::set<std::string> identities;
    for(const auto& p:result.phrases){check(p.complete&&p.hardConstraintsPassed,"complete/hard status");check(p.phrase.size()>=4&&p.phrase.size()<=8&&p.path.size()==p.phrase.size(),"whole-phrase length");
        check(p.id.starts_with("g13:")&&identities.insert(performanceIdentity(p)).second,"generated identity/dedup");
        double end=p.seamQN;for(std::size_t i=0;i<p.phrase.size();++i){const auto& e=p.phrase[i];const auto c=chordPitches(e);
            check(c.root>=0&&c.bass>=0&&mask(c)&&e.durationQN&&*e.durationQN>0&&!e.openEnded&&std::abs(e.startQN-end)<1e-7,"sound/bass/time continuity");
            check(fingerprint({e})==fingerprint({p.path[i].chord})&&!p.path[i].explanation.empty(),"generated path identity");end+=*e.durationQN;}
        for(std::size_t i=1;i<p.phrase.size();++i){const auto a=chordPitches(p.phrase[i-1]),b=chordPitches(p.phrase[i]);
            check(a.bass!=b.bass||absolute(a)!=absolute(b),"no adjacent sounding duplicate hidden by different chord names");}
        check(std::abs(end-p.seamQN-p.durationQN)<1e-7,"complete end boundary");
        const auto candidate=previewCandidate(p);check(candidate.primaryTemplate.empty()&&!candidate.baseQualified&&!candidate.reviewedEnding,"no Factory evidence spoofing");
        const auto seq=buildPerformance(request.input,p,120);check(bool(seq),seq.error);
        check(evaluateMelody(continuationProgression(request.input,candidate),request.constraints).hardSatisfied,"hard melody");
        const auto clip=midi::buildClip(seq.sequence,midi::ArrangementMode::VoiceLed,midi::ExportScope::FullPhrase,{4,4},candidate.key,candidate.intent);check(bool(clip),clip.error);
        const auto payload=midi::makePayload(clip.sequence,"G13");check(midi::writeToMemory(clip.sequence).bytes==payload.smfBytes,"Preview/Annotated MIDI identity");
        const auto notes=midi::readFromMemory(payload.smfBytes);check(bool(notes),notes.error);
        for(std::size_t i=0;i<p.phrase.size();++i){const double at=p.phrase[i].startQN-request.input.front().startQN+.25*(*p.phrase[i].durationQN);int lowest=128;
            for(const auto& n:notes.file.notes)if(n.startQN<=at&&n.startQN+n.durationQN>at)lowest=std::min(lowest,n.pitch);
            check(lowest==p.bassMidi[i],"actual MIDI register preserves the generated bass line");}
        if(p.intent==DevelopmentIntent::BassLine&&p.evaluation.directedBassSteps==p.phrase.size())for(std::size_t i=1;i<p.bassMidi.size();++i)
            check(p.bassMidi[i]!=p.bassMidi[i-1]&&std::abs(p.bassMidi[i]-p.bassMidi[i-1])<=2,"no octave wrap in continuous bass line");
    }
}
void verifyMedia(const std::filesystem::path& directory,std::size_t expected=6) {
    namespace j=benchmark::json;std::size_t files{};
    for(const auto& folder:std::filesystem::directory_iterator(directory)) {
        const auto metadata=folder.path()/"generation.json";if(!folder.is_directory()||!std::filesystem::exists(metadata))continue;
        std::ifstream input(metadata,std::ios::binary);const std::string text((std::istreambuf_iterator<char>(input)),{});const auto root=j::parse(text);
        check(root.at("source").string=="generated-no-factory","export source kind");
        const auto& existing=root.at("input").array;const double anchor=existing.front().at("startQN").number;
        for(const auto& phrase:root.at("phrases").array)if(const auto* file=phrase.find("wav")) {
            ++files;const auto annotated=midi::readFromFile(folder.path()/phrase.at("midiAnnotated").string);
            const auto drag=midi::readFromFile(folder.path()/phrase.at("midiDAWClip").string);check(bool(annotated)&&bool(drag),"exported MIDI read-back");
            const auto noteKey=[](const auto& n){return std::tuple(n.pitch,n.channel,n.startQN,n.durationQN);};
            auto a=annotated.file.notes,b=drag.file.notes;const auto order=[&](const auto& x,const auto& y){return noteKey(x)<noteKey(y);};std::sort(a.begin(),a.end(),order);std::sort(b.begin(),b.end(),order);
            check(a.size()==b.size()&&!a.empty(),"two MIDI representations contain notes");
            for(std::size_t i=0;i<a.size();++i)check(noteKey(a[i])==noteKey(b[i]),"Annotated/DAW actual note identity");
            const auto verifyChord=[&](const j::Value& chord,double duration,int bassMidi=-1) {
                const auto intervals=unsigned(chord.at("intervals").number)|unsigned(chord.at("colorMask").number);
                const int chordRoot=int(chord.at("root").number),bass=int(chord.at("bass").number);
                unsigned expected=1u<<bass;for(int pc=0;pc<12;++pc)if(intervals&(1u<<pc))expected|=1u<<((chordRoot+pc)%12);
                const double probe=chord.at("startQN").number-anchor+duration*.25;unsigned actual{};int lowest=128;
                for(const auto& n:a)if(n.startQN<=probe&&n.startQN+n.durationQN>probe){actual|=1u<<(n.pitch%12);lowest=std::min(lowest,n.pitch);}
                check(actual==expected&&lowest%12==bass&&(bassMidi<0||lowest==bassMidi),"exported actual sounding set and registered slash bass");
            };
            for(const auto& chord:existing){const auto& d=chord.at("durationQN");verifyChord(chord,d.type==j::Value::Type::Null?phrase.at("inputOpenHoldQN").number:d.number);}
            const auto& chords=phrase.at("chords").array;const auto& bassLine=phrase.at("bassMidi").array;check(chords.size()==bassLine.size(),"complete registered bass plan");
            for(std::size_t i=0;i<chords.size();++i)verifyChord(chords[i],chords[i].at("durationQN").number,int(bassLine[i].number));
            std::ifstream wave(folder.path()/file->string,std::ios::binary);unsigned char header[44]{};wave.read(reinterpret_cast<char*>(header),44);
            const auto integer=[&](int at){return unsigned(header[at])|(unsigned(header[at+1])<<8)|(unsigned(header[at+2])<<16)|(unsigned(header[at+3])<<24);};
            const auto frames=(std::filesystem::file_size(folder.path()/file->string)-44)/4;
            const auto expectedFrames=std::uint64_t(std::llround((phrase.at("seamQN").number+phrase.at("durationQN").number-anchor)*.5*48000))+7200;
            check(bool(wave)&&std::string(reinterpret_cast<char*>(header),4)=="RIFF"&&integer(24)==48000&&header[22]==2&&frames==expectedFrames,"WAV full timing and 150ms release tail");
        }
    }
    check(files==expected,"expected focused listening exports");std::cout<<files<<" WAV and "<<files*2<<" MIDI files; "<<checks<<" media read-back assertions PASS\n";
}
void verifyG131(const std::filesystem::path& file) {
    const auto prior=loadStylePrior(file);check(!prior.id.empty()&&!prior.qualityTransitions.empty(),"tonic-relative root/family prior loaded");
    GenerationRequest request;request.input=input({"F","G","Em","Am"});request.stylePrior=prior;request.resultsPerIntent=1;
    const auto original=fingerprint(request.input);std::set<std::string> music,plans;
    for(auto level:{VariationLevel::Conventional,VariationLevel::Rich,VariationLevel::Bold}){
        request.variation=level;const auto result=generate(request);validate(request,result);check(result.phrases.size()==1,"one normal first result per variation");
        const auto& phrase=result.phrases.front();check(phrase.plan.has_value(),"explicit prior-driven full phrase plan");const auto& plan=*phrase.plan;
        check(plan.priorId==prior.id&&plan.variation==level&&plan.slots.size()==phrase.phrase.size(),"plan provenance and event correspondence");
        check(plans.insert(plan.id).second&&music.insert(performanceIdentity(phrase)).second,"distinct preplanned variation and actual music");
        check(phrase.evaluation.outsideChords<=plan.outsideBudget&&phrase.durationQN==16.,"planned four-bar duration and actual outside budget");
        check(result.skeletonExpanded+result.bassExpanded<=4096&&result.expanded<=request.maximumExpansions,"separate bounded planning and realization search");
        std::size_t chromatic{};
        for(std::size_t i=0;i<plan.slots.size();++i){const auto& slot=plan.slots[i];const auto p=chordPitches(phrase.phrase[i]);
            check((p.root-int(phrase.originCenter.tonic)+12)%12==slot.rootDegree&&phrase.bassMidi[i]==slot.bassMidi&&*phrase.phrase[i].durationQN==slot.durationQN,"preplanned root, bass register and rhythm actually realized");
            check(slot.bassMidi>=36&&slot.bassMidi<=55,"planned bass remains in deliberate register");
            if(i)check(std::abs(phrase.bassMidi[i]-phrase.bassMidi[i-1])<=plan.maximumBassLeap,"bounded actual bass trajectory");
            if(slot.appliedDominant){++chromatic;check(i+1<plan.slots.size()&&p.quality==ChordQuality::Dominant7&&phrase.path[i].expectedRoot==std::optional(chordPitches(phrase.phrase[i+1]).root),"applied dominant has explicit immediately realized target");
                check((absolute(p)&~unsigned((1<<0)|(1<<2)|(1<<4)|(1<<5)|(1<<7)|(1<<9)|(1<<11)))!=0,"bold transformation actually contains a context-external tone");}
        }
        check(chromatic==(level==VariationLevel::Bold?1u:0u),"one prepared chromatic episode only in bold");
        const auto again=generate(request);check(again.phrases.size()==1&&again.phrases[0].id==phrase.id&&performanceIdentity(again.phrases[0])==performanceIdentity(phrase),"deterministic planned search and identity");
        auto metadata=phrase;metadata.plan->id="diagnostic-only";check(performanceIdentity(metadata)==performanceIdentity(phrase),"metadata does not alter sounding identity");
        std::cout<<name(level)<<" id="<<phrase.id<<" plan="<<plan.id<<" ms="<<result.elapsedMs<<" expanded="<<result.expanded<<" ";for(const auto& e:phrase.phrase)std::cout<<e.name<<'@'<<*e.durationQN<<' ';std::cout<<"PASS\n";
    }
    check(fingerprint(request.input)==original,"input identity including OPEN preserved");
    request.maximumExpansions=1;const auto capped=generate(request);check(capped.budgetExhausted&&capped.expanded==1,"planned search stops at exact expansion budget");
    request.maximumExpansions=16000;request.variation=VariationLevel::Conventional;
    MelodyConstraint melody;melody.startQN=16.;melody.durationQN=.5;melody.role=ConstraintRole::Present;melody.strictness=ConstraintStrictness::Hard;
    const auto unconstrained=generate(request);check(!unconstrained.phrases.empty(),"available grammar for hard-top fixture");
    const int topPitch=72+chordPitches(unconstrained.phrases.front().phrase.front()).root;
    melody.pitchClass=topPitch%12;melody.pitch=topPitch;melody.role=ConstraintRole::TopVoice;request.constraints.melody={melody};
    const auto feasible=generate(request);validate(request,feasible);const auto performance=buildPerformance(request.input,feasible.phrases.front());check(bool(performance),performance.error);
    const auto clip=midi::buildClip(performance.sequence,midi::ArrangementMode::VoiceLed,midi::ExportScope::FullPhrase,{4,4});check(bool(clip),clip.error);
    const auto exported=midi::readFromMemory(midi::writeToMemory(clip.sequence).bytes);check(bool(exported),exported.error);
    check(std::any_of(exported.file.notes.begin(),exported.file.notes.end(),[&](const auto& n){return n.pitch==topPitch&&std::abs(n.startQN-16.)<1e-7;}),"feasible hard top note actually exported");
    request.constraints.melody.clear();melody.pitch.reset();melody.role=ConstraintRole::Present;
    for(int pc=0;pc<12;++pc){melody.pitchClass=pc;request.constraints.melody.push_back(melody);}const auto impossible=generate(request);
    check(impossible.phrases.empty()&&impossible.constraintRejected>0,"impossible hard melody is rejected without relaxing grammar");
    request.constraints.melody.clear();request.stylePrior->transitions={{-1,0,0,1,1}};const auto unsupported=generate(request);
    check(!unsupported&&unsupported.phrases.empty(),"unsupported prior has no manually authored fallback");
    std::cout<<checks<<" G13.1 focused assertions PASS; only F-G-Em-Am checked\n";
}
}
int main(int argc,char** argv)try {
    if(argc==3&&std::string(argv[1])=="--g131"){verifyG131(argv[2]);return 0;}
    if(argc==3&&std::string(argv[1])=="--g131-media"){verifyMedia(argv[2],3);return 0;}
    if(argc==3&&std::string(argv[1])=="--media"){verifyMedia(argv[2]);return 0;}
    check(argc==3,"usage: PhraseGenerationTests frozen-factory.db output-dir");const std::filesystem::path output=argv[2];std::filesystem::create_directories(output/"inputs");
    const auto factory=library::loadFactory(argv[1]);check(bool(factory),factory.error);check(factory.templates.size()==1288,"frozen Factory V8");
    std::vector<std::pair<std::string,Progression>> cases{{"01-F-G-Em-Am",input({"F","G","Em","Am"})},{"02-F-G-Am",input({"F","G","Am"})},{"03-Chromatic-Bass",input({"Am","G#dim","G","D/F#"})}};
    for(const auto& [name,id]:std::vector<std::pair<std::string,std::string>>{{"04-G12-2","V7_04_061"},{"05-G12-8","V7_04_001"}}) {
        const auto found=std::find_if(factory.templates.begin(),factory.templates.end(),[&](const auto& t){return t.id==id;});check(found!=factory.templates.end(),"G12 D source");
        cases.emplace_back(name,sourceInput(*found,output/(name+"-D.mid")));
    }
    std::ofstream report(output/"generated-cases.tsv");report<<"case\tintent\tid\torigin\ttarget\tchords\tbass\tQN\tending\tdirected_run\toutside\tconnection\tdevelopment\tnovelty\tms\texpanded\n";
    for(const auto& [label,events]:cases) {
        const auto encoded=json(events);const auto decoded=dev::parseProgressionJson(encoded);check(bool(decoded),decoded.error);
        check(fingerprint(decoded.events)==fingerprint(events),"experimental input JSON preserves actual notes/bass/time");
        std::ofstream file(output/"inputs"/(label+".json"));file<<encoded;file.close();check(bool(file),"write fixed input");
        GenerationRequest request;request.input=events;request.context.timeSigNumerator=4;request.context.timeSigDenominator=4;
        const auto before=fingerprint(events);const auto result=generate(request);validate(request,result);check(fingerprint(events)==before,"input not mutated");
        std::set<DevelopmentIntent> intents;for(const auto& p:result.phrases){intents.insert(p.intent);std::string chords,bass;
            for(const auto& e:p.phrase){chords+=e.name+"@"+std::to_string(*e.durationQN)+" ";bass+=std::to_string(chordPitches(e).bass)+" ";}
            report<<label<<'\t'<<name(p.intent)<<'\t'<<p.id<<'\t'<<formatKey(p.originCenter)<<'\t'<<(p.targetCenter?formatKey(*p.targetCenter):"")<<'\t'<<chords<<'\t'<<bass<<'\t'<<p.durationQN<<'\t'<<phraseEndingName(p.ending)<<'\t'<<p.evaluation.directedBassSteps<<'\t'<<p.evaluation.outsideChords<<'\t'<<p.evaluation.connection<<'\t'<<p.evaluation.development<<'\t'<<p.evaluation.novelty<<'\t'<<result.elapsedMs<<'\t'<<result.expanded<<'\n';}
        report.flush();check(intents.size()==5,"not all five development intents produced a complete phrase: "+label);
        std::cout<<label<<" phrases="<<result.phrases.size()<<" expanded="<<result.expanded<<" ms="<<result.elapsedMs<<" PASS\n";
    }
    const auto base=cases.front().second;
    preview::Event ordinary{chordPitches("Cmaj7"),0,4,preview::Segment::Existing};ChordVoicer firstModel,secondModel;
    const auto oldVoice=firstModel.voice(ordinary.chord);const auto sameVoice=preview::voiceEvent(ordinary,secondModel);
    check(sameVoice&&sameVoice->bass==oldVoice.bass&&sameVoice->upper==oldVoice.upper,"absent register override preserves default voicing");
    ordinary.bassMidi=24;ChordVoicer explicitModel;const auto lowVoice=preview::voiceEvent(ordinary,explicitModel);check(lowVoice&&lowVoice->bass==24,"explicit performed bass register");
    ordinary.bassMidi=25;check(!preview::voiceEvent(ordinary,explicitModel),"bass register must retain pitch-class identity");
    for(auto kind:{RelationKind::Functional,RelationKind::VoiceLeading,RelationKind::Outside}) {
        SuccessorContext context;context.recent=base;context.center={PitchClass::C,Mode::Major};
        const auto next=successors(context,kind);check(!next.empty(),"independent operator empty");
        for(const auto& r:next)check(r.kind==kind&&chordPitches(r.chord).bass>=0&&!r.explanation.empty(),"operator evidence");
    }
    GenerationRequest repeat;repeat.input=cases[2].second;repeat.intents={DevelopmentIntent::BassLine};
    const auto a=generate(repeat),b=generate(repeat);check(a.phrases.size()==b.phrases.size()&&!a.phrases.empty(),"repeatable result count");
    for(std::size_t i=0;i<a.phrases.size();++i)check(a.phrases[i].id==b.phrases[i].id&&performanceIdentity(a.phrases[i])==performanceIdentity(b.phrases[i]),"deterministic search");
    GenerationRequest constrained;constrained.input=base;constrained.intents={DevelopmentIntent::Open};
    MelodyConstraint melody;melody.startQN=base.back().startQN+progressionOpenDuration(base);melody.durationQN=.5;melody.pitchClass=0;melody.pitch=72;melody.role=ConstraintRole::TopVoice;melody.strictness=ConstraintStrictness::Hard;
    constrained.constraints.melody={melody};const auto constrainedResult=generate(constrained);validate(constrained,constrainedResult);
    const auto constrainedPerformance=buildPerformance(base,constrainedResult.phrases.front());check(bool(constrainedPerformance),constrainedPerformance.error);
    const auto constrainedClip=midi::buildClip(constrainedPerformance.sequence,midi::ArrangementMode::VoiceLed,midi::ExportScope::FullPhrase,{4,4});check(bool(constrainedClip),constrainedClip.error);
    const auto actualNotes=midi::readFromMemory(midi::writeToMemory(constrainedClip.sequence).bytes);check(bool(actualNotes),actualNotes.error);
    check(std::any_of(actualNotes.file.notes.begin(),actualNotes.file.notes.end(),[&](const auto& n){return n.pitch==72&&std::abs(n.startQN-melody.startQN)<1e-7;}),"hard top note actually exported at generated seam");
    constrained.constraints.melody.clear();for(int pc=0;pc<12;++pc){melody.pitch.reset();melody.pitchClass=pc;melody.role=ConstraintRole::Present;constrained.constraints.melody.push_back(melody);}
    const auto impossible=generate(constrained);check(impossible.phrases.empty()&&impossible.constraintRejected>0,"impossible hard set must not be relaxed");
    GenerationRequest tiny;tiny.input=base;tiny.maximumExpansions=1;const auto capped=generate(tiny);check(capped.budgetExhausted&&capped.expanded==1,"hard budget stop");
    tiny.input.clear();check(!generate(tiny),"empty input rejection");tiny.input=base;tiny.input[1].startQN=0;check(!generate(tiny),"overlap rejection");
    std::cout<<checks<<" focused assertions PASS; no RCR or historical suite executed\n";return 0;
}catch(const std::exception& e){std::cerr<<"PhraseGenerationTests: "<<e.what()<<'\n';return 1;}
