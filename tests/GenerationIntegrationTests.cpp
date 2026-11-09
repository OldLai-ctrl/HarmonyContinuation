#include "generation/ProductGeneration.h"
#include "core/ContextualContinuation.h"
#include "plugin/RecommendationWorker.h"
#include "snapshot/RecommendationSnapshot.h"
#include "session/ProductServices.h"
#include "ui/RecommendationBrowse.h"
#include "preview/OfflinePreviewRenderer.h"
#include "preview/PreviewTransport.h"
#include "midi/MidiWorkflow.h"
#include "ProgressionJson.h"
#include "benchmark/BenchJson.h"
#include "midi/StandardMidiFileReader.h"
#include <sstream>
#include <tuple>
#include <algorithm>
#include <fstream>
#include <thread>
#include <chrono>
#include <iostream>
#include <set>
using namespace harmony;
namespace {
int checks{};
void check(bool value,const std::string& why){if(!value)throw std::runtime_error(why);++checks;}
Progression input(const std::filesystem::path& file){std::ifstream in(file);const auto parsed=dev::parseProgressionJson(std::string((std::istreambuf_iterator<char>(in)),{}));check(bool(parsed),parsed.error);return parsed.events;}
plugin::WorkerResult wait(plugin::RecommendationWorker& worker,std::uint64_t token){
    for(int i=0;i<3000;++i){if(auto result=worker.takeLatest()){check(result->generation==token,"no stale worker result");return std::move(*result);}std::this_thread::sleep_for(std::chrono::milliseconds(10));}
    throw std::runtime_error("worker timeout");
}
}
int main(int argc,char** argv)try{
    check(argc==4,"factory.db listening-directory evidence-directory");
    std::filesystem::path out=argv[3];std::filesystem::create_directories(out);
    plugin::RecommendationWorker worker(argv[1],out/"isolated-user.db",out/"isolated-factory-store");
    std::ofstream report(out/"recommendations.tsv");report<<"input\tposition\tid\tsource\tstyle\tchords\n";
    for(const auto folder:{"01-Open-Development","03-Descending-Bass"}){
        auto progression=input(std::filesystem::path(argv[2])/folder/"INPUT.json");
        AnalysisContext context;context.timeSigNumerator=4;context.timeSigDenominator=4;
        RecommendationRequest request;request.prepareBothViews=true;request.inspirationPriority=true;
        const auto first=wait(worker,worker.submit(progression,context,request,false,1));check(first.error.empty(),first.error);
        std::vector<std::string> ids;std::size_t generated{};
        for(std::size_t pos=0;pos<first.recommendations.inspirationOrder.size();++pos){const auto ref=first.recommendations.inspirationOrder[pos];const auto& c=first.recommendations.groups[ref.group][ref.index];
            report<<folder<<'\t'<<pos+1<<'\t'<<c.id<<'\t'<<(c.generated?"generated":"database")<<'\t'<<(c.generated?c.generated->variation:"")<<'\t';for(const auto& chord:c.continuation)report<<chord.label<<'@'<<chord.durationQN<<' ';report<<'\n';
            if(!c.generated){ids.push_back(continuationMusicFingerprint(c));continue;}
            ++generated;check(c.primaryTemplate.empty()&&c.baseQualified&&c.generated->bassMidi.size()==c.continuation.size(),"independent generated identity");
            check(c.generated->priorLicense.find("CC0")!=std::string::npos,"product prior license");
            ImportedProgressionSession imported;check(imported.replace(progression,TimelineCoordinateMode::RelativeToSelection),"imported fixture coordinate domain");
            auto sequence=preview::buildSequence(imported,&c,120);check(bool(sequence),sequence.error);
            const auto prepared=preview::preparePreview(sequence.sequence);check(bool(prepared),prepared.error);
            for(std::size_t i=0;i<sequence.sequence.events.size();++i)if(sequence.sequence.events[i].bassMidi)
                check(prepared.clip.chords[i].voicing.bass==*sequence.sequence.events[i].bassMidi,"processor transport preserves actual generated bass");
            auto frozen=snapshot::capture(imported,c,{},120,4,4);const auto decoded=snapshot::deserialize(snapshot::serialize(frozen));check(bool(decoded),decoded.error);
            check(decoded.value.candidateAvailable&&continuationMusicFingerprint(decoded.value.candidate)==continuationMusicFingerprint(c),"frozen generated identity");
            auto restored=preview::buildSequence(decoded.value.imported,&decoded.value.candidate,120);check(bool(restored),restored.error);
            auto clip=midi::buildClip(sequence.sequence,midi::ArrangementMode::VoiceLed,midi::ExportScope::FullPhrase,{4,4});check(bool(clip),clip.error);
            auto restoredClip=midi::buildClip(restored.sequence,midi::ArrangementMode::VoiceLed,midi::ExportScope::FullPhrase,{4,4});check(bool(restoredClip),restoredClip.error);
            const auto bytes=midi::writeToMemory(clip.sequence);check(bool(bytes),bytes.error);check(bytes.bytes==midi::writeToMemory(restoredClip.sequence).bytes,"snapshot MIDI actual performance remains identical");
            const auto stem=std::string(folder)+"-"+c.generated->variation;
            std::string error;check(midi::writeToFile(bytes.bytes,out/(stem+".mid"),error),error);
            check(snapshot::saveFile(frozen,out/(stem+".json"),error),error);
            check(preview::writeWav16(preview::renderOffline(sequence.sequence,48000),out/(stem+".wav"),error),error);
            auto changed=c;changed.generated->explanation="different words";check(continuationMusicFingerprint(changed)==continuationMusicFingerprint(c),"metadata independent performance identity");
            changed.generated->bassMidi[0]+=12;check(continuationMusicFingerprint(changed)!=continuationMusicFingerprint(c),"actual bass participates in identity");
            session::PluginSessionState state;state.imported=imported;state.factoryLibraryVersion=8;check(state.pin(c.id,session::continuationFingerprint(c)),"generated pin");
            auto stateDecoded=session::deserialize(session::serialize(state));check(bool(stateDecoded),stateDecoded.error);stateDecoded.state.resolvePins(first.recommendations);check(stateDecoded.state.pinnedCandidateIds==state.pinnedCandidateIds,"Session5 generated pin roundtrip");
        }
        check(std::string(folder)=="01-Open-Development"?generated==3:generated>0,"first input has three styles; second has actual qualified generated routes");
        auto baseline=first.recommendations;
        std::erase_if(baseline.inspirationOrder,[&](const auto ref){return baseline.groups[ref.group][ref.index].generated.has_value();});
        for(auto& group:baseline.groups)std::erase_if(group,[](const auto& c){return c.generated.has_value();});
        generation::appendProductGeneration(baseline,progression,context,request);
        std::vector<std::string> before;
        for(const auto ref:baseline.inspirationOrder)if(!baseline.groups[ref.group][ref.index].generated)
            before.push_back(continuationMusicFingerprint(baseline.groups[ref.group][ref.index]));
        check(before==ids,"database inspiration ordering unchanged after inserting generated candidates");
        ImportedProgressionSession inputSession;check(inputSession.replace(progression,TimelineCoordinateMode::RelativeToSelection),"input MIDI fixture coordinate domain");const auto inputSequence=preview::buildSequence(inputSession,nullptr,120);check(bool(inputSequence),inputSequence.error);
        const auto inputClip=midi::buildClip(inputSequence.sequence,midi::ArrangementMode::VoiceLed,midi::ExportScope::FullPhrase,{4,4});check(bool(inputClip),inputClip.error);
        std::string inputError;check(midi::writeToFile(midi::writeToMemory(inputClip.sequence).bytes,out/(std::string(folder)+"-INPUT.mid"),inputError),inputError);

        auto databaseRequest=request;databaseRequest.inspirationPriority=false;databaseRequest.prepareBothViews=false;
        const auto database=wait(worker,worker.submit(progression,context,databaseRequest,false,2));
        // Exact bank is produced by the unchanged worker query in both runs.
        std::vector<std::string> exactA,exactB;
        for(const auto& group:first.exactRecommendations.groups)for(const auto& c:group)exactA.push_back(c.id+continuationMusicFingerprint(c));
        for(const auto& group:database.recommendations.groups)for(const auto& c:group)exactB.push_back(c.id+continuationMusicFingerprint(c));
        check(exactA==exactB,"database Exact candidates and identity unchanged");
        const auto page=ui::browsePage(first.recommendations.inspirationOrder.size(),0);check(page.end-page.begin==10,"ten entries on first page");
        const auto again=wait(worker,worker.submit(progression,context,request,true,2));
        check(again.analysisReused,"reuse worker query cache on ranking request");
        std::vector<std::string> a,b;for(const auto& g:first.recommendations.groups)for(const auto& c:g)a.push_back(c.id+continuationMusicFingerprint(c));for(const auto& g:again.recommendations.groups)for(const auto& c:g)b.push_back(c.id+continuationMusicFingerprint(c));check(a==b,"deterministic cached and uncached result");
    }
    // Reuse the three accepted frozen research performances, without embedding
    // their restricted statistical prior in this product build.
    const auto accepted=std::filesystem::path(argv[2]).parent_path()/"G13_1_GENERATED_LISTENING";
    for(const auto level:{"conventional","rich","bold"}){
        const auto folder=accepted/level;std::ifstream stream(folder/"generation.json");
        const auto json=benchmark::json::parse(std::string((std::istreambuf_iterator<char>(stream)),{}));const auto& phrase=json.at("phrases").array.front();
        ImportedProgressionSession imported;check(imported.replace(input(folder/"INPUT.json"),TimelineCoordinateMode::RelativeToSelection),"accepted fixture coordinate domain");
        ContinuationCandidate candidate;candidate.id=phrase.at("id").string;candidate.key={PitchClass::C,Mode::Major};candidate.suggestedCurrentChordDurationQN=phrase.at("inputOpenHoldQN").number;
        GeneratedCandidateMetadata meta;meta.generatorVersion=json.at("generatorVersion").string;meta.priorId=json.at("stylePrior").at("id").string;meta.priorLicense=json.at("stylePrior").at("license").string;meta.variation=level;meta.explanation="Accepted frozen non-commercial research performance; not the product CC0 generator output.";
        for(const auto& value:phrase.at("bassMidi").array)meta.bassMidi.push_back(int(value.number));candidate.generated=meta;
        for(const auto& value:phrase.at("chords").array){ChordEvent chord;chord.name=value.at("name").string;chord.root=PitchClass(int(value.at("root").number));chord.bass=PitchClass(int(value.at("bass").number));chord.startQN=value.at("startQN").number;chord.durationQN=value.at("durationQN").number;chord.openEnded=false;chord.quality=chordPitches(chord.name).quality;
            std::ostringstream mask;mask<<"0x"<<std::hex<<(unsigned(value.at("intervals").number)|unsigned(value.at("colorMask").number));chord.extensions.mask=mask.str();
            ConcreteChordEvent c;c.label=chord.name;c.quality=chord.quality;c.harmonicData=chord;c.durationQN=*chord.durationQN;candidate.continuation.push_back(c);
        }
        const auto original=midi::readFromFile(folder/phrase.at("midiAnnotated").string);check(bool(original),original.error);
        const auto frozenRead=snapshot::deserialize(snapshot::serialize(snapshot::capture(imported,candidate,{},120,4,4)));check(bool(frozenRead),frozenRead.error);
        for(const auto& c:{candidate,frozenRead.value.candidate}){
            const auto sequence=preview::buildSequence(imported,&c,120);check(bool(sequence),sequence.error);
            const auto clip=midi::buildClip(sequence.sequence,midi::ArrangementMode::VoiceLed,midi::ExportScope::FullPhrase,{4,4});check(bool(clip),clip.error);
            const auto notes=midi::readFromMemory(midi::writeToMemory(clip.sequence).bytes);check(bool(notes),notes.error);
            const auto key=[](const auto& n){return std::tuple(n.pitch,n.channel,n.startQN,n.durationQN);};
            auto a=original.file.notes,b=notes.file.notes;const auto less=[&](const auto& x,const auto& y){return key(x)<key(y);};std::sort(a.begin(),a.end(),less);std::sort(b.begin(),b.end(),less);check(a.size()==b.size(),"accepted performance note count");
            for(std::size_t i=0;i<a.size();++i)check(key(a[i])==key(b[i]),"accepted research performance identical through product/snapshot path");
        }
    }
    worker.submit(input(std::filesystem::path(argv[2])/"01-Open-Development/INPUT.json"),{},{});worker.cancel();
    const auto last=worker.submit(input(std::filesystem::path(argv[2])/"03-Descending-Bass/INPUT.json"),{},{});wait(worker,last);
    std::cout<<checks<<" G13C focused integration checks PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<"G13C FAIL: "<<e.what()<<'\n';return 1;}
