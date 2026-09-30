#include "core/HarmonyConstraints.h"
#include "core/ChordVoicer.h"
#include "library/ProgressionLibrary.h"
#include "enrichment/ProgressionEnrichmentEngine.h"
#include "midi/MidiClip.h"
#include "snapshot/RecommendationSnapshot.h"
#include "snapshot/EnrichmentSnapshot.h"
#include "session/PluginSessionState.h"
#include "benchmark/BenchJson.h"
#include "demo/DemoScenario.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace harmony;
namespace {
void check(bool okay,const char* message){if(!okay)throw std::runtime_error(message);}
Progression progression(std::initializer_list<const char*> labels,double anchor=0) {
    Progression p;for(const auto* label:labels){ChordEvent e;e.name=label;e.startQN=anchor+4*p.size();
        e.durationQN=4;e.openEnded=false;p.push_back(e);}return p;
}
MelodyConstraint note(const char* name,double start=0,double duration=4,
    ConstraintRole role=ConstraintRole::Present,ConstraintStrictness strictness=ConstraintStrictness::Soft) {
    auto result=parseMelodyNote(name);check(result.has_value(),"parse note");
    result->startQN=start;result->durationQN=duration;result->role=role;result->strictness=strictness;return *result;
}
std::size_t count(const RecommendationSet& s){std::size_t n{};for(const auto& group:s.groups)n+=group.size();return n;}
std::size_t count(const enrichment::EnrichmentResult& s){std::size_t n{};for(const auto& group:s.groups)n+=group.size();return n;}
}
int main(int argc,char** argv) {
    try {
        benchmark::json::Value report=benchmark::json::Value::map();
        report.object["cases"]=benchmark::json::Value::list();int passed{};
        const auto run=[&](const char* name,auto body){body();auto item=benchmark::json::Value::map();
            item.object["id"]=name;item.object["passed"]=true;report.object["cases"].array.push_back(item);++passed;};
        run("major_chord_tone",[]{auto p=progression({"Cmaj7"});auto r=evaluateMelody(p,{{note("E4")}});
            check(r.score==1&&r.observations[0].interval==4,"Cmaj7 E third");});
        run("minor_chord_tone",[]{auto p=progression({"Am"});check(evaluateMelody(p,{{note("C4")}}).score==1,"Am C third");});
        run("seventh",[]{auto p=progression({"G7"});check(evaluateMelody(p,{{note("F4")}}).score==1,"G7 seventh");});
        run("available_ninth",[]{auto p=progression({"G7"});auto r=evaluateMelody(p,{{note("A4")}});
            check(r.observations[0].relation==MelodyRelation::AvailableTension&&r.hardSatisfied,"G7 ninth");
            check(classifyMelody(progression({"G7b9"})[0],8)==MelodyRelation::AvailableTension,"explicit altered tension");
            check(classifyMelody(progression({"G13"})[0],4)==MelodyRelation::AvailableTension,"thirteenth tension");});
        run("clear_conflict",[]{auto p=progression({"C"});auto e=note("Db4",0,4,ConstraintRole::Present,ConstraintStrictness::Hard);
            const auto r=evaluateMelody(p,{{e}});check(!r.hardSatisfied&&r.score==0,"hard conflict");
            ImportedProgressionSession input;input.replace(p,TimelineCoordinateMode::RelativeToSelection);
            check(!preview::buildSequence(input,nullptr,120,{{e}}),"strict conflict cannot enter preview/export");});
        run("uncertain_not_rejected",[]{auto p=progression({"Cmaj7"});auto r=evaluateMelody(p,{{note("F4",0,4,
            ConstraintRole::Present,ConstraintStrictness::Hard)}});check(r.hardSatisfied&&r.score>0,"uncertain F hard allowed");});
        run("absolute_top_voice",[]{ChordVoicer v;auto c=chordPitches("Cmaj7");auto r=v.voice(c,64);
            check(r.melodySatisfied&&r.upper[r.upperCount-1]==64,"absolute E4 top");
            std::uint16_t heard=1u<<((r.bass-c.root+12)%12);for(int i=0;i<r.upperCount;++i)heard|=1u<<((r.upper[i]-c.root+132)%12);
            check((heard&(1u<<4))&&(heard&(1u<<11)),"third and seventh retained");});
        run("pitch_class_top_voice",[]{auto e=note("E",0,4,ConstraintRole::TopVoice);check(!e.pitch&&melodyTopPitch(e)==76,"reasonable octave");
            ChordVoicer v;auto r=v.voice(chordPitches("Am"),melodyTopPitch(e));check(r.melodySatisfied&&r.upper[r.upperCount-1]%12==4,"pitch class top");});
        run("multiple_timed_notes",[]{ImportedProgressionSession input;input.replace(progression({"C","G"}),TimelineCoordinateMode::RelativeToSelection);
            const auto fixture=demo::loadScenario(HC_CONSTRAINT_CASE);check(fixture&&fixture.scenario.constraints.melody.size()==3,"timed melody file input");
            HarmonyConstraintSet set{{note("E4",0,2,ConstraintRole::TopVoice),note("F4",2,2,ConstraintRole::TopVoice),
                note("G4",4,4,ConstraintRole::TopVoice)}};
            auto preview=preview::buildSequence(input,nullptr,120,set);check(preview&&preview.sequence.events.size()==3,"split at note boundary");
            const auto clip=midi::buildClip(preview.sequence,midi::ArrangementMode::VoiceLed,midi::ExportScope::FullPhrase,{});
            check(static_cast<bool>(clip),"multi note MIDI");for(const auto& event:preview.sequence.events){int highest=-1;
                for(const auto& n:clip.sequence.notes)if(n.startQN==event.startQN)highest=std::max(highest,n.midiNote);
                check(highest==event.topVoice,"MIDI exact top voice");}});
        run("contradictory_hard_top",[]{auto p=progression({"Cmaj7"});auto r=evaluateMelody(p,{{
            note("E4",0,4,ConstraintRole::TopVoice,ConstraintStrictness::Hard),
            note("G4",0,4,ConstraintRole::TopVoice,ConstraintStrictness::Hard)}});check(!r.hardSatisfied,"conflicting highest notes rejected");});
        run("bounded_input",[]{check(!parseMelodyNote("E99")&&!parseMelodyNote("E4junk"),"bad notes rejected");
            auto e=note("E4");e.durationQN=-1;check(!validConstraints({{e}}),"invalid duration rejected");
            check(parseMelodyNote("F#4")->pitch==66,"sharp absolute pitch");});
        const auto factory=library::loadFactory(HC_FACTORY_DB_PATH);check(static_cast<bool>(factory),"factory load");
        const CandidateIndex index(factory.templates);auto source=progression({"C","F","G"});source.back().durationQN.reset();source.back().openEnded=true;
        AnalysisContext context;context.forcedKey=KeySignature{PitchClass::C,Mode::Major};const auto query=makeMatchQuery(source,context);
        const auto baseline=recommendContinuations(query,index);check(count(baseline)>0,"continuation baseline exists");
        run("continuation_hard",[&]{RecommendationRequest request;request.constraints={{note("Db4",12,4,ConstraintRole::Present,ConstraintStrictness::Hard)}};
            const auto constrained=recommendContinuations(query,index,request);
            check(count(constrained)<count(baseline),"hard filters conflicts");
            for(const auto& group:constrained.groups)for(const auto& c:group)check(c.melodyCompatibility.hardSatisfied,"surviving hard candidates");});
        run("continuation_soft",[&]{RecommendationRequest request;request.constraints={{note("Db4",12,4)}};
            auto constrained=recommendContinuations(query,index,request);check(count(constrained)==count(baseline),"soft preserves choices");
            bool penalty=false;for(const auto& group:constrained.groups)for(const auto& c:group)
                if(c.melodyCompatibility.score<1)penalty=true;check(penalty,"soft penalty available");});
        run("enrichment_hard_jazz",[]{auto p=progression({"C","Dm","G","C"});auto analysis=analyzeHarmony(p);
            enrichment::EnrichmentConfig config;config.constraints={{note("Db4",0,4,ConstraintRole::Present,ConstraintStrictness::Hard)}};
            auto r=enrichment::enrichProgression(p,analysis,Style::Jazz,config);
            for(const auto& group:r.groups)for(const auto& c:group)check(c.melodyCompatibility.hardSatisfied,"hard enrichment filtering");
            check(count(r)<count(enrichment::enrichProgression(p,analysis,Style::Jazz)),"hard enrichment fewer conflicts");});
        run("enrichment_soft_pop_snapshot",[]{auto p=progression({"C","Am","F","G"},32);auto analysis=analyzeHarmony(p);
            enrichment::EnrichmentConfig config;config.constraints={{note("E4",32,2,ConstraintRole::TopVoice),note("F4",34,2,ConstraintRole::TopVoice)}};
            auto r=enrichment::enrichProgression(p,analysis,Style::Pop,config);check(count(r)>0,"soft enrichment available");
            ImportedProgressionSession input;input.replace(p,TimelineCoordinateMode::AbsoluteProjectQN);
            const auto snap=snapshot::captureEnrichment(input,r.groups[0].front(),120,4,4,{},Style::Pop);
            const auto decoded=snapshot::deserializeEnrichment(snapshot::serialize(snap));check(static_cast<bool>(decoded),"enrichment snapshot");
            check(decoded.value.candidate.constraints.melody.size()==2&&decoded.value.candidate.constraints.melody[0].startQN==0,"relative melody snapshot");});
        run("session_and_continuation_snapshot",[&]{session::PluginSessionState state;
            state.constraints={{note("E4",12,4,ConstraintRole::TopVoice)}};state.tendency=HarmonicTendency::Bold;state.uiZoomPercent=150;
            const auto decoded=session::deserialize(session::serialize(state));check(decoded&&decoded.state.constraints==state.constraints&&
                decoded.state.uiZoomPercent==150&&decoded.state.tendency==state.tendency,"session prefs restored");
            auto oldBytes=session::serialize({});oldBytes.resize(oldBytes.size()-6);oldBytes[3]='4';oldBytes[4]=4;
            const auto old=session::deserialize(oldBytes);check(old&&old.state.uiZoomPercent==100&&old.state.constraints.melody.empty(),"old session defaults");
            auto candidate=baseline.groups[0].front();candidate.constraints=state.constraints;
            ImportedProgressionSession input;input.replace(source,TimelineCoordinateMode::RelativeToSelection);
            const auto restored=snapshot::deserialize(snapshot::serialize(snapshot::capture(input,candidate,{},120,4,4)));
            check(restored&&restored.value.candidate.constraints==candidate.constraints,"continuation melody snapshot");});
        check(passed==16,"case count");report.object["passed"]=passed;
        if(argc==3&&std::string_view(argv[1])=="--output") {std::ofstream out(argv[2]);out<<benchmark::json::dump(report);check(out.good(),"report write");}
        else check(argc==1,"usage: constraint_benchmark --output report.json");
        std::cout<<"constraint cases="<<passed<<"/16 PASS\n";
    }catch(const std::exception& e){std::cerr<<"ConstraintBenchmark: "<<e.what()<<'\n';return 1;}
}
