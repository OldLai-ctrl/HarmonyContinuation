#include "midi/MidiImportWorkflow.h"
#include "midi/MidiWorkflow.h"
#include "session/PluginSessionState.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace harmony;
using namespace harmony::midi;
namespace {
using Bytes=std::vector<std::uint8_t>;
int fixtures{},roundTrips{},qualities{},checks{};
void check(bool okay,const std::string& message){if(!okay)throw std::runtime_error(message);++checks;}
bool near(double a,double b){return std::abs(a-b)<.003;}
void be(Bytes& out,std::uint32_t n,int bytes){for(int shift=(bytes-1)*8;shift>=0;shift-=8)out.push_back(static_cast<std::uint8_t>(n>>shift));}
Bytes smf(int format,std::vector<Bytes> tracks,int division=480) {
    Bytes out{'M','T','h','d'};be(out,6,4);be(out,format,2);be(out,static_cast<std::uint32_t>(tracks.size()),2);be(out,division,2);
    for(auto& track:tracks){out.insert(out.end(),{'M','T','r','k'});be(out,static_cast<std::uint32_t>(track.size()),4);out.insert(out.end(),track.begin(),track.end());}return out;
}
void vlq(Bytes& out,int n){Bytes reverse;reverse.push_back(n&127);while(n>>=7)reverse.push_back((n&127)|128);out.insert(out.end(),reverse.rbegin(),reverse.rend());}
void event(Bytes& out,int delta,std::initializer_list<std::uint8_t> data){vlq(out,delta);out.insert(out.end(),data);}
Bytes blockTrack(int channel=0) {
    Bytes out;for(int p:{60,64,67})event(out,0,{static_cast<std::uint8_t>(0x90|channel),static_cast<std::uint8_t>(p),80});
    bool first=true;for(int p:{60,64,67}){event(out,first?480:0,{static_cast<std::uint8_t>(0x80|channel),static_cast<std::uint8_t>(p),0});first=false;}
    event(out,0,{255,47,0});return out;
}
MidiFile notes(std::initializer_list<int> pitches,double duration=4) {
    MidiFile file;file.ppq=480;file.totalQN=duration;file.tracks.push_back({0,"Chords",pitches.size(),0});
    for(int p:pitches)file.notes.push_back({p,80,0,0,0,duration});return file;
}
Progression phrase(std::initializer_list<const char*> labels) {
    Progression out;double start{};for(const auto* label:labels){ChordEvent c;c.name=label;c.startQN=start;c.durationQN=1.+out.size()*.5;c.openEnded=false;start+=*c.durationQN;out.push_back(c);}return out;
}
void identity(const ChordEvent& expected,const ChordEvent& actual,bool strict,const std::string& context) {
    const auto a=chordPitches(expected),b=chordPitches(actual);
    check(a.root==b.root&&a.quality==b.quality,context+": root/quality "+expected.name+" -> "+actual.name);
    if(strict)check(a.intervals==b.intervals&&a.bass==b.bass,context+": pitch set/bass");
}
void roundTrip(const Progression& source,ArrangementMode mode,const std::string& name,const std::filesystem::path& output) {
    ImportedProgressionSession imported;check(imported.replace(source,TimelineCoordinateMode::RelativeToSelection),"round-trip input");
    const auto preview=preview::buildSequence(imported,nullptr,120);check(bool(preview),preview.error);
    const auto clip=buildClip(preview.sequence,mode,ExportScope::FullPhrase,{});check(bool(clip),clip.error);
    const auto written=writeToMemory(clip.sequence);check(bool(written),written.error);
    const auto path=output/(name+(mode==ArrangementMode::BlockChords?"_block.mid":"_voice.mid"));std::string error;
    check(writeToFile(written.bytes,path,error),error);const auto parsed=readFromFile(path);check(bool(parsed),parsed.error);
    const auto extracted=extractHarmony(parsed.file);check(bool(extracted),extracted.error);
    check(extracted.chords.size()==source.size(),name+": chord count "+std::to_string(extracted.chords.size()));
    for(std::size_t i=0;i<source.size();++i){identity(source[i],extracted.chords[i],mode==ArrangementMode::BlockChords,name);
        check(near(source[i].startQN,extracted.chords[i].startQN)&&near(*source[i].durationQN,*extracted.chords[i].durationQN),name+": rhythm");}
    ++roundTrips;
}
}
int main() {
    try {
        const auto output=std::filesystem::path(HC_TEST_OUTPUT_DIR)/"midi-import-fixtures";std::filesystem::create_directories(output);
        const auto run=[&](const char* name,auto body){body();++fixtures;std::cout<<"fixture PASS "<<name<<'\n';};
        run("Format 0 PPQ",[]{const auto r=readFromMemory(smf(0,{blockTrack()}));check(bool(r)&&r.file.format==0&&r.file.ppq==480&&r.file.notes.size()==3,"format0");check(near(r.file.notes[0].durationQN,1),"PPQ timing");});
        run("Format 1 meta and chord track",[]{Bytes meta{0,255,3,4,'T','e','s','t',0,255,81,3,7,161,32,0,255,88,4,3,2,24,8,0,255,127,2,1,2,0,240,2,1,247,0,255,47,0};
            const auto r=readFromMemory(smf(1,{meta,blockTrack()}));check(bool(r)&&r.file.tracks.size()==2&&r.file.notes[0].track==1,"format1 identity");check(r.file.tracks[0].name=="Test"&&near(r.file.tempos[0].bpm,120)&&r.file.meters[0].numerator==3,"metadata");});
        run("Running status velocity zero",[]{Bytes t;event(t,0,{144,60,80});event(t,0,{64,80});event(t,0,{67,80});event(t,480,{60,0});event(t,0,{64,0});event(t,0,{67,0});event(t,0,{255,47,0});const auto r=readFromMemory(smf(0,{t}));check(bool(r)&&r.file.notes.size()==3,"running status");});
        run("Overlap retrigger FIFO",[]{Bytes t;event(t,0,{144,60,80});event(t,120,{144,60,70});event(t,120,{128,60,0});event(t,120,{128,60,0});event(t,0,{255,47,0});const auto r=readFromMemory(smf(0,{t}));check(bool(r)&&r.file.notes.size()==2&&near(r.file.notes[0].durationQN,.5)&&near(r.file.notes[1].startQN,.25),"overlap pair");});
        run("Unclosed note warning",[]{Bytes t;event(t,0,{144,60,80});event(t,480,{255,47,0});const auto r=readFromMemory(smf(0,{t}));check(bool(r)&&r.file.notes.size()==1&&!r.file.warnings.empty(),"unclosed");});
        run("Unmatched and zero-length notes",[]{Bytes t;event(t,0,{128,62,0});event(t,0,{144,60,80});event(t,0,{128,60,0});event(t,0,{255,47,0});const auto r=readFromMemory(smf(0,{t}));check(bool(r)&&r.file.notes.empty()&&r.file.warnings.size()==2,"safe abnormal notes");});
        run("SMPTE unsupported",[]{check(readFromMemory(smf(0,{blockTrack()},0xe728)).status==ReadStatus::Unsupported,"SMPTE");});
        run("Format 2 unsupported",[]{check(readFromMemory(smf(2,{blockTrack()})).status==ReadStatus::Unsupported,"format2");});
        run("Truncated chunks",[]{auto bytes=smf(0,{blockTrack()});for(std::size_t i=0;i<bytes.size();++i)check(!readFromMemory(std::span(bytes).first(i)),"truncated offset "+std::to_string(i));});
        run("Invalid VLQ",[]{check(!readFromMemory(smf(0,{{128,128,128,128,0,255,47,0}})),"VLQ");});
        run("Running status after meta",[]{check(!readFromMemory(smf(0,{{0,144,60,80,0,255,1,0,0,64,80}})),"running cancelled");});
        run("Invalid channel data",[]{check(!readFromMemory(smf(0,{{0,144,60,255}})),"bad data");});
        run("Resource limits",[]{check(readFromMemory(Bytes(32*1024*1024+1)).status==ReadStatus::ResourceLimit,"byte limit");auto b=smf(1,{blockTrack()});b[10]=0;b[11]=129;check(readFromMemory(b).status==ReadStatus::ResourceLimit,"track limit");Bytes t;for(int i=0;i<257;++i)event(t,0,{144,60,80});check(readFromMemory(smf(0,{t})).status==ReadStatus::ResourceLimit,"overlap limit");});
        run("Drums parsed but ignored",[]{const auto r=readFromMemory(smf(1,{blockTrack(9),blockTrack()}));check(bool(r)&&r.file.tracks[0].percussionNotes==3,"drums retained");const auto e=extractHarmony(r.file);check(bool(e)&&e.selectedTrack==1&&e.chords[0].name=="C","drums excluded");});
        run("Multi-track selection and override",[]{auto file=notes({60,64,67});for(int i=0;i<8;++i)file.notes.push_back({72+i,90,0,1,i*.5,.25});file.tracks.push_back({1,"Melody",8,0});const auto e=extractHarmony(file);check(bool(e)&&e.selectedTrack==0&&e.tracks.size()==2,"chord-like track");MidiHarmonyExtractionConfig config;config.selectedTrack=1;check(!extractHarmony(file,config),"monophonic override refuses chords");});
        run("Near-simultaneous onset",[]{auto file=notes({60,64,67});file.notes[1].startQN=.020;file.notes[1].durationQN-=.020;file.notes[2].startQN=.035;file.notes[2].durationQN-=.035;const auto e=extractHarmony(file);check(bool(e)&&e.chords.size()==1&&near(e.chords[0].startQN,0)&&near(*e.chords[0].durationQN,4),"onset tolerance");});
        run("Active sustain and adjacent merge",[]{auto file=notes({48},4);for(double start:{0.,2.})for(int pitch:{64,67})file.notes.push_back({pitch,80,0,0,start,2});const auto e=extractHarmony(file);check(bool(e)&&e.chords.size()==1&&e.chords[0].name=="C"&&near(*e.chords[0].durationQN,4),"sustain retrigger merge");});
        run("Octave duplicates",[]{const auto e=extractHarmony(notes({36,48,60,64,67,72}));check(bool(e)&&e.chords.size()==1&&e.chords[0].name=="C","octaves");});
        run("Inversion actual bass",[]{const auto e=extractHarmony(notes({52,55,60}));check(bool(e)&&e.chords[0].name=="C/E"&&e.chords[0].root==PitchClass::C&&e.chords[0].bass==PitchClass::E,"inversion");});
        run("Missing fifth",[]{const auto e=extractHarmony(notes({48,64,71}));check(bool(e)&&e.chords[0].name=="Cmaj7"&&e.slices[0].confidence<.85,"no5");});
        run("Short passing note suppression",[]{auto file=notes({48,64,67,70});file.notes.push_back({74,80,0,0,1,.05});const auto e=extractHarmony(file);check(bool(e)&&e.chords.size()==1&&e.chords[0].name=="C7"&&near(*e.chords[0].durationQN,4),"passing note");});
        run("Short clear block retained",[]{const auto e=extractHarmony(notes({60,64,67},.0625));check(bool(e)&&near(*e.chords[0].durationQN,.0625),"short block");});
        run("Complete and OPEN",[]{const auto complete=extractHarmony(notes({60,64,67}));MidiHarmonyExtractionConfig config;config.openEnded=true;const auto open=extractHarmony(notes({60,64,67}),config);check(bool(complete)&&!complete.chords[0].openEnded&&complete.chords[0].durationQN.has_value(),"complete");check(bool(open)&&open.chords[0].openEnded&&!open.chords[0].durationQN,"OPEN");});
        run("Safe invalid extraction and empty file",[]{auto file=notes({60,64,67});file.notes[0].startQN=std::numeric_limits<double>::quiet_NaN();check(!extractHarmony(file),"non-finite");check(!extractHarmony(notes({60})),"no polyphony");auto config=MidiHarmonyExtractionConfig{};config.onsetToleranceQN=-1;check(!extractHarmony(notes({60,64,67}),config),"config");});
        run("Long chord rejected before session",[]{check(!extractHarmony(notes({60,64,67},129)),"existing preview duration cap");});
        for(const auto* suffix:{"","m","dim","aug","6","m6","7","maj7","m7","dim7","m7b5","sus2","sus4","9","maj9","m9"}) {
            const auto label=std::string("C")+suffix;const auto chord=chordPitches(label);MidiFile file=notes({36});for(int p=0;p<12;++p)if(chord.intervals&(1<<p))file.notes.push_back({60+p,80,0,0,0,4});
            const auto e=extractHarmony(file);check(bool(e)&&e.chords.size()==1,label+": quality fit");ChordEvent expected;expected.name=label;identity(expected,e.chords[0],true,label);++qualities;
        }
        const std::vector<std::pair<std::string,Progression>> phrases{{"pop",phrase({"C","Am","F","G"})},{"sevenths",phrase({"Cmaj7","Am7","Dm7","G7"})},{"inversion",phrase({"C","G/B","Am"})},{"passing_dim",phrase({"C","C#dim7","Dm7","G7"})},{"secondary",phrase({"C","A7","Dm","G7"})},{"minor",phrase({"Am","Dm","E7","Am"})}};
        for(const auto& [name,p]:phrases)for(const auto mode:{ArrangementMode::BlockChords,ArrangementMode::VoiceLed})roundTrip(p,mode,name,output);
        run("Import failure preserves phrase and schema 5",[&]{session::PluginSessionState state;state.imported.replace(phrase({"Dm7","G7"}),TimelineCoordinateMode::RelativeToSelection);const auto before=session::serialize(state);
            check(!applyImport(importFile(output/"missing.mid"),state.imported)&&session::serialize(state)==before,"preserve original");
            check(applyImport(importFile(output/"inversion_block.mid"),state.imported),"file workflow");const auto restored=session::deserialize(session::serialize(state));check(bool(restored)&&restored.state.schemaVersion==5&&restored.state.imported.events[1].name=="G/B"&&state.constraints.melody.empty(),"state identity no melody inference");});
        run("Unified save/drag scope and lifetime",[&]{CandidateExportContext context;context.imported.replace(phrase({"C","G7"}),TimelineCoordinateMode::RelativeToSelection);
            ContinuationCandidate c;c.id="drag-test";c.intent=PhraseIntent::Resolve;c.key={PitchClass::C,Mode::Major};c.continuation.push_back({"C",2,{1,0},ChordQuality::Major,{}});
            const auto payload=candidatePayload(context,c);check(bool(payload)&&payload.payload.chordCount==3&&payload.payload.boundaryTick.has_value(),"full phrase default");
            const auto preview=preview::buildSequence(context.imported,&c,120);const auto clip=buildClip(preview.sequence,ArrangementMode::VoiceLed,ExportScope::FullPhrase,{},c.key,c.intent);check(payload.payload.smfBytes==writeToMemory(clip.sequence).bytes,"save drag same writer events");
            const auto dir=output/"drag-lifetime";std::filesystem::create_directories(dir);std::string error;
            check(writeToFile(payload.payload.smfBytes,dir/"HC_old.mid",error)&&writeToFile(payload.payload.smfBytes,dir/"other.mid",error),error);
            for(const auto* name:{"HC_old.mid","other.mid"})std::filesystem::last_write_time(dir/name,std::filesystem::file_time_type::clock::now()-std::chrono::hours(72));
            const auto a=createDragFile(payload.payload,dir),b=createDragFile(payload.payload,dir);check(bool(a)&&bool(b)&&a.path!=b.path&&std::filesystem::exists(a.path)&&std::filesystem::exists(b.path),"unique retained temp");
            check(!std::filesystem::exists(dir/"HC_old.mid")&&std::filesystem::exists(dir/"other.mid"),"expire only own 48h files");
            std::ifstream input(a.path,std::ios::binary);const Bytes dragged{std::istreambuf_iterator<char>(input),{}};check(dragged==payload.payload.dawClipBytes,"temp uses DAWClip");
            context.scope=ExportScope::ContinuationOnly;check(candidatePayload(context,c).payload.chordCount==1,"scope reuse");
            enrichment::EnrichmentCandidate e;e.progression=phrase({"Cmaj7","G7"});context.scope=ExportScope::FullPhrase;check(candidatePayload(context,e).payload.chordCount==2,"enrichment uses transformed original object");
        });
        std::cout<<"Import fixtures "<<fixtures<<'/'<<fixtures<<" PASS; qualities "<<qualities<<"/16; round-trip "<<roundTrips<<"/12; checks "<<checks<<'\n';return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
