#include "demo/DemoScenario.h"
#include "library/ProgressionLibrary.h"
#include "preview/OfflinePreviewRenderer.h"
#include <array>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <stdexcept>

namespace {
using namespace harmony;
namespace fs=std::filesystem;
const std::array<std::vector<const char*>,8> golden{{
    {"C","Am","Dm","G","C"}, {"C","Am","A7","Dm","G","C"},
    {"Dm7","G7","Cmaj7"}, {"C","F","Fm","C"},
    {"C","G","Am","F","C"}, {"Am","F","Dm","E7","Am"},
    {"C","Am","F#","Dm","G","C"}, {"C","Am","F","G","C"}
}};
preview::Sequence makeGolden(int index) {
    preview::Sequence s; s.tempoBPM=120;
    double at{};
    for (const auto* name:golden.at(index)) {
        auto chord=preview::chordFromLabel(name);
        if (chord.root<0 || !chord.intervals) throw std::runtime_error(std::string("unsupported golden chord: ")+name);
        s.events.push_back({std::move(chord),at,2.0,s.events.empty()?preview::Segment::Existing:preview::Segment::Recommended});
        at+=2;
    }
    s.recommendationBoundary=1; s.totalQN=at;
    return s;
}
void output(const preview::Sequence& sequence,const fs::path& path,double rate) {
    preview::PreviewSynth synth; synth.prepare(rate);
    if (!synth.start(sequence)) throw std::runtime_error("schedule failed");
    std::cout << "Tempo " << sequence.tempoBPM << " BPM\n";
    for (std::size_t i=0;i<sequence.events.size();++i) {
        const auto& e=sequence.events[i]; const auto& v=synth.schedule()[i].voicing;
        std::cout << e.chord.label << " " << e.startQN << "+" << e.durationQN << " QN  MIDI " << v.bass;
        for (int n=0;n<v.upperCount;++n) std::cout << ',' << v.upper[n];
        std::cout << '\n';
    }
    const auto audio=preview::renderOffline(sequence,rate);
    if (audio.left.empty()) throw std::runtime_error("render failed");
    std::string error;
    if (!preview::writeWav16(audio,path,error)) throw std::runtime_error(error);
    std::cout << "Duration " << audio.left.size()/rate << " s  Peak " << audio.peak
        << "  RMS " << audio.rms << "  Peak voices " << audio.peakVoices << "\nOutput " << path.string() << '\n';
}
void bench() {
    for (int count:{10,32,64}) for (double rate:{44100.0,48000.0}) {
        auto s=makeGolden(2); s.events.clear();
        double at{};
        for (int i=0;i<count;++i) { s.events.push_back({preview::chordFromLabel(golden[2][i%3]),at,1.0,preview::Segment::Recommended}); at+=1; }
        s.totalQN=at;
        const auto start=std::chrono::steady_clock::now();
        const auto audio=preview::renderOffline(s,rate);
        const auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        if (audio.left.empty()) throw std::runtime_error("benchmark render failed");
        std::cout << count << " chords " << rate << " Hz: " << elapsed << " ms, peak voices "
            << audio.peakVoices << ", peak " << audio.peak << '\n';
    }
}
}
int main(int argc,char** argv) {
    try {
        std::string letter,groupName="Resolve"; fs::path scenarioPath,outputPath,goldenDir;
        int candidateNumber=1; double rate=48000; bool runBench{};
        for (int i=1;i<argc;++i) {
            const std::string key=argv[i];
            auto value=[&]() -> std::string { if (++i>=argc) throw std::runtime_error("missing value for "+key); return argv[i]; };
            if (key=="--case") letter=value(); else if (key=="--scenario") scenarioPath=value();
            else if (key=="--group") groupName=value(); else if (key=="--candidate") candidateNumber=std::stoi(value());
            else if (key=="--output") outputPath=value(); else if (key=="--golden-dir") goldenDir=value();
            else if (key=="--sample-rate") rate=std::stod(value()); else if (key=="--bench") runBench=true;
            else throw std::runtime_error("unknown option: "+key);
        }
        if (runBench) { bench(); return 0; }
        if (!goldenDir.empty()) {
            fs::create_directories(goldenDir);
            for (int i=0;i<8;++i) output(makeGolden(i),goldenDir/(std::string("case_")+char('A'+i)+".wav"),rate);
            return 0;
        }
        if (!letter.empty()) {
            if (letter.size()!=1||letter[0]<'A'||letter[0]>'H') throw std::runtime_error("case must be A-H");
            scenarioPath=fs::path(HC_DEMO_FIXTURE_DIR)/(std::string("case_")+char(letter[0]-'A'+'a')+".json");
        }
        if (scenarioPath.empty()||outputPath.empty()) throw std::runtime_error("usage: preview_cli --case A --group Resolve --candidate 1 --output file.wav | --golden-dir folder | --bench");
        const auto loaded=demo::loadScenario(scenarioPath);
        if (!loaded) throw std::runtime_error(loaded.error);
        ImportedProgressionSession imported;
        if (!imported.replace(loaded.scenario.chords,TimelineCoordinateMode::RelativeToSelection)) throw std::runtime_error("invalid progression");
        const auto factory=library::loadFactory(HC_FACTORY_DB_PATH);
        if (!factory) throw std::runtime_error(factory.error);
        const CandidateIndex index(factory.templates);
        AnalysisContext context; context.forcedKey=loaded.scenario.forcedKey;
        context.timeSigNumerator=loaded.scenario.meterNumerator; context.timeSigDenominator=loaded.scenario.meterDenominator;
        const auto set=recommendContinuations(makeMatchQuery(imported.events,context),index,
            {loaded.scenario.style,loaded.scenario.intent});
        int group=-1;
        for (int i=0;i<4;++i) if (groupName==std::array<const char*,4>{"Resolve","Develop","Loop","Color"}[i]) group=i;
        if (group<0||candidateNumber<1||static_cast<std::size_t>(candidateNumber)>set.groups[group].size())
            throw std::runtime_error("candidate unavailable in group");
        const auto built=preview::buildSequence(imported,&set.groups[group][candidateNumber-1],loaded.scenario.tempo);
        if (!built) throw std::runtime_error(built.error);
        output(built.sequence,outputPath,rate);
    } catch (const std::exception& e) { std::cerr << "preview_cli: " << e.what() << '\n'; return 1; }
}
