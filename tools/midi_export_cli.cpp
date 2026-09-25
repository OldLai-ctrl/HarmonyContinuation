#include "demo/DemoScenario.h"
#include "library/ProgressionLibrary.h"
#include "midi/StandardMidiFileWriter.h"
#include "snapshot/RecommendationSnapshot.h"
#include <array>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
using namespace harmony;
namespace fs=std::filesystem;
}
int main(int argc,char** argv) {
    try {
        std::string letter,groupName="Resolve",modeName="voice-led",scopeName="full";
        fs::path scenarioPath,outputPath,snapshotIn,snapshotOut;
        int candidateNumber=1; bool debug{};
        for (int i=1;i<argc;++i) {
            const std::string key=argv[i];
            auto value=[&]() -> std::string {if(++i>=argc)throw std::runtime_error("missing value for "+key);return argv[i];};
            if(key=="--case")letter=value(); else if(key=="--scenario")scenarioPath=value();
            else if(key=="--group")groupName=value(); else if(key=="--candidate")candidateNumber=std::stoi(value());
            else if(key=="--mode")modeName=value(); else if(key=="--scope")scopeName=value();
            else if(key=="--output")outputPath=value(); else if(key=="--snapshot-in")snapshotIn=value();
            else if(key=="--snapshot-out")snapshotOut=value(); else if(key=="--debug")debug=true;
            else throw std::runtime_error("unknown option: "+key);
        }
        if(modeName!="block"&&modeName!="voice-led")throw std::runtime_error("mode must be block or voice-led");
        if(scopeName!="full"&&scopeName!="current"&&scopeName!="continuation")
            throw std::runtime_error("scope must be full, current or continuation");
        const auto mode=modeName=="block"?midi::ArrangementMode::BlockChords:midi::ArrangementMode::VoiceLed;
        const auto scope=scopeName=="current"?midi::ExportScope::CurrentOnly:
            scopeName=="continuation"?midi::ExportScope::ContinuationOnly:midi::ExportScope::FullPhrase;
        if(outputPath.empty())throw std::runtime_error("usage: midi_export_cli --case A --group Resolve --candidate 1 --mode voice-led --scope full --output file.mid");
        ImportedProgressionSession imported; ContinuationCandidate candidate;
        std::optional<KeySignature> key; std::optional<Style> style; std::optional<PhraseIntent> intent;
        std::vector<MatchResult> matches; double tempo{}; int numerator{},denominator{};
        bool candidateAvailable{};
        if(!snapshotIn.empty()) {
            const auto decoded=snapshot::loadFile(snapshotIn);
            if(!decoded)throw std::runtime_error(decoded.error);
            imported=decoded.value.imported; candidate=decoded.value.candidate; candidateAvailable=true;
            if(decoded.value.match)matches.push_back(*decoded.value.match);
            key=decoded.value.key; style=decoded.value.style; intent=decoded.value.intent;
            tempo=decoded.value.tempoBPM; numerator=decoded.value.meterNumerator; denominator=decoded.value.meterDenominator;
        } else {
            if(!letter.empty()) {
                if(letter.size()!=1||letter[0]<'A'||letter[0]>'H')throw std::runtime_error("case must be A-H");
                scenarioPath=fs::path(HC_DEMO_FIXTURE_DIR)/(std::string("case_")+char(letter[0]-'A'+'a')+".json");
            }
            if(scenarioPath.empty())throw std::runtime_error("case or scenario required");
            const auto loaded=demo::loadScenario(scenarioPath);
            if(!loaded)throw std::runtime_error(loaded.error);
            if(!imported.replace(loaded.scenario.chords,TimelineCoordinateMode::RelativeToSelection))
                throw std::runtime_error("invalid progression");
            tempo=loaded.scenario.tempo; numerator=loaded.scenario.meterNumerator; denominator=loaded.scenario.meterDenominator;
            key=loaded.scenario.forcedKey; style=loaded.scenario.style; intent=loaded.scenario.intent;
            if(scope!=midi::ExportScope::CurrentOnly||!snapshotOut.empty()) {
                const auto factory=library::loadFactory(HC_FACTORY_DB_PATH);
                if(!factory)throw std::runtime_error(factory.error);
                const CandidateIndex index(factory.templates);
                AnalysisContext context; context.forcedKey=key;
                context.timeSigNumerator=numerator; context.timeSigDenominator=denominator;
                const auto set=recommendContinuations(makeMatchQuery(imported.events,context),index,{style,intent});
                int group=-1;
                for(int i=0;i<4;++i)if(groupName==std::array<const char*,4>{"Resolve","Develop","Loop","Color"}[i])group=i;
                if(group<0||candidateNumber<1||static_cast<std::size_t>(candidateNumber)>set.groups[group].size())
                    throw std::runtime_error("candidate unavailable in group");
                candidate=set.groups[group][candidateNumber-1]; candidateAvailable=true;
                matches=set.matches; key=candidate.key; intent=candidate.intent;
            } else if(!key) {
                const auto analysis=analyzeHarmony(imported.events);
                if(analysis.selectedKey)key=analysis.selectedKey->key;
            }
        }
        if(!snapshotOut.empty()) {
            if(!candidateAvailable)throw std::runtime_error("snapshot needs a candidate");
            const auto snap=snapshot::capture(imported,candidate,matches,tempo,numerator,denominator,key,style,intent);
            std::string error;
            if(!snapshot::saveFile(snap,snapshotOut,error))throw std::runtime_error(error);
        }
        const ContinuationCandidate* selected=scope==midi::ExportScope::CurrentOnly?nullptr:
            candidateAvailable?&candidate:nullptr;
        const auto preview=preview::buildSequence(imported,selected,tempo);
        if(!preview)throw std::runtime_error(preview.error);
        const auto clip=midi::buildClip(preview.sequence,mode,scope,{numerator,denominator},key,intent);
        if(!clip)throw std::runtime_error(clip.error);
        const auto written=midi::writeToMemory(clip.sequence);
        if(!written)throw std::runtime_error(written.error);
        std::string error;
        if(!midi::writeToFile(written.bytes,outputPath,error))throw std::runtime_error(error);
        const auto duration=midi::qnToTicks(clip.sequence.totalQN,480);
        std::cout << "Tempo " << clip.sequence.tempoBPM << " BPM  Meter " << numerator << '/' << denominator
            << "  PPQ 480\nChords " << clip.sequence.chordCount << "  Notes " << clip.sequence.notes.size()
            << "  Duration " << clip.sequence.totalQN << " QN / " << duration << " ticks\nMode " << modeName
            << "  Scope " << scopeName << "  Boundary " << (clip.sequence.boundaryQN?
                std::to_string(midi::qnToTicks(*clip.sequence.boundaryQN,480)):"none")
            << "\nOutput " << outputPath.string() << '\n';
        if(debug)for(const auto& note:clip.sequence.notes)
            std::cout << note.midiNote << " @" << note.startQN << "+" << note.durationQN
                << " velocity " << static_cast<int>(note.velocity) << '\n';
        if(!snapshotOut.empty())std::cout << "Snapshot " << snapshotOut.string() << '\n';
    } catch(const std::exception& e) {std::cerr << "midi_export_cli: " << e.what() << '\n';return 1;}
}
