#include "midi/StandardMidiFileWriter.h"
#include "midi/StandardMidiFileReader.h"
#include "midi/MidiHarmonyExtractor.h"
#include "midi/MidiImportWorkflow.h"
#include "midi/MidiWorkflow.h"
#include "snapshot/EnrichmentSnapshot.h"
#include "ui/WhyExplanation.h"
#if defined(HC_RELIABILITY_PLUGIN)
#include "plugin/VstXmlChordParser.h"
#include "plugin/RecommendationWorker.h"
#include <thread>
#endif
#include "library/ProgressionLibrary.h"
#include "core/ContinuationEngine.h"
#include "enrichment/ProgressionEnrichmentEngine.h"
#include "session/ProductServices.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <cstdlib>
#include <new>
#include <map>
#include <set>
#include <unordered_map>
#include <stdexcept>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <Psapi.h>
#pragma comment(lib,"Psapi.lib")
#ifdef small
#undef small
#endif
#ifdef near
#undef near
#endif
#ifdef far
#undef far
#endif
#endif
// Test-only allocation traffic measurement, scoped to the calling thread.
namespace phase3aMemory {thread_local bool enabled{};thread_local std::size_t bytes{},calls{};}
void* operator new(std::size_t n) {
 if(void* p=std::malloc(n?n:1)){if(phase3aMemory::enabled){phase3aMemory::bytes+=n;++phase3aMemory::calls;}return p;}
 throw std::bad_alloc();
}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete[](void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void operator delete[](void* p,std::size_t) noexcept {std::free(p);}
using namespace harmony;

namespace {
const char* retrievalTimingModeName(RetrievalTimingMode mode) {
 switch(mode) {
  case RetrievalTimingMode::Legacy:return "Legacy";
  case RetrievalTimingMode::BarPositionIndependent:return "BarPositionIndependent";
  case RetrievalTimingMode::ContinuousStructure:return "ContinuousStructure";
  case RetrievalTimingMode::HarmonyFirst:return "HarmonyFirst";
 }
 return "Unknown";
}
const char* analysisFunctionName(HarmonicFunction f) {
 switch(f) {
  case HarmonicFunction::Tonic:return "Tonic"; case HarmonicFunction::TonicProlongation:return "TonicProlongation";
  case HarmonicFunction::Predominant:return "Predominant"; case HarmonicFunction::Dominant:return "Dominant";
  case HarmonicFunction::ChromaticPredominant:return "ChromaticPredominant";
  case HarmonicFunction::ChromaticDominant:return "ChromaticDominant"; default:return "Unknown";
 }
}
std::string joinLabels(const std::vector<std::string>& values) {
 std::ostringstream out;for(std::size_t i=0;i<values.size();++i){if(i)out<<" | ";out<<values[i];}return out.str();
}
struct RhythmFixture {
 std::string id; std::vector<std::string> chords; std::optional<KeySignature> key;
};
struct Phase3A0Case { std::string id; std::vector<std::string> names; std::vector<double> durations; KeySignature key; int octaveShift{}; };
std::vector<Phase3A0Case> phase3a0BaseCases() {
 return {
  {"R01_ORIGINAL",{"Am","G#dim","G","D/F#"},{4,4,4,4},{PitchClass::A,Mode::Minor},0},
  {"R01_TRANSPOSED_BMIN",{"Bm","A#dim","A","E/G#"},{4,4,4,4},{PitchClass::B,Mode::Minor},0},
  {"R01_INVERTED_PLUS_OCTAVE",{"Am/C","G#dim/B","G/B","D/F#"},{4,4,4,4},{PitchClass::A,Mode::Minor},12},
  {"PASSING_FSHARP_DIM7",{"F","F#dim7","G","C"},{4,0.25,4,4},{PitchClass::C,Mode::Major},0},
  {"NEG_DIM_RESOLVES_ONLY",{"Am","C#dim","D","E"},{4,4,4,4},{PitchClass::A,Mode::Minor},0},
  {"NEG_DIM_BREAKS_CONTEXT",{"Am","G#dim","C","D"},{4,4,4,4},{PitchClass::A,Mode::Minor},0},
  {"NEG_TARGET_QUALITY_CONFLICT",{"F","F#dim7","Gm","C"},{4,0.25,4,4},{PitchClass::C,Mode::Major},0},
  {"NEG_DIM_ONLY",{"G#dim"},{4},{PitchClass::A,Mode::Minor},0},
  {"CONTROL_NO_DIM",{"Am","G","D"},{4,4,4},{PitchClass::A,Mode::Minor},0}
 };
}
std::vector<Phase3A0Case> phase3a0AllCases() {
 auto samples=phase3a0BaseCases();
 samples.push_back({"NEG_ONE_SIDE",{"Am","G#dim","F","D"},{4,4,4,4},{PitchClass::A,Mode::Minor},0});
 samples.push_back({"NEG_DIRECTION_REVERSES",{"Am","G#dim","Am","D"},{4,4,4,4},{PitchClass::A,Mode::Minor},0});
 samples.push_back({"NEG_TWO_INSERTIONS",{"Am","G#dim","G","F#dim","F","D"},{4,4,4,4,4,4},{PitchClass::A,Mode::Minor},0});
 samples.push_back({"NEG_HALF_DIM",{"Am","G#m7b5","G","D"},{4,4,4,4},{PitchClass::A,Mode::Minor},0});
 samples.push_back({"NEG_NON_DIM_CHROMATIC",{"Am","G#","G","D"},{4,4,4,4},{PitchClass::A,Mode::Minor},0});
 samples.push_back({"NEG_SAME_ROOT_QUALITY",{"A","G#dim","G","D"},{4,4,4,4},{PitchClass::A,Mode::Minor},0});
 samples.push_back({"POS_DESC_DIM7",{"Am","G#dim7","G","D"},{4,4,4,4},{PitchClass::A,Mode::Minor},0});
 samples.push_back({"POS_SHORT_DIM",{"Am","G#dim","G","D"},{4,0.25,4,4},{PitchClass::A,Mode::Minor},0});
 samples.push_back({"R01_INVERSION_ONLY",{"Am/C","G#dim/B","G/B","D/F#"},{4,4,4,4},{PitchClass::A,Mode::Minor},0});
 samples.push_back({"R01_OCTAVE_DOWN",{"Am","G#dim","G","D/F#"},{4,4,4,4},{PitchClass::A,Mode::Minor},-12});
 samples.push_back({"R01_OCTAVE_UP",{"Am","G#dim","G","D/F#"},{4,4,4,4},{PitchClass::A,Mode::Minor},12});
 samples.push_back({"NEG_NO_SHARED_TONES",{"Am","G#dim","Gm","D"},{4,4,4,4},{PitchClass::A,Mode::Minor},0});
 constexpr const char* pitchNames[]{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
 for(int shift=0;shift<12;++shift)samples.push_back({"TRANSPOSE_"+std::to_string(shift),
  {std::string(pitchNames[(9+shift)%12])+"m",std::string(pitchNames[(8+shift)%12])+"dim",pitchNames[(7+shift)%12],pitchNames[(2+shift)%12]},
  {4,4,4,4},{static_cast<PitchClass>((9+shift)%12),Mode::Minor},0});
 return samples;
}
struct RhythmTrial {
 const RhythmFixture* fixture{}; std::string id, variant; int numerator{4},denominator{4};
 double bpm{120.},offset{};std::vector<double> durations;bool openEnded{};int semitoneShift{},octaveShift{};
 std::optional<PhraseIntent> requestedIntent;
};
int runPhase4BRhythmAudit(const std::filesystem::path& out,const library::LoadResult& db,const CandidateIndex& index) {
 std::filesystem::create_directories(out);
 std::ofstream cases(out/"phase4b-cases.tsv"),midiInputs(out/"phase4b-midi-inputs.tsv"),matches(out/"phase4b-matches.tsv"),
  stability(out/"phase4b-stability.tsv"),timeStability(out/"phase4b-time-scaling.tsv"),performance(out/"phase4b-performance.tsv"),stress(out/"phase4b-stress.tsv"),phase3a(out/"phase4b-phase3a.tsv"),
  exportCheck(out/"phase4b-midi-export-check.txt");
 if(!cases||!midiInputs||!matches||!stability||!timeStability||!performance||!stress||!phase3a||!exportCheck)return 70;
 std::map<std::string,const ProgressionTemplate*> factoryById;for(const auto& item:db.templates)factoryById.emplace(item.id,&item);
 cases<<std::setprecision(9)<<"trial\tvariant\tmode\tmeter\tbpm\trequested_intent\tbar_qn\topen\toffset_qn\tinput_qn\tseconds\trecognized\tkey\tstructural_weights\tretrieval_weights\troles\tskeleton_indices\tshortlist\tmatches\tpath_passed\tscore_passed\tresolve\tdevelop\tloop\tcolor\tcolor_ids\tvisible_ids\tthreshold\tlegacy_explicit_equivalent\n";
 midiInputs<<"trial\tpitch\tstart_qn\tduration_qn\tvelocity\tchannel\ttrack\n";
 matches<<std::setprecision(8)<<"trial\tmode\tmeter\tbpm\tFactoryID\tFactory_intent\tFactory_cadence\tFactory_path\tFactory_harmonic_path\tkey\tstart_index\tend_index\tcontinuation_index\tcontinuation_length\ttrace\tdisposition\tmatch_similarity\tfull_harmony\tskeleton_harmony\trhythm_similarity\teligibility_score\tranking_score\trank_group\trank\tvisible\tcontinuation\n";
 stability<<std::setprecision(8)<<"trial\tmode\tlegacy_visible\tmode_visible\tintersection\tunion\tjaccard\tlegacy_color\tmode_color\n";
 timeStability<<std::setprecision(8)<<"base_trial\tvariant_trial\tmode\tbase_visible\tvariant_visible\tintersection\tunion\tjaccard\tbase_color\tvariant_color\n";
 performance<<std::setprecision(8)<<"trial\tmode\trecommend_ms\tallocation_bytes\tallocation_calls\tmatch_count\tvisible_count\n";
 stress<<std::setprecision(8)<<"workload\tmode\tphase3a\truns\tp50_ms\tp95_ms\tmax_ms\tmean_allocation_bytes\tmean_allocation_calls\tmatch_count\tvisible_count\tcpu_ms\tworking_set_before\tworking_set_after\tpeak_working_set\n";
 phase3a<<std::setprecision(8)<<"record\ttrial\tmode\tphase3a\tFactoryID\tinput\tmatch_window\tcontinuation_index\tcontinuation\ttrace\tscore_before\tscore_after\tranking_score\teligibility_score\tdisposition\trefunded_points\teligible\tthreshold\tvisible_count\tcolor_count\tconditions\n";
 std::vector<RhythmFixture> fixtures{
  {"R01",{"Am","G#dim","G","D/F#"},KeySignature{PitchClass::A,Mode::Minor}},
  {"R01_D",{"Am","G#dim","G","D"},KeySignature{PitchClass::A,Mode::Minor}},
  {"R02_CMAJOR",{"F","G","Am"},KeySignature{PitchClass::C,Mode::Major}},
  {"R02_AMINOR",{"F","G","Am"},KeySignature{PitchClass::A,Mode::Minor}},
  {"B08",{"C","Cmaj7/B","C7/Bb","F/A"},KeySignature{PitchClass::C,Mode::Major}},
  {"B11",{"Cmaj9","Dbmaj9","Cmaj9"},KeySignature{PitchClass::C,Mode::Major}},
  {"DIATONIC",{"C","Am","F","G"},KeySignature{PitchClass::C,Mode::Major}},
  {"DIM_NOT_PASSING",{"C","Ddim","Em","G"},KeySignature{PitchClass::C,Mode::Major}},
  {"BORROWED",{"C","Fm","G","C"},KeySignature{PitchClass::C,Mode::Major}},
  {"COLOR",{"Cmaj7","Am7","Dm7","G7"},KeySignature{PitchClass::C,Mode::Major}},
  {"SECONDARY",{"Dm","E7","Am"},KeySignature{PitchClass::A,Mode::Minor}},
  {"SUS",{"Gsus4","G","C"},KeySignature{PitchClass::C,Mode::Major}}
 };
 struct Meter {int n,d;double barQN;};
 const std::vector<Meter> meters{{4,4,4.},{3,4,3.},{6,8,3.},{5,4,5.},{7,8,3.5},{9,8,4.5}};
 const std::array<RetrievalTimingMode,4> modes{RetrievalTimingMode::Legacy,RetrievalTimingMode::BarPositionIndependent,
  RetrievalTimingMode::ContinuousStructure,RetrievalTimingMode::HarmonyFirst};
 std::vector<RhythmTrial> trials;
 auto addTrial=[&](const RhythmFixture& f,const Meter& m,const std::string& variant,const std::vector<double>& d,
                   bool open=false,double bpm=120.,double offset=0.,int semitones=0,int octave=0,
                   std::optional<PhraseIntent> intent={}) {
  std::ostringstream id;id<<f.id<<"_"<<m.n<<"-"<<m.d<<"_"<<variant;
  if(bpm!=120.)id<<"_"<<static_cast<int>(bpm)<<"bpm";
  if(offset!=0.)id<<"_offset"<<offset;
  if(semitones)id<<"_transpose"<<semitones;
  if(octave)id<<"_octave"<<octave;
  trials.push_back({&f,id.str(),variant,m.n,m.d,bpm,offset,d,open,semitones,octave,intent});
 };
 const auto fixed=[](std::size_t n,double d){return std::vector<double>(n,d);};
 for(const auto& meter:meters)for(const auto& f:fixtures) {
  addTrial(f,meter,"fixed-4qn",fixed(f.chords.size(),4.));
  addTrial(f,meter,"one-bar",fixed(f.chords.size(),meter.barQN));
 }
 const auto findFixture=[&](const char* id)->const RhythmFixture& {return *std::find_if(fixtures.begin(),fixtures.end(),[&](const auto& f){return f.id==id;});};
 const auto& r01=findFixture("R01");const auto& r01d=findFixture("R01_D");
 const auto& r02c=findFixture("R02_CMAJOR");const auto& r02a=findFixture("R02_AMINOR");
 for(const auto& meter:meters) {
  for(double scale:{0.5,0.75,1.25,1.5,2.})addTrial(r01,meter,"whole-scale-"+std::to_string(scale),fixed(4,4.*scale));
  for(double d:{1.5,2.0,2.5,3.0,3.125,3.1748021039,3.25,3.5,3.75,4.0,4.25,4.5,5.0})
   addTrial(r01,meter,"middle-dim-"+std::to_string(d),{4.,d,4.,4.});
  addTrial(r01,meter,"open",fixed(4,4.),true);
  addTrial(r01,meter,"entry-offset",fixed(4,4.),false,120.,0.5);
  addTrial(r02c,meter,"R02-3-5-4",{3.,5.,4.});
  addTrial(r02c,meter,"R02-open",{4.,4.,4.},true);
  addTrial(r02a,meter,"R02-3-5-4",{3.,5.,4.});
  addTrial(r02a,meter,"R02-open",{4.,4.,4.},true);
 }
 const Meter fourFour{4,4,4.};
 for(double bpm:{90.,180.})addTrial(r01,fourFour,"fixed-4qn",fixed(4,4.),false,bpm);
 addTrial(r01,fourFour,"equal-seconds-2qn",fixed(4,2.),false,90.);
 addTrial(r01,fourFour,"equal-seconds-4qn",fixed(4,4.),false,180.);
 addTrial(r01,fourFour,"transposed",fixed(4,4.),false,120.,0.,2,0);
 addTrial(r01,fourFour,"octave-up",fixed(4,4.),false,120.,0.,0,1);
 addTrial(r01,fourFour,"octave-down",fixed(4,4.),false,120.,0.,0,-1);
 for(const auto& meter:meters)for(const auto* f:{&r02c,&r02a}) {
  addTrial(*f,meter,"R02-color-intent-fixed",fixed(3,4.),false,120.,0.,0,0,PhraseIntent::Color);
  addTrial(*f,meter,"R02-color-intent-3-5-4",{3.,5.,4.},false,120.,0.,0,0,PhraseIntent::Color);
  addTrial(*f,meter,"R02-color-intent-open",fixed(3,4.),true,120.,0.,0,0,PhraseIntent::Color);
 }
 std::map<std::string,std::map<RetrievalTimingMode,std::set<std::string>>> visibleByTrial;
 std::map<std::string,std::map<RetrievalTimingMode,std::size_t>> colorByTrial;
 std::size_t failures{},midiRoundTrips{},engineRuns{};double engineTotalMs{};
 std::size_t importPathChecks{},midiOutputChecks{};
 for(const auto& trial:trials) {
  Progression source;double cursor=trial.offset;
  for(std::size_t i=0;i<trial.fixture->chords.size();++i) {
   ChordEvent e;e.name=trial.fixture->chords[i];e.startQN=cursor;e.durationQN=trial.durations[i];e.openEnded=false;source.push_back(e);cursor+=trial.durations[i];
  }
  midi::ExportSequence sequence;sequence.totalQN=cursor;sequence.chordCount=source.size();sequence.tempoBPM=trial.bpm;
  sequence.meter={trial.numerator,trial.denominator};
  for(const auto& e:source) {
   const auto tones=chordPitches(e);const auto root=(tones.root+trial.semitoneShift+120)%12;
   const auto bass=(tones.bass+trial.semitoneShift+120)%12;const int octave=trial.octaveShift*12;
   sequence.notes.push_back({36+bass+octave,e.startQN,*e.durationQN,90,0,true});
   for(int n=0;n<12;++n)if(tones.intervals&(1<<n))sequence.notes.push_back({60+(root+n)%12+octave,e.startQN,*e.durationQN,80,0,false});
  }
  const auto bytes=midi::writeToMemory(sequence);if(!bytes){++failures;std::cerr<<"PH4B write failed "<<trial.id<<" "<<bytes.error<<'\n';continue;}
  const auto raw=midi::readFromMemory(bytes.bytes);if(!raw){++failures;std::cerr<<"PH4B parse failed "<<trial.id<<" "<<raw.error<<'\n';continue;}
  if(raw.file.tempos.empty()||raw.file.meters.empty()||std::abs(raw.file.tempos.front().bpm-trial.bpm)>0.1||
     raw.file.meters.front().numerator!=trial.numerator||raw.file.meters.front().denominator!=trial.denominator) {
   ++failures;std::cerr<<"PH4B MIDI metadata mismatch "<<trial.id<<'\n';continue;
  }
  midi::MidiHarmonyExtractionConfig extractionConfig;extractionConfig.openEnded=trial.openEnded;
  const auto extraction=midi::extractHarmony(raw.file,extractionConfig);++midiRoundTrips;
  if(!extraction||extraction.chords.size()!=source.size()) {
   ++failures;std::cerr<<"PH4B recognition failed "<<trial.id<<" expected="<<source.size()<<" got="<<extraction.chords.size()<<" error="<<extraction.error<<'\n';continue;
  }
  bool recognitionOkay=true;
  for(std::size_t i=0;i<source.size();++i) {
   const auto expected=chordPitches(source[i]);const auto actual=normalizeChord(extraction.chords[i]);
   const auto expectedRoot=(expected.root+trial.semitoneShift+120)%12,expectedBass=(expected.bass+trial.semitoneShift+120)%12;
   if(!actual.root||!actual.bass||int(*actual.root)!=expectedRoot||int(*actual.bass)!=expectedBass||actual.quality!=expected.quality||
      std::abs(extraction.chords[i].startQN-source[i].startQN)>0.001||
      (!trial.openEnded&&(!extraction.chords[i].durationQN||std::abs(*extraction.chords[i].durationQN-trial.durations[i])>0.001))||
      (trial.openEnded&&i+1==source.size()&&(!extraction.chords[i].openEnded||extraction.chords[i].durationQN)))recognitionOkay=false;
  }
  if(!recognitionOkay){++failures;std::cerr<<"PH4B recognized MIDI differs from fixture "<<trial.id<<'\n';continue;}
  if(trial.fixture->id=="R01"&&(trial.variant=="fixed-4qn"||trial.variant.rfind("equal-seconds-",0)==0)) {
   const auto importPath=out/("import-probe-"+trial.id+".mid");std::string importError;
   if(!midi::writeToFile(bytes.bytes,importPath,importError)){++failures;exportCheck<<"IMPORT_FILE_WRITE_FAILED "<<trial.id<<' '<<importError<<'\n';}
   else {const auto imported=midi::importFile(importPath,false);
    if(!imported||imported.midi.file.meters.empty()||imported.midi.file.tempos.empty()||
       imported.midi.file.meters.front().numerator!=trial.numerator||imported.midi.file.meters.front().denominator!=trial.denominator||
       std::abs(imported.midi.file.tempos.front().bpm-trial.bpm)>0.1||imported.extraction.chords.size()!=source.size()) {
     ++failures;exportCheck<<"IMPORT_FILE_METADATA_OR_EXTRACTION_FAILED "<<trial.id<<'\n';
    } else {++importPathChecks;exportCheck<<"IMPORT_FILE_PASSED "<<trial.id<<" bpm="<<imported.midi.file.tempos.front().bpm<<" meter="<<imported.midi.file.meters.front().numerator<<'/'<<imported.midi.file.meters.front().denominator<<" chords="<<imported.extraction.chords.size()<<" downstream_context=explicit test injection (Controller not invoked)\n";}
   }
  }
  for(const auto& n:raw.file.notes)midiInputs<<trial.id<<'\t'<<n.pitch<<'\t'<<n.startQN<<'\t'<<n.durationQN<<'\t'<<n.velocity<<'\t'<<n.channel<<'\t'<<n.track<<'\n';
  if(trial.fixture->id=="R01"&&trial.variant=="fixed-4qn"&&trial.numerator==4&&trial.denominator==4&&trial.bpm==120.) {
   std::string error;midi::writeToFile(bytes.bytes,out/"R01_input_4-4_120bpm_4qn.mid",error);
  }
  std::map<RetrievalTimingMode,std::set<std::string>> visibleSets;
  for(const auto mode:modes) {
   AnalysisContext context;context.forcedKey=trial.fixture->key;
   if(context.forcedKey&&trial.semitoneShift)context.forcedKey->tonic=static_cast<PitchClass>((static_cast<int>(context.forcedKey->tonic)+trial.semitoneShift+120)%12);
   context.timeSigNumerator=trial.numerator;
   context.timeSigDenominator=trial.denominator;context.retrievalTimingMode=mode;
   const auto analysis=analyzeHarmony(extraction.chords,context);
   const auto query=makeMatchQuery(extraction.chords,context);
   if(mode==RetrievalTimingMode::Legacy) {
    AnalysisContext defaultContext;defaultContext.forcedKey=context.forcedKey;defaultContext.timeSigNumerator=trial.numerator;defaultContext.timeSigDenominator=trial.denominator;
    const auto defaultQuery=makeMatchQuery(extraction.chords,defaultContext);
    const auto explicitRec=recommendContinuations(query,index);
    const auto defaultRec=recommendContinuations(defaultQuery,index);
    bool same=explicitRec.matches.size()==defaultRec.matches.size();
    for(std::size_t i=0;same&&i<explicitRec.matches.size();++i)
     same=explicitRec.matches[i].templateId==defaultRec.matches[i].templateId&&explicitRec.matches[i].similarity==defaultRec.matches[i].similarity;
    if(!same){++failures;std::cerr<<"PH4B Legacy flag changed default "<<trial.id<<'\n';}
   }
   RecommendationWeights weights;
   if(weights.minimumScore!=60.f){++failures;std::cerr<<"PH4B score threshold changed\n";}
   const auto beforeAlloc=phase3aMemory::bytes,beforeCalls=phase3aMemory::calls;
   phase3aMemory::enabled=true;const auto begin=std::chrono::steady_clock::now();
   RecommendationRequest request;request.preferredIntent=trial.requestedIntent;
   const auto rec=recommendContinuations(query,index,request,weights);
   const auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
   phase3aMemory::enabled=false;const auto allocationBytes=phase3aMemory::bytes-beforeAlloc,allocationCalls=phase3aMemory::calls-beforeCalls;
   ++engineRuns;engineTotalMs+=elapsed;
   std::size_t visibleCount{},colorCount{};std::string ids,colorIds;
   for(std::size_t g=0;g<rec.groups.size();++g)for(std::size_t rank=0;rank<rec.groups[g].size();++rank) {
    const auto& c=rec.groups[g][rank];++visibleCount;visibleSets[mode].insert(c.id);if(g==3){++colorCount;if(!colorIds.empty())colorIds+=',';colorIds+=c.id;}
    if(!ids.empty())ids+=',';ids+=c.id;
   }
   colorByTrial[trial.id][mode]=colorCount;visibleByTrial[trial.id][mode]=visibleSets[mode];
   std::ostringstream recognized,weightsOut,retrievalOut,rolesOut,skeletonOut,keyOut;
   for(std::size_t i=0;i<extraction.chords.size();++i){if(i){recognized<<'|';weightsOut<<'|';retrievalOut<<'|';rolesOut<<'|';}recognized<<extraction.chords[i].name;
    if(i<analysis.full.size()){weightsOut<<analysis.full[i].structuralWeight;retrievalOut<<analysis.full[i].retrievalStructuralWeight;
     rolesOut<<analysisFunctionName(analysis.full[i].function)<<':'<<analysis.full[i].roles<<'/'<<analysis.full[i].retrievalRoles;}}
   for(std::size_t i=0;i<analysis.skeletonIndices.size();++i){if(i)skeletonOut<<',';skeletonOut<<analysis.skeletonIndices[i];}
   for(const auto& k:query.interpretations){if(keyOut.tellp()>0)keyOut<<'|';keyOut<<formatKey(k.key.key)<<':'<<k.key.confidence;}
   double inputSeconds{};for(const auto d:trial.durations)inputSeconds+=d*60./trial.bpm;
   const auto barQN=trial.numerator*4.0/trial.denominator;
   cases<<trial.id<<'\t'<<trial.variant<<'\t'<<retrievalTimingModeName(mode)<<'\t'<<trial.numerator<<'/'<<trial.denominator<<'\t'<<trial.bpm<<'\t'<<(trial.requestedIntent?intentName(*trial.requestedIntent):"neutral")<<'\t'<<barQN<<'\t'<<trial.openEnded<<'\t'<<trial.offset<<'\t'<<cursor-trial.offset<<'\t'<<inputSeconds<<'\t'<<recognized.str()<<'\t'<<(analysis.selectedKey?formatKey(analysis.selectedKey->key):"none")<<'\t'<<weightsOut.str()<<'\t'<<retrievalOut.str()<<'\t'<<rolesOut.str()<<'\t'<<skeletonOut.str()<<'\t'<<rec.stats.shortlistSize<<'\t'<<rec.matches.size()<<'\t'<<rec.stats.pathPassed<<'\t'<<rec.stats.scorePassed<<'\t'<<rec.groups[0].size()<<'\t'<<rec.groups[1].size()<<'\t'<<rec.groups[2].size()<<'\t'<<colorCount<<'\t'<<colorIds<<'\t'<<ids<<'\t'<<weights.minimumScore<<'\t'<<1<<'\n';
   performance<<trial.id<<'\t'<<retrievalTimingModeName(mode)<<'\t'<<elapsed<<'\t'<<allocationBytes<<'\t'<<allocationCalls<<'\t'<<rec.matches.size()<<'\t'<<visibleCount<<'\n';
   for(const auto& m:rec.matches) {
    std::string trace;for(const auto& step:m.alignmentTrace){if(!trace.empty())trace+='|';trace+=(step.queryIndex?std::to_string(*step.queryIndex):"-");trace+=':';trace+=(step.templateIndex?std::to_string(*step.templateIndex):"-");trace+=':';trace+=alignmentOpName(step.operation);}
    std::string rankGroup="-",continuation;std::size_t rank=0;bool shown=false;
    for(std::size_t g=0;g<rec.groups.size();++g)for(std::size_t k=0;k<rec.groups[g].size();++k){const auto& c=rec.groups[g][k];
     if(c.primaryTemplate==m.templateId||std::find(c.supportingTemplates.begin(),c.supportingTemplates.end(),m.templateId)!=c.supportingTemplates.end()){
      shown=true;rankGroup=intentName(c.intent);rank=k+1;for(const auto& e:c.continuation){if(!continuation.empty())continuation+='|';continuation+=e.label+'@'+std::to_string(e.durationQN);}
     }}
    const auto factory=factoryById.find(m.templateId);
    const auto intent=factory==factoryById.end()?"unknown":intentName(factory->second->intent);
    const auto cadence=factory==factoryById.end()?"unknown":cadenceName(factory->second->cadence);
    std::string rawPath;
    if(factory!=factoryById.end()&&!factory->second->rawChords.empty())for(const auto& chord:factory->second->rawChords){if(!rawPath.empty())rawPath+=" | ";rawPath+=chord.name;}
    if(rawPath.empty())rawPath=joinLabels(m.templateLabels);
    matches<<trial.id<<'\t'<<retrievalTimingModeName(mode)<<'\t'<<trial.numerator<<'/'<<trial.denominator<<'\t'<<trial.bpm<<'\t'<<m.templateId<<'\t'<<intent<<'\t'<<cadence<<'\t'<<rawPath<<'\t'<<joinLabels(m.templateLabels)<<'\t'<<formatKey(m.key)<<'\t'<<m.templateMatchStart<<'\t'<<m.templateMatchEnd<<'\t'<<m.continuationStartIndex<<'\t'<<m.continuationLength<<'\t'<<trace<<'\t'<<m.disposition<<'\t'<<m.similarity<<'\t'<<m.subScores.fullHarmony<<'\t'<<m.subScores.skeletonHarmony<<'\t'<<m.subScores.rhythmSimilarity<<'\t'<<m.candidateEligibilityScore<<'\t'<<m.candidateRankingScore<<'\t'<<rankGroup<<'\t'<<rank<<'\t'<<shown<<'\t'<<continuation<<'\n';
   }
   const bool baselineMidi=trial.fixture->id=="R01"&&trial.variant=="fixed-4qn"&&trial.numerator==4&&trial.denominator==4&&trial.bpm==120.;
   const bool meterMidi=trial.fixture->id=="R01"&&trial.variant=="fixed-4qn"&&mode==RetrievalTimingMode::HarmonyFirst;
   const bool tempoMidi=trial.fixture->id=="R01"&&trial.variant=="fixed-4qn"&&trial.numerator==4&&trial.denominator==4&&trial.bpm!=120.&&mode==RetrievalTimingMode::HarmonyFirst;
   const bool equalSecondsMidi=trial.fixture->id=="R01"&&trial.variant.rfind("equal-seconds-",0)==0&&mode==RetrievalTimingMode::HarmonyFirst;
   if(baselineMidi||meterMidi||tempoMidi||equalSecondsMidi) {
    const ContinuationCandidate* selected=nullptr;for(const auto& group:rec.groups)if(!group.empty()){selected=&group.front();break;}
    if(selected){midi::CandidateExportContext exportContext;exportContext.imported.replace(extraction.chords,TimelineCoordinateMode::RelativeToSelection);
     exportContext.tempo=trial.bpm;exportContext.meter={trial.numerator,trial.denominator};exportContext.key=selected->key;
     const auto payload=midi::candidatePayload(exportContext,*selected);if(payload){std::string error;
      const auto midiOut=out/(std::string("R01_recommendation_")+trial.id+"_"+retrievalTimingModeName(mode)+".mid");
      if(midi::writeToFile(payload.payload.smfBytes,midiOut,error)){const auto check=midi::readFromFile(midiOut);
       if(check&&!check.file.tempos.empty()&&!check.file.meters.empty()&&std::abs(check.file.tempos.front().bpm-trial.bpm)<0.1&&check.file.meters.front().numerator==trial.numerator&&check.file.meters.front().denominator==trial.denominator) {
        bool pathMatches=true;double expectedStart{};std::ostringstream pathDiagnostic;
        const double anchor=extraction.chords.front().startQN;
        for(const auto& input:extraction.chords)if(input.durationQN)expectedStart=std::max(expectedStart,input.startQN-anchor+*input.durationQN);
        for(const auto& event:selected->continuation) {
         const auto expected=event.harmonicData?chordPitches(*event.harmonicData):preview::chordFromLabel(event.label,event.quality);
         std::set<int> expectedPitches,actualPitches;int lowestPitch=128;bool onsetFound{},tonesCompatible=true,durationsCompatible=true;
         for(int n=0;n<12;++n)if(expected.intervals&(1<<n))expectedPitches.insert((expected.root+n)%12);
         for(const auto& note:check.file.notes)if(std::abs(note.startQN-expectedStart)<0.001){
          onsetFound=true;actualPitches.insert(note.pitch%12);lowestPitch=std::min(lowestPitch,note.pitch);
          if(!expectedPitches.contains(note.pitch%12))tonesCompatible=false;
          if(std::abs(note.durationQN-event.durationQN)>0.001)durationsCompatible=false;
         }
         if(!onsetFound||!tonesCompatible||!durationsCompatible||!actualPitches.contains(expected.root)||lowestPitch%12!=expected.bass){
          pathMatches=false;pathDiagnostic<<" at="<<expectedStart<<" expected=";for(const auto p:expectedPitches)pathDiagnostic<<p<<',';
          pathDiagnostic<<" actual=";for(const auto p:actualPitches)pathDiagnostic<<p<<',';pathDiagnostic<<" root="<<expected.root<<" bass="<<expected.bass<<" lowest="<<lowestPitch<<" duration_expected="<<event.durationQN;
          pathDiagnostic<<" nearest=";double delta=1e9;for(const auto& note:check.file.notes)if(std::abs(note.startQN-expectedStart)<delta)delta=std::abs(note.startQN-expectedStart);pathDiagnostic<<delta;
         }
         expectedStart+=event.durationQN;
        }
        if(pathMatches){++midiOutputChecks;exportCheck<<retrievalTimingModeName(mode)<<" export/path passed; trial="<<trial.id<<" Factory="<<selected->primaryTemplate<<" notes="<<check.file.notes.size()<<" bpm="<<check.file.tempos.front().bpm<<" meter="<<check.file.meters.front().numerator<<'/'<<check.file.meters.front().denominator<<" continuation="<<selected->continuation.size()<<"\n";}
        else {++failures;exportCheck<<retrievalTimingModeName(mode)<<" export metadata passed but Factory continuation MIDI check failed:"<<pathDiagnostic.str()<<"\n";}
       }
       else {++failures;exportCheck<<retrievalTimingModeName(mode)<<" export metadata check failed\n";}
      }else {++failures;exportCheck<<retrievalTimingModeName(mode)<<" export write failed: "<<error<<"\n";}
     }else exportCheck<<retrievalTimingModeName(mode)<<" export unavailable: "<<payload.error<<"\n";
    }else exportCheck<<retrievalTimingModeName(mode)<<" has no visible R01 candidate\n";
    }else if(meterMidi||tempoMidi) {++failures;exportCheck<<retrievalTimingModeName(mode)<<" expected R01 candidate unavailable for export; trial="<<trial.id<<" visible="<<visibleCount<<"\n";}
    else exportCheck<<retrievalTimingModeName(mode)<<" no R01 visible candidate for optional export; trial="<<trial.id<<" visible="<<visibleCount<<"\n";
  }
  for(const auto mode:modes)if(mode!=RetrievalTimingMode::Legacy) {
   const auto& base=visibleSets[RetrievalTimingMode::Legacy];const auto& changed=visibleSets[mode];
   std::size_t intersection{};for(const auto& id:base)if(changed.contains(id))++intersection;const auto unionCount=base.size()+changed.size()-intersection;
   stability<<trial.id<<'\t'<<retrievalTimingModeName(mode)<<'\t'<<base.size()<<'\t'<<changed.size()<<'\t'<<intersection<<'\t'<<unionCount<<'\t'<<(unionCount?static_cast<double>(intersection)/unionCount:1.)<<'\t'<<colorByTrial[trial.id][RetrievalTimingMode::Legacy]<<'\t'<<colorByTrial[trial.id][mode]<<'\n';
  }
 }
 for(const auto& baseTrial:trials)if(baseTrial.variant=="fixed-4qn"&&baseTrial.bpm==120.&&baseTrial.offset==0.) {
  for(const auto& variant:trials)if(variant.fixture==baseTrial.fixture&&variant.numerator==baseTrial.numerator&&variant.denominator==baseTrial.denominator&&
      (variant.variant.rfind("whole-scale-",0)==0||variant.variant.rfind("middle-dim-",0)==0||variant.variant=="open"||variant.variant=="entry-offset")) {
   for(const auto mode:modes) {
    const auto& a=visibleByTrial[baseTrial.id][mode];const auto& b=visibleByTrial[variant.id][mode];std::size_t intersection{};
    for(const auto& id:a)if(b.contains(id))++intersection;const auto unionCount=a.size()+b.size()-intersection;
    timeStability<<baseTrial.id<<'\t'<<variant.id<<'\t'<<retrievalTimingModeName(mode)<<'\t'<<a.size()<<'\t'<<b.size()<<'\t'<<intersection<<'\t'<<unionCount<<'\t'<<(unionCount?static_cast<double>(intersection)/unionCount:1.)<<'\t'<<colorByTrial[baseTrial.id][mode]<<'\t'<<colorByTrial[variant.id][mode]<<'\n';
   }
  }
 }
 // Exercise all 33 Phase 3A inputs under both retrieval schemes and both
 // compensation states. This is test-only injection; the plugin default stays OFF.
 const std::array<RetrievalTimingMode,2> phase3aModes{RetrievalTimingMode::Legacy,RetrievalTimingMode::HarmonyFirst};
 std::size_t phase3aRuns{};
 for(const auto& fixture:phase3a0AllCases()) {
  Progression source;double at{};
  for(std::size_t i=0;i<fixture.names.size();++i){ChordEvent e;e.name=fixture.names[i];e.startQN=at;e.durationQN=fixture.durations[i];e.openEnded=false;source.push_back(e);at+=fixture.durations[i];}
  midi::ExportSequence sequence;sequence.tempoBPM=120.;sequence.meter={4,4};sequence.chordCount=source.size();sequence.totalQN=at;
  for(const auto& e:source){const auto tones=chordPitches(e);sequence.notes.push_back({36+tones.bass+fixture.octaveShift,e.startQN,*e.durationQN,90,0,true});
   for(int n=0;n<12;++n)if(tones.intervals&(1<<n))sequence.notes.push_back({60+tones.root+n+fixture.octaveShift,e.startQN,*e.durationQN,80,0,false});}
  const auto bytes=midi::writeToMemory(sequence);if(!bytes)return 72;
  const auto raw=midi::readFromMemory(bytes.bytes);if(!raw)return 72;
  const auto recognized=midi::extractHarmony(raw.file);if(!recognized||recognized.chords.size()!=fixture.names.size())return 72;
  std::ostringstream inputNames;for(std::size_t i=0;i<recognized.chords.size();++i){if(i)inputNames<<'|';inputNames<<recognized.chords[i].name;}
  for(const auto mode:phase3aModes) {
   AnalysisContext context;context.forcedKey=fixture.key;context.timeSigNumerator=4;context.timeSigDenominator=4;context.retrievalTimingMode=mode;
   const auto query=makeMatchQuery(recognized.chords,context);RecommendationRequest request;request.preferredIntent=PhraseIntent::Color;
   RecommendationWeights off,on;on.diminishedPassingInsertionException=true;
   const auto baseline=recommendContinuations(query,index,request,off),enabled=recommendContinuations(query,index,request,on);
   if(off.diminishedPassingInsertionException||RecommendationWeights{}.diminishedPassingInsertionException||
      off.minimumScore!=60.f||on.minimumScore!=60.f||baseline.insertionDiagnostics.size()!=0||
      baseline.matches.size()!=enabled.matches.size())return 72;
   std::size_t visible{},color{};std::string conditions;
   for(std::size_t group=0;group<baseline.groups.size();++group){visible+=baseline.groups[group].size();if(group==3)color=baseline.groups[group].size();}
   phase3a<<"run\t"<<fixture.id<<'\t'<<retrievalTimingModeName(mode)<<"\tOFF\t-\t"<<inputNames.str()<<"\t-\t-\t-\t-\t-1\t-1\t-1\t-1\toutput="<<baseline.stats.output<<"\t0\t0\t60\t"<<visible<<'\t'<<color<<"\tthreshold unchanged; default exception flag="<<RecommendationWeights{}.diminishedPassingInsertionException<<'\n';
   phase3a<<"run\t"<<fixture.id<<'\t'<<retrievalTimingModeName(mode)<<"\tON\t-\t"<<inputNames.str()<<"\t-\t-\t-\t-\t-1\t-1\t-1\t-1\toutput="<<enabled.stats.output<<"\t0\t0\t60\t"<<enabled.stats.output<<'\t'<<enabled.groups[3].size()<<"\tthreshold unchanged; Phase3A explicitly injected ON\n";
   phase3aRuns+=2;
   for(const auto& m:baseline.matches) {
    const auto n=std::find_if(enabled.matches.begin(),enabled.matches.end(),[&](const auto& x){return x.templateId==m.templateId;});
    if(n==enabled.matches.end())return 72;
    const auto d=std::find_if(enabled.insertionDiagnostics.begin(),enabled.insertionDiagnostics.end(),[&](const auto& x){return x.templateId==m.templateId;});
    const float refund=d==enabled.insertionDiagnostics.end()?0.f:d->refund;
    if(refund<0.f||(d!=enabled.insertionDiagnostics.end()&&refund>d->originalPenalty)||
       std::abs((n->candidateRankingScore-m.candidateRankingScore)-refund)>0.001f)return 72;
    if(d!=enabled.insertionDiagnostics.end())conditions="single="+std::to_string(d->singleInsertion)+",aligned="+std::to_string(d->reliableAlignment)+
      ",dim="+std::to_string(d->diminished)+",chromatic="+std::to_string(d->chromaticMotion)+",shared="+std::to_string(d->sharedTones)+
      ",core="+std::to_string(d->compatibleCore)+",continuation="+std::to_string(d->validContinuation)+",prior_explanation="+std::to_string(d->alreadyExplained);
    std::string trace,continuation;for(const auto& step:m.alignmentTrace){if(!trace.empty())trace+='|';trace+=(step.queryIndex?std::to_string(*step.queryIndex):"-");trace+=':';
     trace+=(step.templateIndex?std::to_string(*step.templateIndex):"-");trace+=':';trace+=alignmentOpName(step.operation);}
    const auto factory=factoryById.find(m.templateId);if(factory!=factoryById.end())for(std::size_t j=m.continuationStartIndex;j<factory->second->full.size();++j){if(!continuation.empty())continuation+='|';continuation+=formatMatchEvent(factory->second->full[j]);}
    phase3a<<"match\t"<<fixture.id<<'\t'<<retrievalTimingModeName(mode)<<"\tOFF\t"<<m.templateId<<'\t'<<inputNames.str()<<'\t'<<m.templateMatchStart<<'-'<<m.templateMatchEnd<<'\t'<<m.continuationStartIndex<<'\t'<<continuation<<'\t'<<trace<<'\t'<<m.candidateRankingScore<<'\t'<<m.candidateRankingScore<<'\t'<<m.candidateRankingScore<<'\t'<<m.candidateEligibilityScore<<'\t'<<m.disposition<<"\t0\t0\t60\t"<<visible<<'\t'<<color<<'\t'<<conditions<<'\n';
    const float scoreAfter=d!=enabled.insertionDiagnostics.end()?d->scoreAfter:n->candidateRankingScore;
    phase3a<<"match\t"<<fixture.id<<'\t'<<retrievalTimingModeName(mode)<<"\tON\t"<<m.templateId<<'\t'<<inputNames.str()<<'\t'<<m.templateMatchStart<<'-'<<m.templateMatchEnd<<'\t'<<m.continuationStartIndex<<'\t'<<continuation<<'\t'<<trace<<'\t'<<m.candidateRankingScore<<'\t'<<scoreAfter<<'\t'<<n->candidateRankingScore<<'\t'<<n->candidateEligibilityScore<<'\t'<<n->disposition<<'\t'<<refund<<'\t'<<(d!=enabled.insertionDiagnostics.end()&&d->eligible)<<"\t60\t"<<enabled.stats.output<<'\t'<<enabled.groups[3].size()<<'\t'<<conditions<<'\n';
   }
   if(fixture.id=="R01_ORIGINAL") {
    const auto v4=std::find_if(baseline.matches.begin(),baseline.matches.end(),[](const auto& m){return m.templateId=="V4_A08";});
    const auto v4On=std::find_if(enabled.matches.begin(),enabled.matches.end(),[](const auto& m){return m.templateId=="V4_A08";});
    if(v4==baseline.matches.end()||v4On==enabled.matches.end())return 72;
    const float expected=mode==RetrievalTimingMode::Legacy?57.40387f:61.722977f;
    if(std::abs(v4->candidateRankingScore-expected)>0.02f||
       std::abs((v4On->candidateRankingScore-v4->candidateRankingScore)-8.f)>0.02f)return 72;
   }
  }
 }
 if(phase3aRuns!=33u*2u*2u)return 72;
 phase3a<<"SUMMARY\tALL_33\tLegacy+HarmonyFirst\tOFF+ON\t132 runs; both production defaults OFF; threshold=60; Phase3A refund never exceeded original insertion penalty\n";
 struct StressInput {std::string name;Progression source;std::optional<KeySignature> key;int numerator{4},denominator{4};};
 const auto buildStressInput=[](std::string name,std::initializer_list<const char*> labels,std::optional<KeySignature> key,int n=4,int d=4,bool open=false){
  StressInput input;input.name=std::move(name);input.key=key;input.numerator=n;input.denominator=d;double at{};
  for(const auto* label:labels){ChordEvent event;event.name=label;event.startQN=at;event.durationQN=4.;event.openEnded=false;input.source.push_back(event);at+=4.;}
  if(open&&!input.source.empty()){input.source.back().durationQN.reset();input.source.back().openEnded=true;}
  return input;
 };
 std::vector<StressInput> stressInputs;
 stressInputs.push_back(buildStressInput("R01_4-4",{"Am","G#dim","G","D/F#"},KeySignature{PitchClass::A,Mode::Minor}));
 stressInputs.push_back(buildStressInput("R01_OPEN_4-4",{"Am","G#dim","G","D/F#"},KeySignature{PitchClass::A,Mode::Minor},4,4,true));
 stressInputs.push_back(buildStressInput("R01_7-8",{"Am","G#dim","G","D/F#"},KeySignature{PitchClass::A,Mode::Minor},7,8));
 stressInputs.push_back(buildStressInput("B08_LONG_4-4",{"C","Cmaj7/B","C7/Bb","F/A","Fm/Ab","C/G","G7","C"},KeySignature{PitchClass::C,Mode::Major}));
 stressInputs.push_back(buildStressInput("B11_EXTENDED_4-4",{"Cmaj9","Dbmaj9","Cmaj9","Am9","Dm9","G13","Cmaj9"},KeySignature{PitchClass::C,Mode::Major}));
 const auto processCpuMs=[](){
#if defined(_WIN32)
  FILETIME created{},exited{},kernel{},user{};if(GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernel,&user)){
   ULARGE_INTEGER k{},u{};k.LowPart=kernel.dwLowDateTime;k.HighPart=kernel.dwHighDateTime;u.LowPart=user.dwLowDateTime;u.HighPart=user.dwHighDateTime;
   return static_cast<double>(k.QuadPart+u.QuadPart)/10000.0;}
#endif
  return 0.0;
 };
 const auto processMemory=[](){std::pair<std::size_t,std::size_t> result{};
#if defined(_WIN32)
  PROCESS_MEMORY_COUNTERS_EX counters{};counters.cb=sizeof(counters);
  if(GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PPROCESS_MEMORY_COUNTERS>(&counters),sizeof(counters)))
   result={static_cast<std::size_t>(counters.WorkingSetSize),static_cast<std::size_t>(counters.PeakWorkingSetSize)};
#endif
  return result;
 };
 const auto writeStressRow=[&](const std::string& workload,const std::string& mode,const std::string& phase,
                               std::vector<double> times,std::size_t allocBytes,std::size_t allocCalls,std::size_t runs,
                               std::size_t matchCount,std::size_t visibleCount,double cpuDelta,
                               std::pair<std::size_t,std::size_t> memBefore,std::pair<std::size_t,std::size_t> memAfter){
  if(times.empty())return;std::sort(times.begin(),times.end());
  stress<<workload<<'\t'<<mode<<'\t'<<phase<<'\t'<<runs<<'\t'<<times[times.size()/2]<<'\t'<<times[std::min(times.size()-1,times.size()*95/100)]<<'\t'<<times.back()<<'\t'
   <<static_cast<double>(allocBytes)/runs<<'\t'<<static_cast<double>(allocCalls)/runs<<'\t'<<matchCount<<'\t'<<visibleCount<<'\t'<<cpuDelta<<'\t'
   <<memBefore.first<<'\t'<<memAfter.first<<'\t'<<std::max(memBefore.second,memAfter.second)<<'\n';
 };
 const auto stressModes=std::array<RetrievalTimingMode,2>{RetrievalTimingMode::Legacy,RetrievalTimingMode::HarmonyFirst};
 constexpr std::size_t stressRuns=60;
 for(const auto& workload:stressInputs)for(const auto mode:stressModes)for(const bool exceptionOn:{false,true}) {
  AnalysisContext context;context.forcedKey=workload.key;context.timeSigNumerator=workload.numerator;context.timeSigDenominator=workload.denominator;context.retrievalTimingMode=mode;
  RecommendationRequest request;request.preferredIntent=PhraseIntent::Color;RecommendationWeights weights;weights.diminishedPassingInsertionException=exceptionOn;
  std::vector<double> times;std::size_t allocBytes{},allocCalls{},matchCount{},visibleCount{};std::set<std::string> stableSet;bool stable=true;
  const auto cpuBefore=processCpuMs();const auto memBefore=processMemory();
  for(std::size_t run=0;run<stressRuns;++run){const auto beforeBytes=phase3aMemory::bytes,beforeCalls=phase3aMemory::calls;
   phase3aMemory::enabled=true;const auto started=std::chrono::steady_clock::now();const auto query=makeMatchQuery(workload.source,context);
   const auto rec=recommendContinuations(query,index,request,weights);const auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();phase3aMemory::enabled=false;
   times.push_back(elapsed);allocBytes+=phase3aMemory::bytes-beforeBytes;allocCalls+=phase3aMemory::calls-beforeCalls;matchCount=rec.matches.size();visibleCount=rec.stats.output;
   std::set<std::string> ids;for(const auto& group:rec.groups)for(const auto& candidate:group)ids.insert(candidate.id);
   if(run==0)stableSet=ids;else if(ids!=stableSet)stable=false;
  }
  if(!stable){++failures;stress<<"UNSTABLE_"<<workload.name<<'\t'<<retrievalTimingModeName(mode)<<'\t'<<exceptionOn<<"\t"<<stressRuns<<"\t-1\t-1\t-1\t0\t0\t"<<matchCount<<'\t'<<visibleCount<<"\t0\t0\t0\t0\n";}
  const auto memAfter=processMemory();writeStressRow(workload.name+(stable?"":"_ERROR"),retrievalTimingModeName(mode),exceptionOn?"ON":"OFF",std::move(times),allocBytes,allocCalls,stressRuns,matchCount,visibleCount,processCpuMs()-cpuBefore,memBefore,memAfter);
 }
 // Alternate all four mode/exception combinations on the same source to verify
 // that switching API flags is deterministic and does not contaminate later calls.
 {
  const auto& workload=stressInputs.front();std::array<std::array<std::set<std::string>,2>,2> references{};
  for(std::size_t mi=0;mi<stressModes.size();++mi)for(std::size_t pi=0;pi<2;++pi){AnalysisContext c;c.forcedKey=workload.key;c.timeSigNumerator=4;c.timeSigDenominator=4;c.retrievalTimingMode=stressModes[mi];
   RecommendationWeights w;w.diminishedPassingInsertionException=pi!=0;RecommendationRequest request;request.preferredIntent=PhraseIntent::Color;
   const auto rec=recommendContinuations(makeMatchQuery(workload.source,c),index,request,w);
   for(const auto& group:rec.groups)for(const auto& item:group)references[mi][pi].insert(item.id);}
  std::vector<double> times;std::size_t allocBytes{},allocCalls{},matchCount{},visibleCount{};bool stable=true;const auto cpuBefore=processCpuMs();const auto memBefore=processMemory();
  for(std::size_t run=0;run<80;++run){const auto mi=run%2,pi=(run/2)%2;AnalysisContext c;c.forcedKey=workload.key;c.timeSigNumerator=4;c.timeSigDenominator=4;c.retrievalTimingMode=stressModes[mi];
   RecommendationWeights w;w.diminishedPassingInsertionException=pi!=0;RecommendationRequest request;request.preferredIntent=PhraseIntent::Color;
   const auto beforeBytes=phase3aMemory::bytes,beforeCalls=phase3aMemory::calls;phase3aMemory::enabled=true;
   const auto started=std::chrono::steady_clock::now();const auto rec=recommendContinuations(makeMatchQuery(workload.source,c),index,request,w);times.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count());phase3aMemory::enabled=false;
   matchCount=rec.matches.size();visibleCount=rec.stats.output;
   allocBytes+=phase3aMemory::bytes-beforeBytes;allocCalls+=phase3aMemory::calls-beforeCalls;std::set<std::string> ids;for(const auto& group:rec.groups)for(const auto& item:group)ids.insert(item.id);
   if(ids!=references[mi][pi])stable=false;
  }
  if(!stable)++failures;const auto memAfter=processMemory();writeStressRow("R01_TOGGLE_4STATES",stable?"alternating":"unstable","OFF/ON",std::move(times),allocBytes,allocCalls,80,matchCount,visibleCount,processCpuMs()-cpuBefore,memBefore,memAfter);
 }
 // Re-import one fixed MIDI file repeatedly through the production import API.
 {
  const auto inputFile=out/"R01_input_4-4_120bpm_4qn.mid";std::vector<double> times;std::size_t imports{};const auto cpuBefore=processCpuMs();const auto memBefore=processMemory();
  if(std::filesystem::exists(inputFile))for(std::size_t i=0;i<60;++i){const auto started=std::chrono::steady_clock::now();const auto imported=midi::importFile(inputFile,false);
   times.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count());if(!imported||imported.extraction.chords.size()!=4){++failures;break;}++imports;}
  else ++failures;const auto memAfter=processMemory();writeStressRow("R01_REPEATED_MIDI_IMPORT","Importer","not-applicable",std::move(times),0,0,imports,0,0,processCpuMs()-cpuBefore,memBefore,memAfter);
 }
 std::size_t defaults{};for(const auto& fixture:fixtures)defaults+=fixture.chords.size();
 exportCheck<<"SMF round-trips="<<midiRoundTrips<<"; importFile probes="<<importPathChecks<<"; output MIDI path checks="<<midiOutputChecks<<"; recommendation runs="<<engineRuns<<"; Phase3A matrix runs="<<phase3aRuns<<"; failures="<<failures<<"; wall engine ms="<<engineTotalMs<<"; fixed random seed=none\n";
 std::cout<<"PHASE4B_RHYTHM trials="<<trials.size()<<" smf_roundtrips="<<midiRoundTrips<<" runs="<<engineRuns<<" engine_ms="<<engineTotalMs<<" failures="<<failures<<" output="<<out.string()<<'\n';
 (void)db;(void)defaults;
 return failures?71:0;
}
}
int main(int argc,char** argv) {
 const std::filesystem::path out=argc>1?argv[1]:"reliability";
 std::filesystem::create_directories(out);
 const auto db=library::loadFactory(HC_FACTORY_DB_PATH); if(!db)return 2;
 const CandidateIndex index(db.templates);
 if(argc>2&&std::string(argv[2])=="rhythm-decoupling")return runPhase4BRhythmAudit(out,db,index);
 std::ofstream table(out/"matrix.tsv"),events(out/"events.tsv");
 table<<"input\trhythm\timport\topen\tforced\tkeys\tmatches\tcontinuations\tpolish\trich\tadvanced\tspecial\tstyle\tintent\tbestSimilarity\tkeyHypotheses\tshortlist\ttail\ttones\tpaths\tdeduped\tscorePassed\tuiVisible\n";
 events<<"input\trhythm\timport\tname\troot\tquality\tbass\tintervals\tcolors\tstart\tduration\topen\n";
 const std::vector<std::vector<std::string>> cases{{"C","Cmaj7/B"},{"C","Cmaj7/B","C7/Bb"},{"C","Cmaj7/B","C7/Bb","F/A"},{"C","Cmaj7/B","C7/Bb","F/A","Fm/Ab"},{"E7","A7"},{"E7","A7","D7"},{"C","B7","Em"},{"Cmaj9","Dbmaj9"},{"Cmaj9","Dbmaj9","Cmaj9"},{"C","Am","F","G"},{"C","F","G","C"},{"Am","Dm","E7","Am"},{"Dm7","G7","Cmaj7"},{"Cmaj7","Am7","Dm7","G7"},{"Cmaj9","Fmaj9","G13","Cmaj9"}};
 int failures=0;
 for(std::size_t ci=0;ci<cases.size();++ci) for(int rhythm=0;rhythm<8;++rhythm) {
  Progression source;double at= rhythm==4?0.375:0.;
  for(std::size_t i=0;i<cases[ci].size();++i){
   ChordEvent e;e.name=cases[ci][i];e.startQN=at;e.openEnded=false;
   const double d=rhythm==0?4.:rhythm==1?2.:rhythm==2?(i%2?2.:6.):rhythm==3?(i%2?4./3:8./3):rhythm==4?(i%2?1.5:2.5):rhythm==5?1.375:rhythm==6?(i%2?3.98:4.02):4.;
   e.durationQN=d;at+=d;source.push_back(e);
  }
  if(rhythm==7&&ci==10){auto& left=source[1];*left.durationQN-=0.25;ChordEvent passing;passing.name="F#dim7";passing.startQN=source[2].startQN-0.25;passing.durationQN=0.25;passing.openEnded=false;source.insert(source.begin()+2,passing);}
  midi::ExportSequence seq;seq.totalQN=at;seq.chordCount=source.size();
  for(const auto& e:source){const auto p=chordPitches(e);seq.notes.push_back({36+p.bass,e.startQN,*e.durationQN,90,0,true});for(int n=0;n<12;++n)if(p.intervals&(1<<n)){const double offset=rhythm==6?n*.001:0.;seq.notes.push_back({60+p.root+n,e.startQN+offset,*e.durationQN-offset,80,0,false});}}
  const auto bytes=midi::writeToMemory(seq);std::string error;const auto path=out/(std::to_string(ci)+"-"+std::to_string(rhythm)+".mid");
  if(!bytes||!midi::writeToFile(bytes.bytes,path,error))return 3;
  const auto read=midi::readFromFile(path);if(!read)return 4;
  const auto fileImport=midi::importFile(path,false);
  const auto& extracted=fileImport.extraction;if(!fileImport){std::cerr<<"extract "<<ci<<' '<<extracted.error<<'\n';++failures;continue;}
  for(int imported=0;imported<3;++imported)for(int open=0;open<2;++open)for(int forced=0;forced<2;++forced){
   auto p=imported==1?extracted.chords:source;
   if(imported==2)for(auto& e:p){const auto tones=chordPitches(e);std::ostringstream mask;mask<<"0x"<<std::hex<<tones.intervals;e.extensions.mask=mask.str();e.bassNoteValue=tones.bass;e.keyNoteValue=tones.root;}
#if defined(HC_RELIABILITY_PLUGIN)
   if(imported==2){std::ostringstream xml;xml<<"<vst-xml version=\"1.3\"><sourceApp>CubaseFixture</sourceApp>";std::size_t id=0;
    constexpr const char* notes[]{"C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B"};
    for(const auto& e:p){const auto tones=chordPitches(e);xml<<"<chord id=\""<<++id<<"\"><name>"<<e.name<<"</name><projectTime domain=\"quarterNotes\">"<<e.startQN<<"</projectTime><keyNote>"<<*e.keyNoteValue<<"</keyNote><bassNote>"<<*e.bassNoteValue<<"</bassNote><mask>"<<*e.extensions.mask<<"</mask><pitches>";for(int n=0;n<12;++n)if(tones.intervals&(1<<n))xml<<notes[(tones.root+n)%12]<<';';xml<<"</pitches></chord>";}xml<<"</vst-xml>";
    const auto parsed=plugin::VstXmlChordParser{}.parse(xml.str());if(!parsed.ok())return 6;p=parsed.chords;
    // XML has no final duration; CLOSED is the explicit, independently tested user override.
    if(!open){p.back().durationQN=source.back().durationQN;p.back().openEnded=false;}
   }
#endif
   if(open){p.back().durationQN.reset();p.back().openEnded=true;}
   for(const auto& e:p){const auto tones=chordPitches(e);events<<ci<<'\t'<<rhythm<<'\t'<<imported<<'\t'<<e.name<<'\t'<<tones.root<<'\t'<<int(tones.quality)<<'\t'<<tones.bass<<'\t'<<tones.intervals<<'\t'<<tones.colorMask<<'\t'<<e.startQN<<'\t'<<e.durationQN.value_or(-1)<<'\t'<<e.openEnded<<'\n';}
   AnalysisContext ctx;if(forced)ctx.forcedKey=KeySignature{ci==11?PitchClass::A:PitchClass::C,ci==11?Mode::Minor:Mode::Major};
   const auto query=makeMatchQuery(p,ctx);const auto analysis=analyzeHarmony(p,ctx);
   RecommendationWeights w;w.perGroup=10;const auto rec=recommendContinuations(query,index,{},w);const auto enrich=enrichment::enrichProgression(p,analysis);
   auto enabledWeights=w;enabledWeights.diminishedPassingInsertionException=true;
   const auto enabledRec=recommendContinuations(query,index,{},enabledWeights);
   // Existing 1440-case matrix must not change for this narrow insertion rule.
   for(const auto& d:enabledRec.insertionDiagnostics)if(d.refund!=0)return 50;
   for(std::size_t g=0;g<rec.groups.size();++g){if(rec.groups[g].size()!=enabledRec.groups[g].size())return 51;
    for(std::size_t i=0;i<rec.groups[g].size();++i)if(rec.groups[g][i].id!=enabledRec.groups[g][i].id||rec.groups[g][i].rankingScore!=enabledRec.groups[g][i].rankingScore)return 52;}
   std::size_t count=0;bool special=false;const char* target=ci<4?"V4_B08":ci<6?"V4_B01":ci==7||ci==8?"V4_B11":"";
   for(const auto& g:rec.groups)for(const auto& c:g){++count;if(c.primaryTemplate==target||std::find(c.supportingTemplates.begin(),c.supportingTemplates.end(),target)!=c.supportingTemplates.end()){special=true;if(argc>2&&c.continuationStart!=p.size())return 15;}}
   if(argc>2){
    if(rec.stats.output!=count||rec.stats.output>rec.stats.scorePassed||rec.stats.scorePassed>rec.stats.distinct)return 19;
    // Every source node survives a safe upgrade; source tones/bass cannot be lost.
    for(const auto& g:enrich.groups)for(const auto& c:g){
     if(c.operations.empty())return 7;
     for(const auto& e:p){const auto found=std::find_if(c.progression.begin(),c.progression.end(),[&](const auto& x){return std::abs(x.startQN-e.startQN)<1e-6;});if(found==c.progression.end())return 8;
      if(imported){const auto a=chordPitches(e),b=chordPitches(*found);if(a.root!=b.root||a.bass!=b.bass||(a.intervals&b.intervals)!=a.intervals)return 9;}}
     ImportedProgressionSession session;if(!session.replace(c.progression,TimelineCoordinateMode::RelativeToSelection))return 10;
     const auto preview=preview::buildSequence(session,nullptr,120);if(!preview)return 11;
     const auto clip=midi::buildClip(preview.sequence,midi::ArrangementMode::VoiceLed,midi::ExportScope::FullPhrase,{});if(!clip)return 12;
     for(const auto& e:preview.sequence.events){unsigned mask{};int bass=-1;for(const auto& n:clip.sequence.notes)if(std::abs(n.startQN-e.startQN)<1e-6){if(n.bass)bass=n.midiNote%12;else mask|=1u<<((n.midiNote-e.chord.root+120)%12);}if(bass!=e.chord.bass||(e.chord.exactIntervals&&mask!=e.chord.intervals))return 13;}
     for(auto locale:{session::Locale::ZhCN,session::Locale::EnUS})if(ui::functionalWhy(c,locale).empty())return 14;
     ImportedProgressionSession original;original.replace(p,TimelineCoordinateMode::RelativeToSelection);
     const auto saved=snapshot::captureEnrichment(original,c,120,4,4,ctx.forcedKey,{});
     const auto restored=snapshot::deserializeEnrichment(snapshot::serialize(saved));if(!restored||restored.value.candidate.fingerprint!=c.fingerprint)return 16;
    }
   }
   std::string hypotheses;for(const auto& k:query.interpretations)hypotheses+=formatKey(k.key.key)+":"+std::to_string(k.key.confidence)+";";
   const auto shown=session::presentationIndices(rec,{});std::size_t visible{};for(const auto& g:shown)visible+=g.size();
   table<<ci<<'\t'<<rhythm<<'\t'<<imported<<'\t'<<open<<'\t'<<forced<<'\t'<<query.interpretations.size()<<'\t'<<rec.matches.size()<<'\t'<<count<<'\t'<<enrich.groups[0].size()<<'\t'<<enrich.groups[1].size()<<'\t'<<enrich.groups[2].size()<<'\t'<<special<<"\tAUTO\tAUTO\t"<<(rec.matches.empty()?0:rec.matches.front().similarity)<<'\t'<<hypotheses<<'\t'<<rec.stats.shortlistSize<<'\t'<<rec.stats.tailPassed<<'\t'<<rec.stats.realizationPassed<<'\t'<<rec.stats.pathPassed<<'\t'<<rec.stats.distinct<<'\t'<<rec.stats.scorePassed<<'\t'<<visible<<'\n';
#if defined(HC_RELIABILITY_PLUGIN)
   if(argc>2&&rhythm==0&&imported==2&&!forced) {
    plugin::RecommendationWorker worker(HC_FACTORY_DB_PATH,out/("worker-user-"+std::to_string(ci)+"-"+std::to_string(open)+".db"));
    const auto generation=worker.submit(p,ctx,{},false,1);std::optional<plugin::WorkerResult> result;
    for(int wait=0;wait<1000&&!result;++wait){result=worker.takeLatest();if(!result)std::this_thread::sleep_for(std::chrono::milliseconds(5));}
    if(!result||!result->error.empty()||result->generation!=generation||result->factoryCount!=657||result->recommendations.stats.output!=count)return 17;
    for(std::size_t g=0;g<3;++g)if(result->enrichments.groups[g].size()!=enrich.groups[g].size())return 18;
   }
#endif
   if(argc>2&&(!count||(target[0]&&!special)||(ci>=9&&ci<=13&&enrich.groups[0].empty()))){++failures;std::cerr<<"FAIL "<<ci<<" rhythm="<<rhythm<<" midi="<<imported<<" open="<<open<<" forced="<<forced<<" cont="<<count<<" special="<<special<<" polish="<<enrich.groups[0].size()<<'\n';}
  }
 }
 // Phase 1B controlled synthetic reproductions. These fixtures extend this existing
 // reliability test; they are generated MIDI, not captured Cubase input.
 struct Phase1BCase { std::string id; std::vector<std::string> names; std::vector<double> durations; bool openEnded; };
 const std::vector<Phase1BCase> phase1bCases{
  {"R01_D_SLASH_FSHARP",{"Am","G#dim","G","D/F#"},{4,4,4,4},false},
  {"R01_D_PLAIN",{"Am","G#dim","G","D"},{4,4,4,4},false},
  {"R01_PREFIX_AM_GSHARP_DIM",{"Am","G#dim"},{4,4},false},
  {"R01_LOCAL_GSHARP_DIM_G_D_SLASH_FSHARP",{"G#dim","G","D/F#"},{4,4,4},false},
  {"R02_T1",{"F","G","Am"},{4,4,4},false},
  {"R02_T2",{"F","G","Am"},{4,4,4},true},
  {"R02_T3",{"F","G","Am"},{3,5,4},false},
  {"R02_T4",{"F","G","Am"},{3,5,4},true}
 };
 std::ofstream phase1bInputs(out/"phase1b-inputs.tsv"),phase1bDiagnostics(out/"phase1b-diagnostics.txt");
 if(!phase1bInputs||!phase1bDiagnostics)return 20;
 phase1bInputs<<std::setprecision(17);
 phase1bInputs<<"case\trecord\tindex\tname_or_pitch\troot_pc\tquality_enum\tbass_pc\tinterval_mask\tstart_qn\tduration_qn\tend_qn\tOPEN\tvelocity\tchannel\ttrack\n";
 for(const auto& fixture:phase1bCases) {
  Progression source;double cursor{};
  for(std::size_t i=0;i<fixture.names.size();++i) {
   ChordEvent event;event.name=fixture.names[i];event.startQN=cursor;event.durationQN=fixture.durations[i];event.openEnded=false;
   source.push_back(event);cursor+=fixture.durations[i];
  }
  midi::ExportSequence sequence;sequence.totalQN=cursor;sequence.chordCount=source.size();sequence.tempoBPM=120.;sequence.meter={4,4};
  for(const auto& event:source) {
   const auto tones=chordPitches(event);
   sequence.notes.push_back({36+tones.bass,event.startQN,*event.durationQN,90,0,true});
   for(int n=0;n<12;++n)if(tones.intervals&(1<<n))sequence.notes.push_back({60+tones.root+n,event.startQN,*event.durationQN,80,0,false});
  }
  const auto bytes=midi::writeToMemory(sequence);std::string midiError;
  const auto midiPath=out/("phase1b-"+fixture.id+".mid");
  if(!bytes||!midi::writeToFile(bytes.bytes,midiPath,midiError)){std::cerr<<"Phase1B MIDI write failed "<<fixture.id<<" "<<midiError<<'\n';return 21;}
  const auto raw=midi::readFromFile(midiPath);if(!raw){std::cerr<<"Phase1B MIDI read failed "<<fixture.id<<" "<<raw.error<<'\n';return 22;}
  std::size_t noteIndex{};
  for(const auto& note:raw.file.notes) {
   const double end=note.startQN+note.durationQN;
   phase1bInputs<<fixture.id<<"\tMIDI_NOTE\t"<<noteIndex++<<'\t'<<note.pitch<<"\t-1\t-1\t-1\t-1\t"<<note.startQN<<'\t'<<note.durationQN<<'\t'<<end<<"\t0\t"<<note.velocity<<'\t'<<note.channel<<'\t'<<note.track<<'\n';
  }
  const auto imported=midi::importFile(midiPath,fixture.openEnded);
  if(!imported){std::cerr<<"Phase1B MIDI extraction failed "<<fixture.id<<" "<<imported.extraction.error<<'\n';return 23;}
  const auto& recognized=imported.extraction.chords;
  if(recognized.size()!=source.size()){std::cerr<<"Phase1B chord count mismatch "<<fixture.id<<" expected="<<source.size()<<" got="<<recognized.size()<<'\n';return 24;}
  for(std::size_t i=0;i<recognized.size();++i) {
   const auto expected=chordPitches(source[i]);const auto actual=normalizeChord(recognized[i]);
   if(!actual.root||!actual.bass||static_cast<int>(*actual.root)!=expected.root||static_cast<int>(*actual.bass)!=expected.bass||actual.quality!=expected.quality||
      std::abs(recognized[i].startQN-source[i].startQN)>1e-6||(!fixture.openEnded&&(!recognized[i].durationQN||std::abs(*recognized[i].durationQN-fixture.durations[i])>1e-6))||
      (fixture.openEnded&&i+1==recognized.size()&&(recognized[i].durationQN||!recognized[i].openEnded))) {
    std::cerr<<"Phase1B recognition mismatch "<<fixture.id<<" chord="<<i<<" name="<<recognized[i].name<<'\n';return 25;
   }
   phase1bInputs<<fixture.id<<"\tRECOGNIZED_CHORD\t"<<i<<'\t'<<recognized[i].name<<'\t'<<int(*actual.root)<<'\t'<<int(actual.quality)<<'\t'<<int(*actual.bass)<<'\t'<<actual.intervalMask<<'\t'<<recognized[i].startQN<<'\t'<<recognized[i].durationQN.value_or(-1)<<'\t'<<(recognized[i].durationQN?recognized[i].startQN+*recognized[i].durationQN:-1)<<'\t'<<recognized[i].openEnded<<"\t-1\t-1\t-1\n";
  }
  AnalysisContext context;context.forcedKey=KeySignature{PitchClass::A,Mode::Minor};context.timeSigNumerator=4;context.timeSigDenominator=4;
  RecommendationRequest request;request.preferredIntent=PhraseIntent::Color;
  RecommendationWeights weights;
  const auto query=makeMatchQuery(recognized,context);const auto rec=recommendContinuations(query,index,request,weights);
  const auto analysis=analyzeHarmony(recognized,context);
  // Phase 2 offline score audit: expose the existing score terms for the
  // five R01 candidates that stop at the ordinary score gate. No weights or
  // production behavior are changed by this diagnostic.
  if(fixture.id.rfind("R01_",0)==0&&rec.stats.pathPassed==5)for(const auto& match:rec.matches)if(match.disposition=="minimum_score") {
   const auto itemIt=std::find_if(db.templates.begin(),db.templates.end(),[&](const auto& item){return item.id==match.templateId;});
   if(itemIt==db.templates.end())continue;
   const auto& item=*itemIt;
   const auto intent=item.intent;
   const float style=request.style?([&]{for(const auto& value:item.styleWeights)if(value.first==*request.style)return value.second;return 0.25f;}()):0.5f;
   const float intentScore=request.preferredIntent&&*request.preferredIntent==intent?1.f:0.5f;
   const float cadence=intent==PhraseIntent::Loop?(item.cadence==CadenceType::LoopClosure?1.f:0.5f):
     intent==PhraseIntent::Resolve?(item.cadence==CadenceType::Authentic||item.cadence==CadenceType::PerfectAuthentic||item.cadence==CadenceType::Plagal?1.f:0.4f):0.5f;
   std::size_t suffixCount=item.full.size()-match.continuationStartIndex;
   if(item.loopable){const auto wrapEnd=match.templateMatchStart==0?1:match.templateMatchStart;suffixCount+=std::min(wrapEnd,item.full.size());}
   const float continuation=0.45f+0.1f*static_cast<float>(std::min<std::size_t>(5,suffixCount));
   const auto rhythm=estimateRhythmScale(match,query,item);
   const float rhythmScore=rhythm.samples>=2?0.5f*match.subScores.rhythmSimilarity+0.5f*rhythm.confidence:0.5f;
   float penalty{};std::size_t unexplainedInsertions{},templateDeletions{};
   for(const auto& step:match.alignmentTrace) {
    if(step.operation==AlignmentOp::QueryInsertion&&!hasReason(step.reasons,MatchReason::ResolvesToMatchedTarget)) {penalty+=weights.unexplainedInsertionPenalty;++unexplainedInsertions;}
    else if(step.operation==AlignmentOp::TemplateDeletion) {penalty+=weights.templateDeletionPenalty;++templateDeletions;}
   }
   const float scoreBeforePenalty=100.f*(weights.match*match.similarity+weights.skeleton*match.subScores.skeletonHarmony+
     weights.style*style+weights.intent*intentScore+weights.cadence*cadence+weights.continuation*continuation+
     weights.prior*item.priorWeight+weights.rhythm*rhythmScore);
   phase1bDiagnostics<<"PHASE2_SCORE id="<<item.id<<" match="<<match.similarity<<" skeleton="<<match.subScores.skeletonHarmony
    <<" fullHarmony="<<match.subScores.fullHarmony<<" keyCompatibility="<<match.subScores.keyCompatibility<<" rawCost="<<match.rawCost
    <<" style="<<style<<" intent="<<intentScore<<" cadence="<<cadence<<" continuation="<<continuation
    <<" suffixChords="<<suffixCount<<" prior="<<item.priorWeight<<" rhythmSimilarity="<<match.subScores.rhythmSimilarity
    <<" rhythmSamples="<<rhythm.samples<<" rhythmConfidence="<<rhythm.confidence<<" rhythm="<<rhythmScore
    <<" support=0 unexplainedInsertions="<<unexplainedInsertions<<" templateDeletions="<<templateDeletions
    <<" scoreBeforePenalty="<<scoreBeforePenalty<<" penalty="<<penalty<<" scoreRebuilt="<<scoreBeforePenalty-penalty
    <<" scoreReported="<<match.candidateRankingScore<<'\n';
  }
  const auto visible=session::presentationIndices(rec,{});
  std::size_t all{},colorVisible{};for(const auto& group:rec.groups)all+=group.size();for(const auto i:visible[3])if(i<rec.groups[3].size())++colorVisible;
  phase1bInputs<<fixture.id<<"\tRESULT\t0\t"<<"Factory="<<db.templates.size()<<";key="<<(analysis.selectedKey?formatKey(analysis.selectedKey->key):"UNKNOWN")
    <<";inputIntent=COLOR;source=generated-standard-MIDI;mode=4/4;tempoBPM=120;ColorCandidates="<<rec.groups[3].size()<<";ColorVisible="<<colorVisible
    <<";allContinuations="<<all<<";visibleAll="<<rec.stats.output<<";shortlist="<<rec.stats.shortlistSize<<";aligned="<<rec.stats.aligned
    <<";similarity="<<rec.stats.similarityPassed<<";tail="<<rec.stats.tailPassed<<";realization="<<rec.stats.realizationPassed<<";path="<<rec.stats.pathPassed
    <<";distinct="<<rec.stats.distinct<<";score="<<rec.stats.scorePassed<<";output="<<rec.stats.output<<"\t-1\t-1\t-1\t-1\t-1\t-1\t-1\n";
  phase1bDiagnostics<<"\n========== CASE "<<fixture.id<<" simulated MIDI; A minor; 4/4; 120 BPM; intent Color; default candidate settings; OPEN="<<fixture.openEnded<<" ==========\n"
    <<session::recommendationDiagnostics(recognized,context,request,rec);
  phase1bDiagnostics<<"CONTROL_COUNTS colorCandidates="<<rec.groups[3].size()<<" colorDefaultVisible="<<colorVisible<<" allContinuations="<<all<<"\n";
  for(const auto& group:rec.groups)for(const auto& candidate:group) {
   phase1bDiagnostics<<"CONTROL_CANDIDATE intent="<<intentName(candidate.intent)<<" group="<<(&group-&rec.groups[0])<<" id="<<candidate.primaryTemplate
     <<" score="<<candidate.rankingScore<<" rhythm="<<candidate.subscores.rhythm<<" rhythmScale="<<candidate.rhythmScale<<" rhythmConfidence="<<candidate.rhythmConfidence
     <<" match="<<candidate.subscores.match<<" intentScore="<<candidate.subscores.intent<<" openSuggestedCurrentDuration="<<candidate.suggestedCurrentChordDurationQN.value_or(-1)
     <<" matchWindow="<<candidate.matchStart<<'-'<<candidate.matchEnd<<" continuationStart="<<candidate.continuationStart<<" path=";
   for(const auto& chord:candidate.continuation)phase1bDiagnostics<<chord.label<<'('<<chord.durationQN<<") -> ";
   phase1bDiagnostics<<'\n';
  }
  for(const auto& match:rec.matches)phase1bDiagnostics<<"CONTROL_MATCH id="<<match.templateId<<" window="<<match.templateMatchStart<<'-'<<match.templateMatchEnd
    <<" continuationStart="<<match.continuationStartIndex<<" hasContinuation="<<match.hasContinuation<<" continuationLength="<<match.continuationLength
    <<" similarity="<<match.similarity<<" rhythmScore="<<match.subScores.rhythmSimilarity<<" disposition="<<match.disposition<<'\n';
  for(const auto& match:rec.matches) {
   const auto itemIt=std::find_if(db.templates.begin(),db.templates.end(),[&](const auto& item){return item.id==match.templateId;});
   if(itemIt==db.templates.end())continue;
   const auto& item=*itemIt;std::vector<const MatchEvent*> suffix;
   for(std::size_t i=match.continuationStartIndex;i<item.full.size();++i)suffix.push_back(&item.full[i]);
   if(item.loopable){const auto wrapEnd=match.templateMatchStart==0?1:match.templateMatchStart;for(std::size_t i=0;i<std::min(wrapEnd,item.full.size());++i)suffix.push_back(&item.full[i]);}
   PhraseIntent resolvedIntent=item.intent;
   if(resolvedIntent==PhraseIntent::Neutral) {
    if(item.loopable)resolvedIntent=PhraseIntent::Loop;
    else if(std::any_of(suffix.begin(),suffix.end(),[](const auto* event){return hasRole(event->roles,Role::Borrowed)||hasRole(event->roles,Role::SecondaryLeadingTone);}))resolvedIntent=PhraseIntent::Color;
    else if(item.cadence==CadenceType::Authentic||item.cadence==CadenceType::PerfectAuthentic||item.cadence==CadenceType::Plagal)resolvedIntent=PhraseIntent::Resolve;
    else resolvedIntent=PhraseIntent::Develop;
   }
   if(match.disposition=="minimum_match"&&resolvedIntent!=PhraseIntent::Color)continue;
   const ContinuationCandidate* kept{};int keptGroup=-1;std::size_t keptRank{};bool keptVisible{};
   for(std::size_t g=0;g<rec.groups.size();++g)for(std::size_t ci=0;ci<rec.groups[g].size();++ci) {
    const auto& candidate=rec.groups[g][ci];
    if(candidate.primaryTemplate==item.id||std::find(candidate.supportingTemplates.begin(),candidate.supportingTemplates.end(),item.id)!=candidate.supportingTemplates.end()) {
     kept=&candidate;keptGroup=static_cast<int>(g);keptRank=ci+1;keptVisible=std::find(visible[g].begin(),visible[g].end(),ci)!=visible[g].end();
    }
   }
   phase1bDiagnostics<<"MATCH_TRACE id="<<item.id<<" returnedWindow="<<match.templateMatchStart<<'-'<<match.templateMatchEnd<<" mode="<<(item.mode==Mode::Major?"Major":"Minor")
    <<" rawIntent="<<intentName(item.intent)<<" resolvedIntent="<<intentName(resolvedIntent)<<" loopable="<<item.loopable<<" hasContinuation="<<match.hasContinuation
    <<" similarity="<<match.similarity<<" candidateScore="<<match.candidateRankingScore<<" disposition="<<match.disposition
    <<" retained="<<(kept?1:0)<<" group="<<keptGroup<<" rank="<<keptRank<<" defaultVisible="<<keptVisible<<" suffix=";
   for(const auto* event:suffix)phase1bDiagnostics<<formatMatchEvent(*event)<<" -> ";
   phase1bDiagnostics<<'\n';
  }
  if(fixture.id.rfind("R02_T",0)==0)for(const auto& item:db.templates)if(item.intent==PhraseIntent::Color) {
   const auto found=std::find_if(rec.matches.begin(),rec.matches.end(),[&](const auto& match){return match.templateId==item.id;});
   phase1bDiagnostics<<"FACTORY_COLOR_TRACE id="<<item.id<<" mode="<<(item.mode==Mode::Major?"Major":"Minor")<<" matched="<<(found!=rec.matches.end());
   if(found!=rec.matches.end())phase1bDiagnostics<<" similarity="<<found->similarity<<" window="<<found->templateMatchStart<<'-'<<found->templateMatchEnd<<" continuationStart="<<found->continuationStartIndex
     <<" hasContinuation="<<found->hasContinuation<<" disposition="<<found->disposition<<" score="<<found->candidateRankingScore;
   phase1bDiagnostics<<'\n';
  }
  // Compare the same R02 T1 MIDI under its two plausible fixed tonal contexts.
  // Keep the ordinary score and match thresholds; this is an offline test only.
  if(fixture.id=="R02_T1") {
   AnalysisContext majorContext=context;majorContext.forcedKey=KeySignature{PitchClass::C,Mode::Major};
   const auto majorQuery=makeMatchQuery(recognized,majorContext);
   const auto majorRec=recommendContinuations(majorQuery,index,request,weights);
   const auto majorVisible=session::presentationIndices(majorRec,{});
   std::size_t majorColorVisible{};
   for(const auto i:majorVisible[3])if(i<majorRec.groups[3].size())++majorColorVisible;
   phase1bDiagnostics<<"PHASE2_KEY_CONTEXT key=C Major source=R02_T1 simulated-midi Color="<<majorRec.groups[3].size()
    <<" ColorDefaultVisible="<<majorColorVisible<<" output="<<majorRec.stats.output<<" aligned="<<majorRec.stats.aligned
    <<" similarity="<<majorRec.stats.similarityPassed<<" path="<<majorRec.stats.pathPassed<<" distinct="<<majorRec.stats.distinct
    <<" score="<<majorRec.stats.scorePassed<<'\n';
   for(const auto& group:majorRec.groups)for(const auto& candidate:group) {
    phase1bDiagnostics<<"PHASE2_KEY_CANDIDATE group="<<(&group-&majorRec.groups[0])<<" id="<<candidate.primaryTemplate
     <<" intent="<<intentName(candidate.intent)<<" score="<<candidate.rankingScore<<" visible="
     <<(std::find(majorVisible[&group-&majorRec.groups[0]].begin(),majorVisible[&group-&majorRec.groups[0]].end(),
                   static_cast<std::size_t>(&candidate-&group[0]))!=majorVisible[&group-&majorRec.groups[0]].end())<<" path=";
    for(const auto& chord:candidate.continuation)phase1bDiagnostics<<chord.label<<" -> ";
    phase1bDiagnostics<<'\n';
   }
   for(const auto& item:db.templates)if(item.intent==PhraseIntent::Color) {
    const auto found=std::find_if(majorRec.matches.begin(),majorRec.matches.end(),[&](const auto& match){return match.templateId==item.id;});
    phase1bDiagnostics<<"PHASE2_KEY_COLOR id="<<item.id<<" mode="<<(item.mode==Mode::Major?"Major":"Minor")<<" matched="<<(found!=majorRec.matches.end());
    if(found!=majorRec.matches.end())phase1bDiagnostics<<" similarity="<<found->similarity<<" rawCost="<<found->rawCost<<" window="<<found->templateMatchStart<<'-'<<found->templateMatchEnd
     <<" continuation="<<found->hasContinuation<<" disposition="<<found->disposition<<" score="<<found->candidateRankingScore;
    phase1bDiagnostics<<'\n';
    if(found!=majorRec.matches.end()&&found->similarity>=0.45f)for(const auto& step:found->alignmentTrace)
     phase1bDiagnostics<<"PHASE2_KEY_COLOR_ALIGN id="<<item.id<<" q="<<(step.queryIndex?int(*step.queryIndex):-1)
      <<" t="<<(step.templateIndex?int(*step.templateIndex):-1)<<" op="<<alignmentOpName(step.operation)
      <<" cost="<<step.cost<<" reasons="<<reasonNames(step.reasons)<<'\n';
   }
  }
  if(fixture.id=="R02_T1") {
   const auto pitchForDegree=[](const ScaleDegree& degree,Mode mode) {
    constexpr int major[]{0,2,4,5,7,9,11},minor[]{0,2,3,5,7,8,10};
    if(degree.degree<1||degree.degree>7)return -100;
    const int value=(mode==Mode::Major?major:minor)[degree.degree-1]+degree.alteration;
    return (value%12+12)%12;
   };
   std::size_t exactWindows{};
   for(const auto& item:db.templates)for(std::size_t start=0;start+2<item.full.size();++start) {
    const auto& a=item.full[start];const auto& b=item.full[start+1];const auto& c=item.full[start+2];
    if(!a.degree||!b.degree||!c.degree||a.quality!=ChordQuality::Major||b.quality!=ChordQuality::Major||c.quality!=ChordQuality::Minor)continue;
    const int rootA=pitchForDegree(*a.degree,item.mode),rootB=pitchForDegree(*b.degree,item.mode),rootC=pitchForDegree(*c.degree,item.mode);
    if(rootA<0||((rootB-rootA+12)%12)!=2||((rootC-rootA+12)%12)!=4)continue;
    ++exactWindows;
    const MatchResult* returned{};for(const auto& match:rec.matches)if(match.templateId==item.id){returned=&match;break;}
    bool hasCandidate{};int candidateGroup=-1;std::size_t candidateRank{};bool defaultVisible{};
    const ContinuationCandidate* candidate{};
    for(std::size_t g=0;g<rec.groups.size();++g)for(std::size_t ci=0;ci<rec.groups[g].size();++ci) {
     const auto& cnd=rec.groups[g][ci];
     if(cnd.primaryTemplate==item.id||std::find(cnd.supportingTemplates.begin(),cnd.supportingTemplates.end(),item.id)!=cnd.supportingTemplates.end()) {
      hasCandidate=true;candidateGroup=static_cast<int>(g);candidateRank=ci+1;candidate=&cnd;
      defaultVisible=std::find(visible[g].begin(),visible[g].end(),ci)!=visible[g].end();
     }
    }
    phase1bDiagnostics<<"R02_EXACT_WINDOW id="<<item.id<<" window="<<start<<'-'<<(start+2)<<" keyMode="<<(item.mode==Mode::Major?"Major":"Minor")
     <<" tailLength="<<(item.full.size()-start-3)<<" loopable="<<item.loopable<<" rawIntent="<<intentName(item.intent)
     <<" returned="<<(returned?1:0)<<" returnedWindow="<<(returned?std::to_string(returned->templateMatchStart)+"-"+std::to_string(returned->templateMatchEnd):"-")
     <<" hasContinuation="<<(returned?returned->hasContinuation:0)<<" continuationLength="<<(returned?returned->continuationLength:0)
     <<" disposition="<<(returned?returned->disposition:"not_returned_as_best_window")<<" candidate="<<hasCandidate
     <<" group="<<candidateGroup<<" rank="<<candidateRank<<" defaultVisible="<<defaultVisible;
    if(candidate) {
     phase1bDiagnostics<<" candidateIntent="<<intentName(candidate->intent)<<" score="<<candidate->rankingScore<<" path=";
     for(const auto& chord:candidate->continuation)phase1bDiagnostics<<chord.label<<" -> ";
    } else phase1bDiagnostics<<" abstractSuffix=";
    if(!candidate)for(std::size_t i=start+3;i<item.full.size();++i)phase1bDiagnostics<<formatMatchEvent(item.full[i])<<" -> ";
    phase1bDiagnostics<<'\n';
   }
   phase1bDiagnostics<<"R02_EXACT_WINDOW_TOTAL count="<<exactWindows<<"\n";
  }
 }
 // Phase 3A-0 offline counterfactuals. This extends the existing test entry
 // point only; production matching and recommendation code remain unchanged.
 const auto phase3a0Cases=phase3a0BaseCases();
 const auto writeSyntheticMidi=[](const Progression& progression,int octaveShift,const std::filesystem::path& path,std::string& error) {
  midi::ExportSequence sequence;sequence.tempoBPM=120.;sequence.meter={4,4};sequence.chordCount=progression.size();
  for(const auto& event:progression){const auto tones=chordPitches(event);sequence.totalQN=std::max(sequence.totalQN,event.startQN+event.durationQN.value_or(0.));
   sequence.notes.push_back({36+tones.bass+octaveShift,event.startQN,event.durationQN.value_or(4.),90,0,true});
   for(int n=0;n<12;++n)if(tones.intervals&(1<<n))sequence.notes.push_back({60+tones.root+n+octaveShift,event.startQN,event.durationQN.value_or(4.),80,0,false});}
  const auto bytes=midi::writeToMemory(sequence);return bytes&&midi::writeToFile(bytes.bytes,path,error);
 };
 const auto rootForDegree=[](const ScaleDegree& degree,KeySignature key) {
  constexpr int major[]{0,2,4,5,7,9,11},minor[]{0,2,3,5,7,8,10};if(degree.degree<1||degree.degree>7)return -100;
  const int tonic=static_cast<int>(key.tonic);return (tonic+(key.mode==Mode::Major?major:minor)[degree.degree-1]+degree.alteration+120)%12;
 };
 const auto passingDiminishedIndex=[](const Progression& progression)->std::optional<std::size_t> {
  if(progression.size()<3)return {};for(std::size_t i=1;i+1<progression.size();++i){const auto left=normalizeChord(progression[i-1]),mid=normalizeChord(progression[i]),right=normalizeChord(progression[i+1]);
   if(!left.root||!mid.root||!right.root)continue;
   if(mid.quality!=ChordQuality::Diminished&&mid.quality!=ChordQuality::Diminished7&&mid.quality!=ChordQuality::HalfDiminished7)continue;
   const int a=(static_cast<int>(*mid.root)-static_cast<int>(*left.root)+12)%12,b=(static_cast<int>(*right.root)-static_cast<int>(*mid.root)+12)%12;
   if((a==1||a==11)&&a==b)return i;
  }return {};
 };
 std::ofstream phase3a0(out/"phase3a0-offline-diagnostics.txt");if(!phase3a0)return 30;phase3a0<<std::setprecision(8);
 // A08 stores abstract degrees, not an authored note-level MIDI performance.
 // Export a clearly synthetic root-position realization for optional listening.
 Progression a08Path;double a08At{};for(const auto* name:{"Am","D","Am","G","D","Am"}){ChordEvent e;e.name=name;e.startQN=a08At;e.durationQN=4.;e.openEnded=false;a08Path.push_back(e);a08At+=4.;}
 std::string a08MidiError;const auto a08MidiPath=out/"phase3a0-V4_A08-synthetic-realization.mid";
 if(!writeSyntheticMidi(a08Path,0,a08MidiPath,a08MidiError)){std::cerr<<"Phase3A0 A08 MIDI export failed "<<a08MidiError<<'\n';return 31;}
 const auto a08MidiRaw=midi::readFromFile(a08MidiPath);if(!a08MidiRaw)return 32;
 const auto a08MidiCheck=midi::importFile(a08MidiPath,false);if(!a08MidiCheck||a08MidiCheck.extraction.chords.size()!=6)return 32;
 phase3a0<<"A08_SYNTHETIC_MIDI path="<<a08MidiPath.string()<<" source=abstract Factory degrees realized as root-position triads; not authored Factory MIDI or Cubase output; recognized=";
 for(const auto& event:a08MidiCheck.extraction.chords)phase3a0<<event.name<<'('<<event.startQN<<'+'<<event.durationQN.value_or(-1)<<") ";
 phase3a0<<" midiNotes=";for(const auto& note:a08MidiRaw.file.notes)phase3a0<<note.pitch<<'@'<<note.startQN<<'+'<<note.durationQN<<';';phase3a0<<'\n';
 const float penalty=RecommendationWeights{}.unexplainedInsertionPenalty,minimum=RecommendationWeights{}.minimumScore;
 const float small=penalty*0.25f,medium=penalty*0.5f;
 // The threshold-near amount is the exact gap from the strongest R01 score to 60,
 // derived from the existing Phase 2 score record rather than a tuned constant.
 constexpr float strongestR01Score=57.40387f;const float nearThreshold=minimum-strongestR01Score;
 phase3a0<<"COMPENSATION_LEVELS no=0 small="<<small<<" nearThreshold="<<nearThreshold<<" medium="<<medium<<" fullRefund="<<penalty<<" gate="<<minimum<<'\n';
 for(const auto& fixture:phase3a0Cases){Progression source;double cursor{};
  for(std::size_t i=0;i<fixture.names.size();++i){ChordEvent e;e.name=fixture.names[i];e.startQN=cursor;e.durationQN=fixture.durations[i];e.openEnded=false;source.push_back(e);cursor+=fixture.durations[i];}
  const auto midiPath=out/("phase3a0-"+fixture.id+".mid");std::string midiError;if(!writeSyntheticMidi(source,fixture.octaveShift,midiPath,midiError)){std::cerr<<"Phase3A0 MIDI export failed "<<fixture.id<<" "<<midiError<<'\n';return 33;}
  const auto imported=midi::importFile(midiPath,false);if(!imported){std::cerr<<"Phase3A0 MIDI import failed "<<fixture.id<<" "<<imported.extraction.error<<'\n';return 34;}
  const auto& recognized=imported.extraction.chords;AnalysisContext context;context.forcedKey=fixture.key;context.timeSigNumerator=4;context.timeSigDenominator=4;
  const auto query=makeMatchQuery(recognized,context);RecommendationRequest request;request.preferredIntent=PhraseIntent::Color;RecommendationWeights weights;
  const auto rec=recommendContinuations(query,index,request,weights);const auto passing=passingDiminishedIndex(recognized);
  phase3a0<<"\nCASE id="<<fixture.id<<" forcedKey="<<formatKey(fixture.key)<<" octaveShift="<<fixture.octaveShift<<" source=synthetic-MIDI recognized=";
  for(const auto& event:recognized)phase3a0<<event.name<<'('<<event.startQN<<'+'<<event.durationQN.value_or(-1)<<") ";
  phase3a0<<" stages=aligned:"<<rec.stats.aligned<<",similarity:"<<rec.stats.similarityPassed<<",tail:"<<rec.stats.tailPassed<<",path:"<<rec.stats.pathPassed<<",distinct:"<<rec.stats.distinct<<",score:"<<rec.stats.scorePassed<<",output:"<<rec.stats.output
   <<" passingDimQueryIndex="<<(passing?static_cast<int>(*passing):-1)<<" matches="<<rec.matches.size()<<'\n';
  for(const auto& match:rec.matches){const auto itemIt=std::find_if(db.templates.begin(),db.templates.end(),[&](const auto& item){return item.id==match.templateId;});if(itemIt==db.templates.end())continue;
   std::size_t insertions{},unexplained{};std::optional<std::size_t> insertionIndex;bool insertionAtDim=false;
   for(const auto& step:match.alignmentTrace)if(step.operation==AlignmentOp::QueryInsertion&&step.queryIndex){++insertions;insertionIndex=step.queryIndex;
    if(passing&&*passing==*step.queryIndex)insertionAtDim=true;
    if(!hasReason(step.reasons,MatchReason::ResolvesToMatchedTarget))++unexplained;}
   bool exactOtherChords=insertions==1&&insertionIndex&&passing&&*insertionIndex==*passing;
   if(exactOtherChords)for(std::size_t qi=0;qi<recognized.size();++qi)if(qi!=*insertionIndex){const AlignmentStep* aligned{};
    for(const auto& step:match.alignmentTrace)if(step.queryIndex&&*step.queryIndex==qi){aligned=&step;break;}
    if(!aligned||(aligned->operation!=AlignmentOp::Match&&aligned->operation!=AlignmentOp::Substitute)||!aligned->templateIndex||*aligned->templateIndex>=itemIt->full.size()){exactOtherChords=false;break;}
    const auto chord=normalizeChord(recognized[qi]);const auto& templ=itemIt->full[*aligned->templateIndex];
    if(!chord.root||!templ.degree||chord.quality!=templ.quality||static_cast<int>(*chord.root)!=rootForDegree(*templ.degree,match.key)){exactOtherChords=false;break;}
   }
   const bool eligible=exactOtherChords&&match.hasContinuation&&match.similarity>=weights.minimumMatch&&match.candidateRankingScore>=0.f;
   const bool relevant=match.templateId=="V4_A08"||eligible||(insertionAtDim&&match.hasContinuation&&match.similarity>=weights.minimumMatch&&match.candidateRankingScore>=0.f);if(!relevant)continue;
   std::size_t suffixCount=itemIt->full.size()-match.continuationStartIndex;
   if(itemIt->loopable){const auto wrapEnd=match.templateMatchStart==0?1:match.templateMatchStart;suffixCount+=std::min(wrapEnd,itemIt->full.size());}
   std::vector<std::string> suffix;for(std::size_t i=match.continuationStartIndex;i<itemIt->full.size();++i)suffix.push_back(formatMatchEvent(itemIt->full[i]));
   if(itemIt->loopable){const auto wrapEnd=match.templateMatchStart==0?1:match.templateMatchStart;for(std::size_t i=0;i<std::min(wrapEnd,itemIt->full.size());++i)suffix.push_back(formatMatchEvent(itemIt->full[i]));}
   const float current=match.candidateRankingScore;const float targeted=eligible?current+penalty*static_cast<float>(unexplained):current;
   phase3a0<<"CANDIDATE id="<<match.templateId<<" intent="<<intentName(itemIt->intent)<<" window="<<match.templateMatchStart<<'-'<<match.templateMatchEnd
    <<" similarity="<<match.similarity<<" continuation="<<match.hasContinuation<<" disposition="<<match.disposition<<" unexplainedInsertions="<<unexplained
    <<" strictPassingDimEligible="<<eligible<<" scoreCurrent="<<current<<" scorePlusSmall="<<(current>=0?current+small*unexplained:-1)
    <<" scorePlusNear="<<(current>=0?current+nearThreshold*unexplained:-1)<<" scorePlusMedium="<<(current>=0?current+medium*unexplained:-1)
    <<" scorePlusFullGlobal="<<(current>=0?current+penalty*unexplained:-1)<<" scoreStrictTargeted="<<targeted<<" suffix=";
   for(const auto& chord:suffix)phase3a0<<chord<<" -> ";phase3a0<<'\n';
  }
 }
 // Phase 3A prerequisite: verify the original engine before scoring changes.
 {
  Progression p;for(const auto* name:{"Am","G#dim","G","D/F#"}){ChordEvent e;e.name=name;e.startQN=p.size()*4.;e.durationQN=4.;p.push_back(e);}
  AnalysisContext ctx;ctx.forcedKey=KeySignature{PitchClass::A,Mode::Minor};ctx.timeSigNumerator=4;ctx.timeSigDenominator=4;
  const auto q=makeMatchQuery(p,ctx);
  // A real intent preference exposes the existing candidate without lowering gates.
  RecommendationRequest request;request.preferredIntent=PhraseIntent::Loop;
  const auto r=recommendContinuations(q,index,request);
  const auto m=std::find_if(r.matches.begin(),r.matches.end(),[](const auto& x){return x.templateId=="V4_A08";});
  if(m==r.matches.end()||m->templateMatchStart!=2||m->templateMatchEnd!=4||m->continuationStartIndex!=5||m->alignmentTrace.size()!=4)return 40;
  const auto& t=m->alignmentTrace;
  if(t[0].queryIndex!=0||t[0].templateIndex!=2||t[0].operation!=AlignmentOp::Match||
     t[1].queryIndex!=1||t[1].templateIndex||t[1].operation!=AlignmentOp::QueryInsertion||
     t[2].queryIndex!=2||t[2].templateIndex!=3||t[2].operation!=AlignmentOp::Match||
     t[3].queryIndex!=3||t[3].templateIndex!=4||t[3].operation!=AlignmentOp::Substitute)return 41;
  const ContinuationCandidate* c{};for(const auto& group:r.groups)for(const auto& x:group)if(x.id=="V4_A08")c=&x;
  if(!c||c->continuation.size()!=3||c->continuation[0].label!="Am"||c->continuation[1].label!="Am"||c->continuation[2].label!="D")return 42;
  const auto output=continuationProgression(p,*c);std::string error;
  if(!writeSyntheticMidi(output,0,out/"phase3a-A08-verified-path.mid",error))return 43;
  const auto read=midi::importFile(out/"phase3a-A08-verified-path.mid",false);
  // Extraction may merge adjacent identical tonics; raw MIDI preserves both events.
  const auto raw=midi::readFromFile(out/"phase3a-A08-verified-path.mid");
  if(!read||!raw)return 44;
  midi::CandidateExportContext exportContext;
  if(!exportContext.imported.replace(p,TimelineCoordinateMode::RelativeToSelection))return 45;
  const auto payload=midi::candidatePayload(exportContext,*c);
  if(!payload)return 46;
  if(!midi::writeToFile(payload.payload.smfBytes,out/"phase3a-A08-engine-output.mid",error))return 47;
  const auto engineMidi=midi::readFromFile(out/"phase3a-A08-engine-output.mid");if(!engineMidi)return 48;
  for(int i=0;i<3;++i){unsigned mask{};std::size_t notes{};for(const auto& n:engineMidi.file.notes)if(std::abs(n.startQN-(16.+i*4.))<1e-6){
    mask|=1u<<(n.pitch%12);++notes;if(std::abs(n.durationQN-4.)>1e-6)return 79;}
   if(notes!=4||mask!=(i<2?((1u<<9)|(1u<<0)|(1u<<4)):((1u<<2)|(1u<<6)|(1u<<9))))return 80;
  }
  std::ofstream trace(out/"phase3a-path-verification.txt");
  trace<<"Factory index path=0:Am 1:D 2:Am 3:G 4:D 5:Am\nquery alignment=0->2 Match;1->gap QueryInsertion;2->3 Match;3->4 Substitute(BorrowedVariant)\nsuffix indices=5,0,1; labels=Am,Am,D; loopable=true; no reorder/deletion; duplicate tonic is source loop boundary\n";
  for(const auto& e:output)trace<<e.name<<'@'<<e.startQN<<'+'<<e.durationQN.value_or(-1)<<'\n';
  for(const auto& n:raw.file.notes)trace<<"MIDI "<<n.pitch<<'@'<<n.startQN<<'+'<<n.durationQN<<'\n';
  for(const auto& n:engineMidi.file.notes)trace<<"ENGINE_MIDI "<<n.pitch<<'@'<<n.startQN<<'+'<<n.durationQN<<'\n';
 }
 // Phase 3A enabled/off comparisons and adversarial trace tests.
 std::ofstream phase3a(out/"phase3a-implementation-diagnostics.txt");phase3a<<std::setprecision(9);
 auto samples=phase3a0Cases;
 samples.push_back({"NEG_ONE_SIDE",{"Am","G#dim","F","D"},{4,4,4,4},{PitchClass::A,Mode::Minor},0});
 samples.push_back({"NEG_DIRECTION_REVERSES",{"Am","G#dim","Am","D"},{4,4,4,4},{PitchClass::A,Mode::Minor},0});
 samples.push_back({"NEG_TWO_INSERTIONS",{"Am","G#dim","G","F#dim","F","D"},{4,4,4,4,4,4},{PitchClass::A,Mode::Minor},0});
 samples.push_back({"NEG_HALF_DIM",{"Am","G#m7b5","G","D"},{4,4,4,4},{PitchClass::A,Mode::Minor},0});
 samples.push_back({"NEG_NON_DIM_CHROMATIC",{"Am","G#","G","D"},{4,4,4,4},{PitchClass::A,Mode::Minor},0});
 samples.push_back({"NEG_SAME_ROOT_QUALITY",{"A","G#dim","G","D"},{4,4,4,4},{PitchClass::A,Mode::Minor},0});
 samples.push_back({"POS_DESC_DIM7",{"Am","G#dim7","G","D"},{4,4,4,4},{PitchClass::A,Mode::Minor},0});
 samples.push_back({"POS_SHORT_DIM",{"Am","G#dim","G","D"},{4,0.25,4,4},{PitchClass::A,Mode::Minor},0});
 samples.push_back({"R01_INVERSION_ONLY",{"Am/C","G#dim/B","G/B","D/F#"},{4,4,4,4},{PitchClass::A,Mode::Minor},0});
 samples.push_back({"R01_OCTAVE_DOWN",{"Am","G#dim","G","D/F#"},{4,4,4,4},{PitchClass::A,Mode::Minor},-12});
 samples.push_back({"R01_OCTAVE_UP",{"Am","G#dim","G","D/F#"},{4,4,4,4},{PitchClass::A,Mode::Minor},12});
 samples.push_back({"NEG_NO_SHARED_TONES",{"Am","G#dim","Gm","D"},{4,4,4,4},{PitchClass::A,Mode::Minor},0});
 constexpr const char* pitchNames[]{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
 for(int shift=0;shift<12;++shift)samples.push_back({"TRANSPOSE_"+std::to_string(shift),
  {std::string(pitchNames[(9+shift)%12])+"m",std::string(pitchNames[(8+shift)%12])+"dim",pitchNames[(7+shift)%12],pitchNames[(2+shift)%12]},
  {4,4,4,4},{static_cast<PitchClass>((9+shift)%12),Mode::Minor},0});
 std::size_t verifiedSamples{},refundedSamples{};
 for(const auto& fixture:samples){
  Progression p;double at{};for(std::size_t i=0;i<fixture.names.size();++i){ChordEvent e;e.name=fixture.names[i];e.startQN=at;e.durationQN=fixture.durations[i];e.openEnded=false;p.push_back(e);at+=*e.durationQN;}
  // Use the actual MIDI-recognized input for the original nine fixtures.
  if(verifiedSamples<phase3a0Cases.size()){const auto read=midi::importFile(out/("phase3a0-"+fixture.id+".mid"),false);if(!read)return 53;p=read.extraction.chords;}
  else if(fixture.id.rfind("R01_",0)==0){std::string error;const auto path=out/("phase3a-"+fixture.id+".mid");
   if(!writeSyntheticMidi(p,fixture.octaveShift,path,error))return 76;const auto read=midi::importFile(path,false);if(!read)return 77;p=read.extraction.chords;}
  AnalysisContext context;context.forcedKey=fixture.key;context.timeSigNumerator=4;context.timeSigDenominator=4;const auto q=makeMatchQuery(p,context);
  RecommendationRequest request;request.preferredIntent=PhraseIntent::Color;
  RecommendationWeights off,on;on.diminishedPassingInsertionException=true;
  const auto before=recommendContinuations(q,index,request,off),after=recommendContinuations(q,index,request,on);
  if(!before.insertionDiagnostics.empty()||off.diminishedPassingInsertionException||off.minimumScore!=60||on.minimumScore!=60)return 54;
  phase3a<<"CASE "<<fixture.id<<" outputOff="<<before.stats.output<<" outputOn="<<after.stats.output<<" paths="<<before.stats.pathPassed<<'/'<<after.stats.pathPassed<<'\n';
  std::size_t refunds{};
  for(const auto& d:after.insertionDiagnostics){
   if(d.refund<0||d.refund>d.originalPenalty||d.refund!=(d.eligible?d.originalPenalty:0.f)||std::abs(d.scoreAfter-d.scoreBefore-d.refund)>0.0001f)return 55;
   if(d.refund>0)++refunds;
   phase3a<<d.templateId<<" single="<<d.singleInsertion<<" trace="<<d.reliableAlignment<<" dim="<<d.diminished<<" chromatic="<<d.chromaticMotion<<" shared="<<d.sharedTones<<" core="<<d.compatibleCore<<" continuation="<<d.validContinuation<<" explained="<<d.alreadyExplained<<" eligible="<<d.eligible<<" totalPenalty="<<d.totalInsertionPenalty<<" penalty="<<d.originalPenalty<<" refund="<<d.refund<<" score="<<d.scoreBefore<<"->"<<d.scoreAfter<<" gate="<<(d.scoreAfter>=60)<<'\n';
  }
  const bool original=fixture.id.rfind("R01_",0)==0||fixture.id.rfind("TRANSPOSE_",0)==0;
  if(original && (refunds!=1||after.stats.output!=1||after.groups[2].empty()||after.groups[2][0].id!="V4_A08"))return 56;
  if(fixture.id.rfind("NEG_",0)==0 && refunds!=0)return 57;
  if(fixture.id=="PASSING_FSHARP_DIM7" && refunds!=0)return 58;
  if(fixture.id=="CONTROL_NO_DIM" && refunds!=0)return 59;
  if(fixture.id.rfind("POS_",0)==0 && refunds==0)return 73;
  if(refunds)++refundedSamples;
  if(fixture.id=="R01_ORIGINAL") {
   for(const auto& m:before.matches)if(m.candidateRankingScore>=0){const auto n=std::find_if(after.matches.begin(),after.matches.end(),[&](const auto& x){return x.templateId==m.templateId;});if(n==after.matches.end())return 60;
    phase3a<<"R01_SCORE "<<m.templateId<<' '<<m.candidateRankingScore<<" -> "<<n->candidateRankingScore<<" disposition="<<n->disposition<<'\n';
    if(m.templateId!="V4_A08" && m.candidateRankingScore!=n->candidateRankingScore)return 61;}
   const auto found=std::find_if(before.matches.begin(),before.matches.end(),[](const auto& m){return m.templateId=="V4_A08";});if(found==before.matches.end())return 62;
   if(before.stats.pathPassed!=5||before.stats.output!=0||std::abs(found->candidateRankingScore-57.40387f)>0.0001f)return 74;
   const auto itemIt=std::find_if(db.templates.begin(),db.templates.end(),[](const auto& t){return t.id=="V4_A08";});auto item=*itemIt;
   if(!evaluateDiminishedPassingInsertion(q,*found,item).eligible)return 63;
   // Renaming proves that no Factory ID participates in eligibility.
   item.id="unrelated-id";if(!evaluateDiminishedPassingInsertion(q,*found,item).eligible)return 64;
   auto broken=*found;broken.alignmentTrace[0].templateIndex=1;
   if(evaluateDiminishedPassingInsertion(q,broken,item).eligible)return 65;
   broken=*found;broken.alignmentTrace[2].queryIndex=0;
   if(evaluateDiminishedPassingInsertion(q,broken,item).eligible)return 66;
   broken=*found;broken.alignmentTrace[2].operation=AlignmentOp::TemplateDeletion;
   if(evaluateDiminishedPassingInsertion(q,broken,item).eligible)return 67;
   broken=*found;broken.alignmentTrace[2].operation=AlignmentOp::QueryInsertion;
   if(evaluateDiminishedPassingInsertion(q,broken,item).eligible)return 68;
   broken=*found;broken.alignmentTrace[1].reasons|=flag(MatchReason::ResolvesToMatchedTarget);
   if(evaluateDiminishedPassingInsertion(q,broken,item).eligible)return 69;
   broken=*found;broken.templateMatchEnd=item.full.size()-1;broken.continuationStartIndex=item.full.size();item.loopable=false;
   if(evaluateDiminishedPassingInsertion(q,broken,item).validContinuation)return 70;
   auto exhausted=*itemIt;exhausted.id="exhausted";exhausted.loopable=false;
   exhausted.full=std::vector<MatchEvent>(itemIt->full.begin()+2,itemIt->full.begin()+5);exhausted.skeletonIndices={0,1,2};
   const auto noTail=recommendContinuations(q,CandidateIndex({exhausted}),request,on);
   if(noTail.stats.output!=0||!noTail.insertionDiagnostics.empty())return 78;
   for(float eventPenalty:{0.f,1.f,8.f,12.f}){auto varying=on;varying.unexplainedInsertionPenalty=eventPenalty;
    const auto r=recommendContinuations(q,index,request,varying);
    const auto d=std::find_if(r.insertionDiagnostics.begin(),r.insertionDiagnostics.end(),[](const auto& x){return x.templateId=="V4_A08";});
    if(d==r.insertionDiagnostics.end()||d->originalPenalty!=eventPenalty||d->refund!=eventPenalty||std::abs(d->scoreAfter-65.40387f)>0.0001f)return 75;
   }
   // MIDI of an unchanged candidate is byte-identical across scoring settings.
   RecommendationRequest loopRequest;loopRequest.preferredIntent=PhraseIntent::Loop;
   const auto left=recommendContinuations(q,index,loopRequest,off),right=recommendContinuations(q,index,loopRequest,on);
   const ContinuationCandidate *a{},*b{};for(const auto& g:left.groups)for(const auto& c:g)if(c.id=="V4_A08")a=&c;
   for(const auto& g:right.groups)for(const auto& c:g)if(c.id=="V4_A08")b=&c;
   midi::CandidateExportContext ec;ec.imported.replace(p,TimelineCoordinateMode::RelativeToSelection);
   if(!a||!b)return 71;const auto ma=midi::candidatePayload(ec,*a),mb=midi::candidatePayload(ec,*b);
   if(!ma||!mb||ma.payload.smfBytes!=mb.payload.smfBytes)return 72;
   // Alternate OFF/ON in one process, fixed query/index, warm cache, 80 samples.
   for(int i=0;i<10;++i){recommendContinuations(q,index,request,off);recommendContinuations(q,index,request,on);}
   std::vector<double> timings[2];std::size_t allocated[2]{},calls[2]{};
   for(int run=0;run<80;++run)for(int order=0;order<2;++order){const int mode=(run+order)%2;
    phase3aMemory::bytes=phase3aMemory::calls=0;phase3aMemory::enabled=true;
    const auto start=std::chrono::steady_clock::now();const auto r=recommendContinuations(q,index,request,mode?on:off);
    const auto end=std::chrono::steady_clock::now();phase3aMemory::enabled=false;
    timings[mode].push_back(std::chrono::duration<double,std::micro>(end-start).count());allocated[mode]+=phase3aMemory::bytes;calls[mode]+=phase3aMemory::calls;
   }
   for(int mode=0;mode<2;++mode){auto& t=timings[mode];std::sort(t.begin(),t.end());phase3a<<"PERFORMANCE mode="<<mode<<" n="<<t.size()<<" median_us="<<t[t.size()/2]<<" p95_us="<<t[t.size()*95/100]<<" allocation_bytes_per_call="<<allocated[mode]/t.size()<<" allocations_per_call="<<calls[mode]/t.size()<<'\n';}
   const auto scoringStart=std::chrono::steady_clock::now();std::size_t accepted{};
   for(int n=0;n<10000;++n)accepted+=evaluateDiminishedPassingInsertion(q,*found,*itemIt).eligible;
   phase3a<<"SCORING predicate_us="<<std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-scoringStart).count()/10000<<" accepted="<<accepted<<'\n';
   const auto stageStart=std::chrono::steady_clock::now();std::size_t records{};
   for(int n=0;n<2000;++n){std::vector<DiminishedInsertionDiagnostic> diagnostics;
    for(const auto& m:before.matches)if(m.candidateRankingScore>=0){
     const auto t=std::find_if(db.templates.begin(),db.templates.end(),[&](const auto& x){return x.id==m.templateId;});
     auto d=evaluateDiminishedPassingInsertion(q,m,*t);d.refund=d.eligible?8.f:0.f;diagnostics.push_back(std::move(d));}
    records+=diagnostics.size();}
   phase3a<<"SCORING five_candidate_check_and_diagnostics_us="<<std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-stageStart).count()/2000<<" records="<<records<<" includes test lookup overhead\n";
  }
  ++verifiedSamples;
 }
 phase3a<<"SUMMARY samples="<<verifiedSamples<<" refundedSamples="<<refundedSamples<<" existingMatrix=1440 identical; malformedTraceChecks=7; MIDI bytes identical\n";
 if(argc>2 && std::string_view(argv[2])=="system-audit") {
  const auto auditDir=out/"system-audit";std::filesystem::create_directories(auditDir);
  const auto csv=[](std::string_view s){std::string o="\"";for(char c:s){if(c=='\"')o+='\"';o+=c;}return o+'\"';};
  const auto pitchName=[](int pc)->const char*{constexpr const char* n[]{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};return n[(pc%12+12)%12];};
   const auto styleTag=[](Style s)->const char*{switch(s){case Style::Pop:return "Pop";case Style::Rock:return "Rock";case Style::Rnb:return "RNB";case Style::Jazz:return "Jazz";case Style::CityPop:return "CityPop";case Style::Functional:return "Functional";}return "unknown";};
  const auto effective=[](const ProgressionTemplate& item,const std::vector<MatchEvent>& suffix){
   if(item.intent!=PhraseIntent::Neutral)return item.intent;if(item.loopable)return PhraseIntent::Loop;
   for(const auto& e:suffix)if(hasRole(e.roles,Role::Borrowed)||hasRole(e.roles,Role::SecondaryLeadingTone))return PhraseIntent::Color;
   if(item.cadence==CadenceType::Authentic||item.cadence==CadenceType::PerfectAuthentic||item.cadence==CadenceType::Plagal)return PhraseIntent::Resolve;
   return PhraseIntent::Develop;
  };
   struct PerFactory {std::size_t windows{},probeRuns{},modeCompatibleRuns{},queries{},aligned{},paths{},scoreGate{},primaryVisible{},supportVisible{};
    float bestSimilarity{},bestScore{-1.f};std::string bestInput,bestDisposition,bestRankGroup;std::size_t bestRank{};std::map<std::string,std::size_t> blockers;};
  struct Exposure {std::size_t visible{},rank1{},rank2{},rank3{},primary{},support{};};
   struct PathPairs {std::size_t runs{},pairs{},engineSimilar{},sameRelativePath{};};
   struct ScoreStratum {std::size_t rows{},scoreable{},gate{};double similaritySum{},scoreSum{};};
  std::unordered_map<TemplateID,std::size_t> rowOf;rowOf.reserve(db.templates.size());
  std::vector<PerFactory> per(db.templates.size());
  for(std::size_t i=0;i<db.templates.size();++i)rowOf.emplace(db.templates[i].id,i);
   std::ofstream rows(auditDir/"factory-reachability.csv"),runs(auditDir/"run-summary.csv"),scores(auditDir/"score-component-samples.csv"),trace(auditDir/"regression-match-trace.csv");
  std::map<std::string,std::unordered_map<TemplateID,Exposure>> exposureByCorpus;
  std::map<std::string,std::map<std::string,std::size_t>> intentVisible,scoreBands;
   std::map<std::string,PathPairs> pathPairs;
   std::map<std::string,ScoreStratum> scoreStrata;
  std::vector<std::string> authoredPaths;
  std::set<std::string> listeningWritten;
  std::ofstream listening(auditDir/"listening-midi-manifest.csv");
   listening<<"case_id,FactoryID,intent,key,file,source\n";
  phase3aMemory::bytes=phase3aMemory::calls=0;phase3aMemory::enabled=true;
   const auto memoryStartBytes=phase3aMemory::bytes;
   std::size_t peakWorkingSetBytes{};
   const auto sampleProcessMemory=[&](){
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS_EX counters{};counters.cb=sizeof(counters);
    if(GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PPROCESS_MEMORY_COUNTERS>(&counters),sizeof(counters)))peakWorkingSetBytes=std::max<std::size_t>(peakWorkingSetBytes,counters.PeakWorkingSetSize);
#endif
   };
   sampleProcessMemory();
   if(!rows||!runs||!scores||!trace)return 82;
   rows<<"FactoryID,mode,intent,styles,event_count,skeleton_count,natural_continuation_positions,scheduled_queries,mode_compatible_queries,matched_template_rows,aligned_queries,path_queries,score_gate_rows,primary_visible,support_visible,best_score,best_similarity,best_input,best_disposition,best_rank_group,best_rank,dominant_obstacle,obstacle_counts\n";
   runs<<"corpus,case_id,variant,key,mode,requested_style,preferred_intent,input_chords,input_sequence,query_interpretations,shortlist_size,aligned,similarity_pass,tail_pass,realization_pass,path_pass,distinct,score_pass_prediversity,visible,prefilter_ms,alignment_ms,elapsed_ms\n";
   scores<<"corpus,case_id,record_type,FactoryID,intent,style,match_similarity,skeleton,style_score,intent_score,cadence,continuation,prior,rhythm,support,insertions,deletions,pre_support_estimate,engine_recorded_score,disposition\n";
   trace<<"corpus,case_id,variant,key,requested_style,preferred_intent,recognized_input,FactoryID,template_intent,similarity,match_start,match_end,continuation_start,has_continuation,loopable,output_intent,template_path,continuation_path,alignment_trace,score,disposition\n";
  const KeySignature cMajor{PitchClass::C,Mode::Major},cMinor{PitchClass::C,Mode::Minor};
  auto makeSymbols=[&](const ProgressionTemplate& item,std::size_t first,std::size_t last,KeySignature key,
                       double scale,int rhythmShape,bool open,bool inversion,bool omitFifth) {
   Progression p;double at{};
   for(std::size_t j=first;j<=last&&j<item.full.size();++j){const auto& event=item.full[j];
    const auto baseDuration=event.durationQN.value_or(4.0);double duration=baseDuration*scale;
    if(rhythmShape==1)duration=(j==first?3.0:j==first+1?5.0:4.0);
    auto concrete=realizeContinuation(event,key,duration);ChordEvent chord;chord.name=concrete.label;chord.startQN=at;chord.durationQN=duration;chord.openEnded=false;chord.quality=event.quality;
    if(inversion){const auto tones=chordPitches(chord);int chosen=-1;for(int x:{3,4,7,10,11})if(tones.intervals&(1<<x)){chosen=x;break;}
     if(chosen>=0)chord.name+="/"+std::string(pitchName(tones.root+chosen));}
    if(omitFifth){auto tones=chordPitches(chord);if(tones.intervals&(1<<7)){std::ostringstream mask;mask<<"0x"<<std::hex<<(tones.intervals&~(1u<<7));chord.extensions.mask=mask.str();}}
    p.push_back(std::move(chord));at+=duration;
   }
   if(open&&!p.empty()){p.back().durationQN.reset();p.back().openEnded=true;}
   return p;
  };
   std::string lastTransformFailure;
   auto midiTransform=[&](const Progression& p,int semitones,int octave,int bassOctave,bool inversion,bool omitFifth,
                         double scale,int rhythmShape,bool open,bool splitChannels=false)->std::optional<Progression>{
    lastTransformFailure.clear();
   midi::ExportSequence sequence;sequence.tempoBPM=120.;sequence.meter={4,4};sequence.chordCount=p.size();
   double cursor{};
   for(std::size_t i=0;i<p.size();++i){const auto& source=p[i];const auto tones=chordPitches(source);double duration=source.durationQN.value_or(4.0)*scale;
    if(rhythmShape==1)duration=i==0?3.0:i==1?5.0:4.0;
    const double start=rhythmShape==1?cursor:source.startQN*scale;cursor=start+duration;
    sequence.totalQN=std::max(sequence.totalQN,start+duration);
    int bass=tones.bass;if(inversion){for(int x:{3,4,7,10,11})if(tones.intervals&(1<<x)){bass=(tones.root+x)%12;break;}}
     sequence.notes.push_back({36+bass+semitones+12*octave+12*bassOctave, start,duration,90,static_cast<std::uint8_t>(splitChannels?1:0),true});
    for(int n=0;n<12;++n)if((tones.intervals&(1<<n))&&!(omitFifth&&n==7))
     sequence.notes.push_back({60+tones.root+n+semitones+12*octave,start,duration,80,static_cast<std::uint8_t>(splitChannels?2:0),false});
   }
    const auto bytes=midi::writeToMemory(sequence);if(!bytes){lastTransformFailure="SMF serialization failed";return {};}
    const auto raw=midi::readFromMemory(bytes.bytes);if(!raw){lastTransformFailure="SMF parse failed";return {};}
   midi::MidiHarmonyExtractionConfig config;config.openEnded=open;
    const auto extracted=midi::extractHarmony(raw.file,config);if(!extracted){lastTransformFailure="harmony recognition failed: "+extracted.error;return {};}
   return extracted.chords;
  };
   std::size_t totalRuns{},failedRecognitions{};std::map<std::string,std::size_t> transformFailureReasons;double totalEngineMs{};const auto auditStarted=std::chrono::steady_clock::now();
  const auto logRun=[&](std::string_view corpus,std::string_view caseId,std::string_view variant,const Progression& p,
                        KeySignature key,const RecommendationRequest& request)->RecommendationSet {
   AnalysisContext ctx;ctx.forcedKey=key;ctx.timeSigNumerator=4;ctx.timeSigDenominator=4;
   const auto q=makeMatchQuery(p,ctx);const auto start=std::chrono::steady_clock::now();
    const auto rec=recommendContinuations(q,index,request,RecommendationWeights{});const auto finish=std::chrono::steady_clock::now();sampleProcessMemory();
   const auto ms=std::chrono::duration<double,std::milli>(finish-start).count();totalEngineMs+=ms;++totalRuns;
    std::ostringstream inputSequence;for(std::size_t i=0;i<p.size();++i){if(i)inputSequence<<'|';const auto tones=chordPitches(p[i]);inputSequence<<p[i].name<<'@'<<p[i].startQN<<'+'<<p[i].durationQN.value_or(-1)<<"{root="<<tones.root<<",bass="<<tones.bass<<",mask="<<tones.intervals<<",open="<<p[i].openEnded<<'}';}
    runs<<csv(corpus)<<','<<csv(caseId)<<','<<csv(variant)<<','<<csv(formatKey(key))<<','<<(key.mode==Mode::Major?"major":"minor")<<','<<(request.style?styleTag(*request.style):"none")<<','<<(request.preferredIntent?intentName(*request.preferredIntent):"none")<<','<<p.size()<<','<<csv(inputSequence.str())<<','<<q.interpretations.size()<<','<<rec.stats.shortlistSize<<','
     <<rec.stats.aligned<<','<<rec.stats.similarityPassed<<','<<rec.stats.tailPassed<<','<<rec.stats.realizationPassed<<','<<rec.stats.pathPassed<<','<<rec.stats.distinct<<','<<rec.stats.scorePassed<<','<<rec.stats.output<<','<<rec.stats.prefilterMs<<','<<rec.stats.alignmentMs<<','<<std::fixed<<std::setprecision(4)<<ms<<'\n';
    if(RecommendationWeights{}.diminishedPassingInsertionException)throw std::runtime_error("Phase 3A audit baseline unexpectedly enabled");
    for(std::size_t i=0;i<db.templates.size();++i){++per[i].probeRuns;if(std::any_of(q.interpretations.begin(),q.interpretations.end(),[&](const auto& qi){return qi.key.key.mode==db.templates[i].mode;}))++per[i].modeCompatibleRuns;}
    for(const auto& match:rec.matches){const auto band=match.candidateRankingScore<0.f?"not_scored":match.candidateRankingScore<40.f?"<40":match.candidateRankingScore<50.f?"40-49.99":match.candidateRankingScore<60.f?"50-59.99":match.candidateRankingScore<70.f?"60-69.99":"70+";++scoreBands[std::string(corpus)][band];const auto ri=rowOf.find(match.templateId);if(ri==rowOf.end())continue;const auto& scoredItem=db.templates[ri->second];const auto lengthBucket=p.size()<=2?"1-2":p.size()<=4?"3-4":"5+";std::ostringstream stratumKey;stratumKey<<corpus<<'|'<<(key.mode==Mode::Major?"major":"minor")<<'|'<<lengthBucket<<'|'<<static_cast<int>(scoredItem.complexityLevel)<<'|'<<intentName(scoredItem.intent)<<'|'<<(request.style?styleTag(*request.style):"none")<<'|'<<(request.preferredIntent?intentName(*request.preferredIntent):"none");auto& stratum=scoreStrata[stratumKey.str()];++stratum.rows;stratum.similaritySum+=match.similarity;if(match.candidateRankingScore>=0.f){++stratum.scoreable;stratum.scoreSum+=match.candidateRankingScore;if(match.candidateRankingScore>=60.f)++stratum.gate;}auto& stat=per[ri->second];++stat.queries;
    stat.bestSimilarity=std::max(stat.bestSimilarity,match.similarity);
    const bool path=match.disposition=="candidate_generated"||match.disposition=="minimum_score"||match.disposition=="equivalent_path_deduplication"||match.disposition=="diversity_score"||match.disposition=="retained_limit"||match.disposition=="TARGET_RETAINED";
    if(match.disposition!="minimum_match")++stat.aligned;if(path)++stat.paths;
    if(match.candidateRankingScore>=60.f)++stat.scoreGate;
    ++stat.blockers[match.disposition.empty()?"unclassified":match.disposition];
    if(match.candidateRankingScore>stat.bestScore){stat.bestScore=match.candidateRankingScore;stat.bestDisposition=match.disposition;stat.bestInput=std::string(corpus)+":"+std::string(caseId)+":"+std::string(variant);}
     if(corpus=="R01_R02_B08_B11"){
      const auto& item=db.templates[ri->second];std::vector<MatchEvent> suffix;for(std::size_t n=match.continuationStartIndex;n<item.full.size();++n)suffix.push_back(item.full[n]);
      if(item.loopable){const auto wrapEnd=match.templateMatchStart==0?1:match.templateMatchStart;for(std::size_t n=0;n<std::min(wrapEnd,item.full.size());++n)suffix.push_back(item.full[n]);}
      std::ostringstream fullPath,tail,alignment;for(std::size_t n=0;n<item.full.size();++n){if(n)fullPath<<'|';fullPath<<formatMatchEvent(item.full[n]);}
      for(std::size_t n=0;n<suffix.size();++n){if(n)tail<<'|';tail<<realizeContinuation(suffix[n],match.key,suffix[n].durationQN.value_or(4.)).label;}
      for(std::size_t n=0;n<match.alignmentTrace.size();++n){const auto& step=match.alignmentTrace[n];if(n)alignment<<'|';alignment<<alignmentOpName(step.operation)<<"(q=";if(step.queryIndex)alignment<<*step.queryIndex;else alignment<<'-';alignment<<",t=";if(step.templateIndex)alignment<<*step.templateIndex;else alignment<<'-';alignment<<",reason="<<reasonNames(step.reasons)<<')';}
      trace<<csv(corpus)<<','<<csv(caseId)<<','<<csv(variant)<<','<<csv(formatKey(match.key))<<','<<(request.style?std::to_string(static_cast<int>(*request.style)):"none")<<','<<(request.preferredIntent?intentName(*request.preferredIntent):"none")<<','<<csv(inputSequence.str())<<','<<csv(item.id)<<','<<intentName(item.intent)<<','<<match.similarity<<','<<match.templateMatchStart<<','<<match.templateMatchEnd<<','<<match.continuationStartIndex<<','<<match.hasContinuation<<','<<item.loopable<<','<<intentName(effective(item,suffix))<<','<<csv(fullPath.str())<<','<<csv(tail.str())<<','<<csv(alignment.str())<<','<<match.candidateRankingScore<<','<<csv(match.disposition)<<'\n';
     }
   }
   const std::string corpusKey(corpus);
   for(std::size_t g=0;g<rec.groups.size();++g)for(std::size_t r=0;r<rec.groups[g].size();++r){const auto& c=rec.groups[g][r];
     if(const auto it=rowOf.find(c.primaryTemplate);it!=rowOf.end()){auto& st=per[it->second];++st.primaryVisible;const auto rank=r+1;if(!st.bestRank||rank<st.bestRank){st.bestRank=rank;st.bestRankGroup=intentName(c.intent);}}
    for(const auto& id:c.supportingTemplates)if(id!=c.primaryTemplate)if(const auto it=rowOf.find(id);it!=rowOf.end())++per[it->second].supportVisible;
    auto& e=exposureByCorpus[corpusKey][c.primaryTemplate];++e.visible;++e.primary;
    if(r==0)++e.rank1;else if(r==1)++e.rank2;else if(r==2)++e.rank3;
    ++intentVisible[corpusKey][intentName(c.intent)];
    for(const auto& id:c.supportingTemplates)if(id!=c.primaryTemplate){auto& support=exposureByCorpus[corpusKey][id];++support.visible;++support.support;}
    if(corpus!="factory_exact_prefix"&&corpus!="factory_skeleton_prefix"&&corpus!="factory_middle_slice"&&corpus!="factory_transposed_G"){
     const auto full=continuationProgression(p,c);std::ostringstream path;for(const auto& chord:full)path<<chord.name<<'|';authoredPaths.push_back(path.str());
    }
    if((corpus=="independent"||corpus=="R01_R02_B08_B11")&&variant=="base"&&listeningWritten.size()<8&&
       (caseId=="COLOR_01"||caseId=="RNB_01"||caseId=="CHROMATIC_01"||caseId=="R02_A"||caseId=="B08_04"||caseId=="JAZZ_01"||caseId=="BORROWED_01"||caseId=="CITY_01")&&
       listeningWritten.insert(std::string(caseId)).second){
     const auto folder=auditDir/"listening";std::filesystem::create_directories(folder);
     midi::CandidateExportContext context;context.tempo=120.;context.meter={4,4};
     if(context.imported.replace(p,TimelineCoordinateMode::RelativeToSelection)){
      const auto payload=midi::candidatePayload(context,c);std::string error;const auto filename=std::string(caseId)+"__"+c.primaryTemplate+".mid";
      if(payload&&midi::writeToFile(payload.payload.smfBytes,folder/filename,error))
        listening<<csv(caseId)<<','<<csv(c.primaryTemplate)<<','<<intentName(c.intent)<<','<<csv(formatKey(c.key))<<','<<csv(filename)<<",synthetic abstract Factory realization\n";
     }
    }
    for(const auto& group:rec.groups)for(std::size_t a=0;a<group.size();++a)for(std::size_t b=a+1;b<group.size();++b){
     auto& pp=pathPairs[corpusKey];++pp.pairs;const auto& left=group[a].continuation;const auto& right=group[b].continuation;const auto common=std::min(left.size(),right.size());std::size_t shared{};
     while(shared<common&&left[shared].degree.degree==right[shared].degree.degree&&left[shared].degree.alteration==right[shared].degree.alteration)++shared;
     if(shared>=2&&shared*2>=common)++pp.engineSimilar;
     if(left.size()==right.size()&&std::equal(left.begin(),left.end(),right.begin(),[](const auto&x,const auto&y){return x.degree.degree==y.degree.degree&&x.degree.alteration==y.degree.alteration&&x.quality==y.quality;}))++pp.sameRelativePath;
    }
    ++pathPairs[corpusKey].runs;
   }
   if(corpus=="independent"||corpus=="R01_R02_B08_B11")for(const auto& match:rec.matches)if(match.candidateRankingScore>=0.f){
    const auto ti=rowOf.find(match.templateId);if(ti==rowOf.end())continue;const auto& item=db.templates[ti->second];
    const auto selected=std::find_if(q.interpretations.begin(),q.interpretations.end(),[&](const auto& x){return x.key.key.tonic==match.key.tonic&&x.key.key.mode==match.key.mode;});
    if(selected==q.interpretations.end())continue;std::vector<MatchEvent> suffix;
    for(std::size_t n=match.continuationStartIndex;n<item.full.size();++n)suffix.push_back(item.full[n]);
    if(item.loopable){const auto wrapEnd=match.templateMatchStart==0?1:match.templateMatchStart;for(std::size_t n=0;n<std::min(wrapEnd,item.full.size());++n)suffix.push_back(item.full[n]);}
    const auto intent=effective(item,suffix);const auto rhythm=estimateRhythmScale(match,q,item);
    float style=0.5f;if(request.style){style=0.25f;for(const auto& sw:item.styleWeights)if(sw.first==*request.style){style=sw.second;break;}}
    const float intentScore=request.preferredIntent?(intent==*request.preferredIntent?1.f:0.5f):0.5f;
    const float cadence=intent==PhraseIntent::Loop?(item.cadence==CadenceType::LoopClosure?1.f:0.5f):
     intent==PhraseIntent::Resolve?(item.cadence==CadenceType::Authentic||item.cadence==CadenceType::PerfectAuthentic||item.cadence==CadenceType::Plagal?1.f:0.5f):
     intent==PhraseIntent::Develop?(item.cadence==CadenceType::None?0.75f:0.5f):
     intent==PhraseIntent::Color?(item.cadence==CadenceType::Modal||item.cadence==CadenceType::Deceptive?0.75f:0.5f):0.5f;
    const float continuation=std::clamp(0.45f+0.1f*static_cast<float>(std::min<std::size_t>(5,suffix.size())),0.f,1.f);
    const float rhythmScore=rhythm.samples>=2?std::clamp(0.5f*match.subScores.rhythmSimilarity+0.5f*rhythm.confidence,0.f,1.f):0.5f;
    std::size_t ins{},del{};float penalty{};for(const auto& step:match.alignmentTrace){if(step.operation==AlignmentOp::QueryInsertion&&!hasReason(step.reasons,MatchReason::ResolvesToMatchedTarget)){++ins;penalty+=8.f;}if(step.operation==AlignmentOp::TemplateDeletion){++del;penalty+=2.f;}}
    const float raw=100.f*(.34f*match.similarity+.11f*match.subScores.skeletonHarmony+.08f*style+.07f*intentScore+.07f*cadence+.12f*continuation+.08f*item.priorWeight+.08f*rhythmScore)-penalty;
     scores<<csv(corpus)<<','<<csv(caseId)<<",match_row,"<<csv(item.id)<<','<<csv(intentName(intent))<<','<<(request.style?std::to_string(static_cast<int>(*request.style)):"none")<<','<<match.similarity<<','<<match.subScores.skeletonHarmony<<','<<style<<','<<intentScore<<','<<cadence<<','<<continuation<<','<<item.priorWeight<<','<<rhythmScore<<",0,"<<ins<<','<<del<<','<<raw<<','<<match.candidateRankingScore<<','<<csv(match.disposition)<<'\n';
   }
    if(corpus=="independent"||corpus=="R01_R02_B08_B11")for(const auto& group:rec.groups)for(const auto& candidate:group){
     std::size_t ins{},del{};for(const auto& m:rec.matches)if(m.templateId==candidate.primaryTemplate&&m.key.tonic==candidate.key.tonic&&m.key.mode==candidate.key.mode){for(const auto& step:m.alignmentTrace){if(step.operation==AlignmentOp::QueryInsertion&&!hasReason(step.reasons,MatchReason::ResolvesToMatchedTarget))++ins;else if(step.operation==AlignmentOp::TemplateDeletion)++del;}break;}
     scores<<csv(corpus)<<','<<csv(caseId)<<",visible_candidate,"<<csv(candidate.primaryTemplate)<<','<<csv(intentName(candidate.intent))<<','<<(request.style?std::to_string(static_cast<int>(*request.style)):"none")<<','<<candidate.subscores.match<<','<<candidate.subscores.skeleton<<','<<candidate.subscores.style<<','<<candidate.subscores.intent<<','<<candidate.subscores.cadence<<','<<candidate.subscores.continuation<<','<<candidate.subscores.prior<<','<<candidate.subscores.rhythm<<','<<candidate.subscores.support<<','<<ins<<','<<del<<",,"<<candidate.rankingScore<<",TARGET_RETAINED\n";
    }
   return rec;
  };
  // Every Factory row: all symbolic exact-prefix endpoints, then bounded
  // skeleton, middle-slice, and transposed-prefix probes. A failed probe is not
  // labelled globally unreachable.
  for(std::size_t ti=0;ti<db.templates.size();++ti){const auto& item=db.templates[ti];const auto n=item.full.size();if(!n)continue;
   const KeySignature key=item.mode==Mode::Major?cMajor:cMinor;
   for(std::size_t end=0;end<n;++end){const auto prefix=makeSymbols(item,0,end,key,1.,0,false,false,false);
    if(end+1<n||(item.loopable&&realizeContinuation(item.full.front(),key,1).label!=realizeContinuation(item.full[end],key,1).label))++per[ti].windows;
    logRun("factory_exact_prefix",item.id,std::string("end=")+std::to_string(end),prefix,key,{});
   }
   const auto end=n>1?n-2:0;auto simplified=makeSymbols(item,0,end,key,1.,0,false,false,false);
   if(item.skeletonIndices.size()<item.full.size()&&end>0){Progression p;double at{};
    for(auto idx:item.skeletonIndices)if(idx<=end){const auto e=realizeContinuation(item.full[idx],key,item.full[idx].durationQN.value_or(4.));ChordEvent chord;chord.name=e.label;chord.startQN=at;chord.durationQN=item.full[idx].durationQN.value_or(4.);chord.openEnded=false;p.push_back(chord);at+=*chord.durationQN;}
    if(p.size()>=2)logRun("factory_skeleton_prefix",item.id,"declared_skeleton",p,key,{});
   }else if(simplified.size()>=2)logRun("factory_skeleton_prefix",item.id,"full_equals_skeleton",simplified,key,{});
   if(n>=5){const std::size_t midEnd=std::min<std::size_t>(n-2,3);if(midEnd>=2){auto middle=makeSymbols(item,1,midEnd,key,1.,0,false,false,false);logRun("factory_middle_slice",item.id,"indices=1.."+std::to_string(midEnd),middle,key,{});}}
   if(n>=2){const auto variantEnd=std::min<std::size_t>(n-2,3);auto transposed=makeSymbols(item,0,variantEnd,{PitchClass::G,item.mode},1.,0,false,false,false);logRun("factory_transposed_G",item.id,"key_relative_exact_prefix",transposed,{PitchClass::G,item.mode},{});}
  }
  struct Independent {const char* id;const char* genre;KeySignature key;std::optional<Style> style;std::optional<PhraseIntent> intent;std::vector<std::string> chords;};
  const std::vector<Independent> independent{
   {"POP_01","Pop",{PitchClass::C,Mode::Major},Style::Pop,PhraseIntent::Resolve,{"C","G","Am","F"}},
   {"POP_02","Pop",{PitchClass::G,Mode::Major},Style::Pop,PhraseIntent::Resolve,{"G","D","Em","C"}},
   {"POP_03","Pop",{PitchClass::D,Mode::Major},Style::Pop,PhraseIntent::Loop,{"D","A","Bm","G"}},
   {"POP_04","Pop",{PitchClass::A,Mode::Minor},Style::Pop,PhraseIntent::Develop,{"Am","F","C","G"}},
   {"MINOR_01","Minor",{PitchClass::A,Mode::Minor},Style::Functional,PhraseIntent::Resolve,{"Am","G","F","E7"}},
   {"MINOR_02","Minor",{PitchClass::D,Mode::Minor},Style::Functional,PhraseIntent::Resolve,{"Dm","Bb","F","A7"}},
   {"MINOR_03","Minor",{PitchClass::E,Mode::Minor},Style::Rock,PhraseIntent::Loop,{"Em","C","Am","B7"}},
   {"JAZZ_01","Jazz",{PitchClass::C,Mode::Major},Style::Jazz,PhraseIntent::Resolve,{"Dm7","G7","Cmaj7","A7"}},
   {"JAZZ_02","Jazz",{PitchClass::F,Mode::Major},Style::Jazz,PhraseIntent::Develop,{"Fmaj7","E7","Am7","D7"}},
   {"JAZZ_03","Jazz",{PitchClass::A,Mode::Minor},Style::Jazz,PhraseIntent::Resolve,{"Bm7b5","E7","Am7","Dm7"}},
   {"JAZZ_04","Jazz",{PitchClass::Bb,Mode::Major},Style::Jazz,PhraseIntent::Loop,{"Cm7","F7","Bbmaj7","G7"}},
   {"RNB_01","R&B",{PitchClass::C,Mode::Major},Style::Rnb,PhraseIntent::Resolve,{"Cmaj9","Dm9","G13","Cmaj9"}},
   {"RNB_02","R&B",{PitchClass::Eb,Mode::Major},Style::Rnb,PhraseIntent::Develop,{"Ebmaj7","Fm7","Gm7","Abmaj7"}},
   {"RNB_03","R&B",{PitchClass::F,Mode::Major},Style::Rnb,PhraseIntent::Loop,{"Fm9","Bb13","Ebmaj9","Abmaj7"}},
   {"CITY_01","CityPop",{PitchClass::C,Mode::Major},Style::CityPop,PhraseIntent::Resolve,{"Fmaj7","G7","Em7","Am7","Dm7","G7","Cmaj7"}},
   {"CITY_02","CityPop",{PitchClass::D,Mode::Major},Style::CityPop,PhraseIntent::Develop,{"Dmaj7","E7","C#m7","F#7"}},
   {"ROCK_01","Rock",{PitchClass::D,Mode::Major},Style::Rock,PhraseIntent::Resolve,{"D","A","G","D"}},
   {"ROCK_02","Rock",{PitchClass::G,Mode::Major},Style::Rock,PhraseIntent::Loop,{"Em","C","G","D"}},
   {"ROCK_03","Rock",{PitchClass::A,Mode::Major},Style::Rock,PhraseIntent::Develop,{"A","G","D","A"}},
   {"FUNCTIONAL_01","Functional",{PitchClass::C,Mode::Major},Style::Functional,PhraseIntent::Resolve,{"C","Am","Dm","G7","C"}},
   {"FUNCTIONAL_02","Functional",{PitchClass::Bb,Mode::Major},Style::Functional,PhraseIntent::Resolve,{"Bb","Gm","Cm","F7"}},
   {"BORROWED_01","Borrowed",{PitchClass::C,Mode::Major},Style::Pop,PhraseIntent::Color,{"C","F","Fm","C"}},
   {"BORROWED_02","Borrowed",{PitchClass::A,Mode::Minor},Style::Rnb,PhraseIntent::Develop,{"Am","Dm","Fm","E7"}},
   {"COLOR_01","Color",{PitchClass::C,Mode::Major},Style::Jazz,PhraseIntent::Color,{"C","Dbmaj7","F#dim7","G7","C"}},
   {"CHROMATIC_01","Chromatic",{PitchClass::A,Mode::Minor},Style::Rock,PhraseIntent::Develop,{"Am","G#dim","G","D"}},
   {"CHROMATIC_02","Chromatic",{PitchClass::C,Mode::Major},Style::Functional,PhraseIntent::Resolve,{"Em","Ebdim7","Dm","G7","C"}},
   {"INVERSIONS_01","Bass",{PitchClass::C,Mode::Major},Style::Pop,PhraseIntent::Resolve,{"Am/C","F/A","G/B","C"}},
   {"SHORT_01","Short",{PitchClass::C,Mode::Major},Style::Pop,PhraseIntent::Develop,{"C","F"}},
   {"LONG_01","Long",{PitchClass::C,Mode::Major},Style::Functional,PhraseIntent::Resolve,{"C","Am","Dm","G7","C","F","Bb"}},
   {"R01","Regression",{PitchClass::A,Mode::Minor},std::nullopt,PhraseIntent::Color,{"Am","G#dim","G","D/F#"}},
   {"R02_C","Regression",{PitchClass::C,Mode::Major},Style::Pop,PhraseIntent::Color,{"F","G","Am"}},
   {"R02_A","Regression",{PitchClass::A,Mode::Minor},Style::Pop,PhraseIntent::Color,{"F","G","Am"}},
   {"B08_01","Regression",{PitchClass::C,Mode::Major},Style::Pop,PhraseIntent::Resolve,{"C","Cmaj7/B"}},
   {"B08_02","Regression",{PitchClass::C,Mode::Major},Style::Pop,PhraseIntent::Resolve,{"C","Cmaj7/B","C7/Bb"}},
   {"B08_03","Regression",{PitchClass::C,Mode::Major},Style::Pop,PhraseIntent::Resolve,{"C","Cmaj7/B","C7/Bb","F/A"}},
   {"B08_04","Regression",{PitchClass::C,Mode::Major},Style::Pop,PhraseIntent::Resolve,{"C","Cmaj7/B","C7/Bb","F/A","Fm/Ab"}},
   {"B11_01","Regression",{PitchClass::C,Mode::Major},Style::Jazz,PhraseIntent::Color,{"Cmaj9","Dbmaj9"}},
   {"B11_02","Regression",{PitchClass::C,Mode::Major},Style::Jazz,PhraseIntent::Color,{"Cmaj9","Dbmaj9","Cmaj9"}}
  };
  std::vector<Progression> authored;std::vector<RecommendationRequest> requests;
  for(const auto& sample:independent){Progression p;double at{};for(const auto& name:sample.chords){ChordEvent e;e.name=name;e.startQN=at;e.durationQN=4.;e.openEnded=false;p.push_back(e);at+=4.;}authored.push_back(std::move(p));requests.push_back({sample.style,sample.intent,{}});}
  std::size_t authoredOpportunity{},regressionOpportunity{},zeroVisible{};
  const auto runAuthored=[&](std::size_t si,const Progression& p,const RecommendationRequest& request,std::string_view corpus,std::string_view variant){
   const auto& sample=independent[si];const auto imported=midiTransform(p,0,0,0,false,false,1.,0,false);if(!imported){++failedRecognitions;++transformFailureReasons[lastTransformFailure];return RecommendationSet{};}const auto rec=logRun(corpus,sample.id,variant,*imported,sample.key,request);
   if(corpus=="independent")++authoredOpportunity;else if(corpus=="R01_R02_B08_B11")++regressionOpportunity;
   if(!rec.stats.output)++zeroVisible;
   return rec;
  };
  // Fixed independent corpus plus explicit R01/R02/B08/B11 regression anchors.
  for(std::size_t i=0;i<independent.size();++i)runAuthored(i,authored[i],requests[i],
      independent[i].genre==std::string_view("Regression")?"R01_R02_B08_B11":"independent","base");
  constexpr Style allStyles[]{Style::Pop,Style::Rock,Style::Rnb,Style::Jazz,Style::CityPop,Style::Functional};
  constexpr PhraseIntent allIntents[]{PhraseIntent::Resolve,PhraseIntent::Develop,PhraseIntent::Loop,PhraseIntent::Color};
  for(auto si:{0u,4u,7u,11u,14u,16u,19u,21u,24u,25u})for(auto style:allStyles)for(auto intent:allIntents){RecommendationRequest r;r.style=style;r.preferredIntent=intent;runAuthored(si,authored[si],r,"style_intent_grid",std::to_string(static_cast<int>(style))+"_"+std::to_string(static_cast<int>(intent)));}
  std::ofstream stability(auditDir/"stability-matrix.csv");stability<<"case_id,variant,key,input_count,recognized_count,recognized,aligned,paths,distinct,score_gate,base_visible,variant_visible,stable_ids,shared_ids,overlap_ratio,exact_set_stable,base_candidates,variant_candidates,rank_changes,elapsed_ms,V4_A08_base_match,V4_A08_variant_match,V4_B08_base_match,V4_B08_variant_match,import_status\n";
   std::size_t variantRuns{},stablePairs{},equalSets{},comparableSetPairs{},emptyToEmpty{},emptyToNonempty{},nonemptyToEmpty{};
  for(auto si:{0u,1u,3u,7u,8u,11u,14u,16u,21u,24u,25u,26u,29u,30u,31u,32u,33u,34u,35u,36u})if(si<authored.size()){
   const auto& sample=independent[si];const auto& p=authored[si];const auto baseMidi=midiTransform(p,0,0,0,false,false,1.,0,false);if(!baseMidi){++failedRecognitions;++transformFailureReasons[lastTransformFailure];continue;}const auto base=logRun("independent",sample.id,"stability_base_midi",*baseMidi,sample.key,requests[si]);std::set<std::string> baseIds;std::map<std::string,std::pair<std::string,std::size_t>> baseRanks;for(std::size_t g=0;g<base.groups.size();++g)for(std::size_t r=0;r<base.groups[g].size();++r){const auto& c=base.groups[g][r];baseIds.insert(c.primaryTemplate);baseRanks[c.primaryTemplate]={intentName(c.intent),r+1};}
   const auto describeCandidates=[](const RecommendationSet& set){std::ostringstream out;bool first=true;for(std::size_t g=0;g<set.groups.size();++g)for(std::size_t r=0;r<set.groups[g].size();++r){const auto& c=set.groups[g][r];const auto& s=c.subscores;if(!first)out<<';';first=false;out<<c.primaryTemplate<<'['<<intentName(c.intent)<<'#'<<r+1<<'='<<std::fixed<<std::setprecision(3)<<c.rankingScore<<";m="<<s.match<<";sk="<<s.skeleton<<";style="<<s.style<<";intent="<<s.intent<<";cad="<<s.cadence<<";cont="<<s.continuation<<";prior="<<s.prior<<";rhythm="<<s.rhythm<<";support="<<s.support<<']';}return out.str();};
   const auto baseDescription=describeCandidates(base);
   const auto describeFocusMatch=[&](const RecommendationSet& set,std::string_view wanted){std::ostringstream out;for(const auto& match:set.matches)if(match.templateId==wanted){const auto found=rowOf.find(match.templateId);const auto* item=found==rowOf.end()?nullptr:&db.templates[found->second];out<<wanted<<"{sim="<<std::fixed<<std::setprecision(6)<<match.similarity<<";skeleton="<<match.subScores.skeletonHarmony<<";full="<<match.subScores.fullHarmony<<";key="<<match.subScores.keyCompatibility<<";rhythm="<<match.subScores.rhythmSimilarity<<";rawCost="<<match.rawCost<<";window="<<match.templateMatchStart<<".."<<match.templateMatchEnd<<";continuationStart="<<match.continuationStartIndex<<";hasContinuation="<<match.hasContinuation<<";loopable="<<(item?item->loopable:false)<<";score="<<match.candidateRankingScore<<";disposition="<<match.disposition<<";alignment=";for(std::size_t n=0;n<match.alignmentTrace.size();++n){const auto& step=match.alignmentTrace[n];if(n)out<<'|';out<<alignmentOpName(step.operation)<<"(q=";if(step.queryIndex)out<<*step.queryIndex;else out<<'-';out<<",t=";if(step.templateIndex)out<<*step.templateIndex;else out<<'-';out<<",reason="<<reasonNames(step.reasons)<<')';}if(item){out<<";suffix=";bool first=true;for(std::size_t n=match.continuationStartIndex;n<item->full.size();++n){if(!first)out<<'|';first=false;out<<realizeContinuation(item->full[n],match.key,item->full[n].durationQN.value_or(4.)).label;}if(item->loopable){const auto wrapEnd=match.templateMatchStart==0?1:match.templateMatchStart;for(std::size_t n=0;n<std::min(wrapEnd,item->full.size());++n){if(!first)out<<'|';first=false;out<<realizeContinuation(item->full[n],match.key,item->full[n].durationQN.value_or(4.)).label;}}}out<<'}';break;}return out.str();};
   const auto baseA08=describeFocusMatch(base,"V4_A08"),baseB08=describeFocusMatch(base,"V4_B08");
    const auto variant=[&](const char* name,int semi,int octave,int bassOctave,bool inv,bool omit,double scale,int shape,bool open,bool splitChannels){
     ++variantRuns;const auto transformed=midiTransform(p,semi,octave,bassOctave,inv,omit,scale,shape,open,splitChannels);
      if(!transformed){++failedRecognitions;++transformFailureReasons[lastTransformFailure];stability<<sample.id<<','<<name<<','<<formatKey(sample.key)<<','<<p.size()<<",0,,0,0,0,0,"<<base.stats.output<<",0,0,0,,0,"<<csv(baseDescription)<<",,0,0,"<<csv(baseA08)<<",,"<<csv(baseB08)<<",,"<<csv(lastTransformFailure)<<'\n';return;}
    auto key=sample.key;key.tonic=static_cast<PitchClass>((static_cast<int>(key.tonic)+semi+120)%12);
    const auto rec=logRun("independent",sample.id,name,*transformed,key,requests[si]);std::set<std::string> ids;for(const auto&g:rec.groups)for(const auto&c:g)ids.insert(c.primaryTemplate);
     std::size_t shared{},rankChanges{};std::map<std::string,std::pair<std::string,std::size_t>> variantRanks;for(std::size_t g=0;g<rec.groups.size();++g)for(std::size_t r=0;r<rec.groups[g].size();++r)variantRanks[rec.groups[g][r].primaryTemplate]={intentName(rec.groups[g][r].intent),r+1};for(const auto&id:baseIds)if(ids.contains(id)){++shared;if(baseRanks[id]!=variantRanks[id])++rankChanges;}
    const double overlap=static_cast<double>(shared)/std::max<std::size_t>(1,std::max(baseIds.size(),ids.size()));const bool bothNonempty=!baseIds.empty()&&!ids.empty();const bool exactSet=bothNonempty&&ids==baseIds;const bool stableSet=bothNonempty&&overlap>=.8;
    if(baseIds.empty()&&ids.empty())++emptyToEmpty;else if(baseIds.empty())++emptyToNonempty;else if(ids.empty())++nonemptyToEmpty;else{++comparableSetPairs;if(stableSet)++stablePairs;if(exactSet)++equalSets;}std::ostringstream desc;
    for(std::size_t j=0;j<transformed->size();++j){if(j)desc<<'|';desc<<(*transformed)[j].name<<'@'<<(*transformed)[j].startQN<<'+'<<(*transformed)[j].durationQN.value_or(-1);}
     stability<<sample.id<<','<<name<<','<<formatKey(key)<<','<<p.size()<<','<<transformed->size()<<','<<csv(desc.str())<<','<<rec.stats.aligned<<','<<rec.stats.pathPassed<<','<<rec.stats.distinct<<','<<rec.stats.scorePassed<<','<<base.stats.output<<','<<rec.stats.output<<','<<stableSet<<','<<shared<<','<<overlap<<','<<exactSet<<','<<csv(baseDescription)<<','<<csv(describeCandidates(rec))<<','<<rankChanges<<','<<rec.stats.totalMs<<','<<csv(baseA08)<<','<<csv(describeFocusMatch(rec,"V4_A08"))<<','<<csv(baseB08)<<','<<csv(describeFocusMatch(rec,"V4_B08"))<<",ok\n";
   };
    variant("transpose_up5",5,0,0,false,false,1,0,false,false);variant("octave_up",0,1,0,false,false,1,0,false,false);
    variant("octave_down",0,-1,0,false,false,1,0,false,false);
    variant("inverted_bass",0,0,0,true,false,1,0,false,false);variant("omit_fifth",0,0,0,false,true,1,0,false,false);
    variant("bass_down_octave",0,0,-1,false,false,1,0,false,false);variant("tempo_half",0,0,0,false,false,.5,0,false,false);
      variant("tempo_double",0,0,0,false,false,2,0,false,false);variant("3plus5QN",0,0,0,false,false,1,1,false,false);
    variant("OPEN",0,0,0,false,false,1,0,true,false);variant("bass_and_chords_split_channels",0,0,0,false,false,1,0,false,true);
  }
  std::ofstream ablation(auditDir/"score-ablations.csv");
  ablation<<"case_id,ablation,visible_baseline,visible_counterfactual,score_gate_baseline,score_gate_counterfactual,top1_baseline,top1_counterfactual,elapsed_ms\n";
  const std::vector<std::string> ablations{"match","skeleton_score","style","intent","cadence","continuation","prior","rhythm","support","insertion_penalty","deletion_penalty","intent_completion_tiebreak","tendency_conservative","tendency_bold"};
  for(auto si:{0u,3u,7u,11u,14u,16u,20u,21u,24u,25u,26u,29u,30u,31u,32u,33u,34u,35u,36u})if(si<authored.size()){
   const auto& sample=independent[si];const auto& req=requests[si];const auto base=logRun("ablation",sample.id,"base",authored[si],sample.key,req);
   for(const auto& name:ablations){RecommendationWeights w;float removed{};
    if(name=="match")removed=w.match;else if(name=="skeleton_score")removed=w.skeleton;else if(name=="style")removed=w.style;else if(name=="intent")removed=w.intent;else if(name=="cadence")removed=w.cadence;else if(name=="continuation")removed=w.continuation;else if(name=="prior")removed=w.prior;else if(name=="rhythm")removed=w.rhythm;else if(name=="support")removed=w.support;
    if(removed){const float f=1.f/(1.f-removed);w.match*=f;w.skeleton*=f;w.style*=f;w.intent*=f;w.cadence*=f;w.continuation*=f;w.prior*=f;w.rhythm*=f;w.support*=f;
     if(name=="match")w.match=0;else if(name=="skeleton_score")w.skeleton=0;else if(name=="style")w.style=0;else if(name=="intent")w.intent=0;else if(name=="cadence")w.cadence=0;else if(name=="continuation")w.continuation=0;else if(name=="prior")w.prior=0;else if(name=="rhythm")w.rhythm=0;else w.support=0;
     }else if(name=="insertion_penalty")w.unexplainedInsertionPenalty=0;else if(name=="deletion_penalty")w.templateDeletionPenalty=0;else if(name=="intent_completion_tiebreak")w.intentCompletionTieBreak=0;
     auto counterRequest=req;if(name=="tendency_conservative")counterRequest.tendency=HarmonicTendency::Conservative;else if(name=="tendency_bold")counterRequest.tendency=HarmonicTendency::Bold;
    AnalysisContext cx;cx.forcedKey=sample.key;cx.timeSigNumerator=4;cx.timeSigDenominator=4;const auto q=makeMatchQuery(authored[si],cx);
     const auto start=std::chrono::steady_clock::now();const auto test=recommendContinuations(q,index,counterRequest,w);const auto stop=std::chrono::steady_clock::now();
    const ContinuationCandidate *a{},*b{};for(const auto&g:base.groups)for(const auto&c:g)if(!a||c.rankingScore>a->rankingScore)a=&c;
    for(const auto&g:test.groups)for(const auto&c:g)if(!b||c.rankingScore>b->rankingScore)b=&c;
    ablation<<sample.id<<','<<name<<','<<base.stats.output<<','<<test.stats.output<<','<<base.stats.scorePassed<<','<<test.stats.scorePassed<<','<<(a?a->id:"-")<<','<<(b?b->id:"-")<<','<<std::chrono::duration<double,std::milli>(stop-start).count()<<'\n';
   }
  }
  std::ofstream expose(auditDir/"recommendation-exposure.csv");
  expose<<"corpus,FactoryID,intent,styles,complexity,event_count,visible_primary,visible_as_support,visible_total,rank1,rank2,rank3\n";
  for(const auto&[corpus,population]:exposureByCorpus)for(const auto&item:db.templates){const auto found=population.find(item.id);const Exposure e=found==population.end()?Exposure{}:found->second;std::ostringstream st;for(const auto&sw:item.styleWeights)st<<static_cast<int>(sw.first)<<':'<<sw.second<<'|';
   expose<<csv(corpus)<<','<<csv(item.id)<<','<<intentName(item.intent)<<','<<csv(st.str())<<','<<static_cast<int>(item.complexityLevel)<<','<<item.full.size()<<','<<e.primary<<','<<e.support<<','<<e.visible<<','<<e.rank1<<','<<e.rank2<<','<<e.rank3<<'\n';
  }
  std::ofstream strataOut(auditDir/"score-distribution-strata.csv");strataOut<<"corpus,mode,input_length_bucket,complexity,template_intent,requested_style,preferred_intent,match_rows,scoreable_rows,score_gate_rows,mean_similarity,mean_recorded_score\n";
  for(const auto&[key,s]:scoreStrata){std::istringstream parts(key);std::string corpus,mode,length,complexity,intent,style,preferred;std::getline(parts,corpus,'|');std::getline(parts,mode,'|');std::getline(parts,length,'|');std::getline(parts,complexity,'|');std::getline(parts,intent,'|');std::getline(parts,style,'|');std::getline(parts,preferred,'|');strataOut<<csv(corpus)<<','<<mode<<','<<length<<','<<complexity<<','<<intent<<','<<style<<','<<preferred<<','<<s.rows<<','<<s.scoreable<<','<<s.gate<<','<<(s.rows?s.similaritySum/s.rows:0)<<','<<(s.scoreable?s.scoreSum/s.scoreable:0)<<'\n';}
  std::ofstream summary(auditDir/"summary.txt");summary<<std::setprecision(8)<<"Factory rows="<<db.templates.size()<<"\n";
  std::size_t eventPositions{},terminalNonLoop{},loopable{},zeroPathOpportunity{};std::map<std::string,std::size_t> intents,modes,qualities;std::map<std::size_t,std::size_t> lengths;
  for(std::size_t i=0;i<db.templates.size();++i){const auto&item=db.templates[i];intents[intentName(item.intent)]++;modes[item.mode==Mode::Major?"major":"minor"]++;lengths[item.full.size()]++;
   eventPositions+=item.full.size();if(item.full.size()>0&&!item.loopable)++terminalNonLoop;if(item.loopable)++loopable;if(per[i].windows==0)++zeroPathOpportunity;
   for(const auto&e:item.full)qualities[std::to_string(static_cast<int>(e.quality))]++;
  }
  summary<<"event positions="<<eventPositions<<" terminal nonloop templates="<<terminalNonLoop<<" loopable templates="<<loopable<<" with no tested natural continuation position="<<zeroPathOpportunity<<"\nIntents:";for(const auto&[k,v]:intents)summary<<' '<<k<<'='<<v;
  summary<<"\nModes:";for(const auto&[k,v]:modes)summary<<' '<<k<<'='<<v;
  summary<<"\nPath lengths:";for(const auto&[k,v]:lengths)summary<<' '<<k<<'='<<v;
  for(const auto&[corpus,bands]:scoreBands){summary<<"\nScore bands "<<corpus<<':';for(const auto&[band,count]:bands)summary<<' '<<band<<'='<<count;}
  for(const auto&[corpus,population]:exposureByCorpus){
   std::vector<std::pair<std::size_t,std::string>> ranked;std::size_t total{},supportTotal{};
   for(const auto&item:db.templates){const auto found=population.find(item.id);const auto primary=found==population.end()?0:found->second.primary;const auto support=found==population.end()?0:found->second.support;ranked.emplace_back(primary,item.id);total+=primary;supportTotal+=support;}
   std::sort(ranked.begin(),ranked.end(),[](const auto&a,const auto&b){return a.first>b.first;});
   const auto zero=std::count_if(ranked.begin(),ranked.end(),[](const auto&x){return x.first==0;});
   summary<<"\nExposure "<<corpus<<" primary-visible appearances="<<total<<" unique primary IDs="<<ranked.size()-zero<<" zero-primary IDs="<<zero<<" supporting-source attributions="<<supportTotal;
   for(auto top:{10u,50u,100u}){std::size_t count{};for(std::size_t i=0;i<std::min<std::size_t>(top,ranked.size());++i)count+=ranked[i].first;summary<<" Top"<<top<<" share="<<(total?static_cast<double>(count)/total:0);}
   summary<<'\n';
  }
  for(const auto&[corpus,pairs]:pathPairs)summary<<"Top-three within-intent path pairs "<<corpus<<" queries="<<pairs.runs<<" pairs="<<pairs.pairs<<" engine-prefix-similar="<<pairs.engineSimilar<<" ratio="<<(pairs.pairs?static_cast<double>(pairs.engineSimilar)/pairs.pairs:0)<<" same-relative-full-path="<<pairs.sameRelativePath<<" ratio="<<(pairs.pairs?static_cast<double>(pairs.sameRelativePath)/pairs.pairs:0)<<'\n';
  summary<<"Input cases="<<independent.size()<<" fixed authored seed=none; style-intent grid=240; MIDI transform pairs="<<variantRuns<<" recognition failures="<<failedRecognitions<<" both-nonempty comparable="<<comparableSetPairs<<" stable(overlap>=.8)="<<stablePairs<<" exact-set-stable="<<equalSets<<" empty-both="<<emptyToEmpty<<" empty-to-nonempty="<<emptyToNonempty<<" nonempty-to-empty="<<nonemptyToEmpty<<"\n";
  for(const auto&[reason,count]:transformFailureReasons)summary<<"Transform failure "<<reason<<'='<<count<<'\n';
  summary<<"Recommendation probes="<<totalRuns<<" summed engine ms="<<totalEngineMs<<" wall ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-auditStarted).count()<<"\n";
  summary<<"Match gate=.60; score gate=60; per-group display cap=3; shortlist includes all 657 rows because 657 < 800. Phase3A exception OFF (default RecommendationWeights).\nQuality enums:";for(const auto&[k,v]:qualities)summary<<' '<<k<<'='<<v;
  for(std::size_t i=0;i<db.templates.size();++i){auto&s=per[i];if(!s.queries)++s.blockers["not_aligned_in_tested_space"];std::string obstacle;std::size_t max{};for(const auto&[key,count]:s.blockers)if(count>max){max=count;obstacle=key;}
   const auto&item=db.templates[i];std::ostringstream st,counts;for(const auto&sw:item.styleWeights)st<<static_cast<int>(sw.first)<<':'<<sw.second<<'|';for(const auto&[key,count]:s.blockers)counts<<key<<':'<<count<<'|';
   rows<<csv(item.id)<<','<<(item.mode==Mode::Major?"major":"minor")<<','<<intentName(item.intent)<<','<<csv(st.str())<<','<<item.full.size()<<','<<item.skeletonIndices.size()<<','<<s.windows<<','<<s.probeRuns<<','<<s.modeCompatibleRuns<<','<<s.queries<<','<<s.aligned<<','<<s.paths<<','<<s.scoreGate<<','<<s.primaryVisible<<','<<s.supportVisible<<','<<s.bestScore<<','<<s.bestSimilarity<<','<<csv(s.bestInput)<<','<<csv(s.bestDisposition)<<','<<csv(s.bestRankGroup)<<','<<s.bestRank<<','<<csv(obstacle)<<','<<csv(counts.str())<<'\n';
  }
  std::ofstream diversity(auditDir/"visible-diversity.txt");std::set<std::string> uniquePaths(authoredPaths.begin(),authoredPaths.end());
  diversity<<"Authored/regression visible full paths="<<authoredPaths.size()<<" unique exact paths="<<uniquePaths.size()<<" repeated appearances="<<authoredPaths.size()-uniquePaths.size()<<"\n";
  for(const auto&[corpus,intentsForCorpus]:intentVisible)for(const auto&[intent,count]:intentsForCorpus)diversity<<"intent "<<corpus<<' '<<intent<<'='<<count<<'\n';
  const auto allocationBytes=phase3aMemory::bytes;const auto allocationCalls=phase3aMemory::calls;sampleProcessMemory();phase3aMemory::enabled=false;
  summary<<"Audit allocation traffic new_bytes="<<allocationBytes-memoryStartBytes<<" new_calls="<<allocationCalls<<"; cumulative allocations, not simultaneous memory.\n";
#if defined(_WIN32)
  summary<<"Peak working set bytes="<<peakWorkingSetBytes<<" (sampled from Windows process counters).\n";
#else
  summary<<"Peak working set unavailable on this platform.\n";
#endif
  std::cout<<"SYSTEM_AUDIT runs="<<totalRuns<<" wall_ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-auditStarted).count()<<" output="<<auditDir.string()<<" recognitionFailures="<<failedRecognitions<<'\n';
 }
 std::cout<<"matrix failures="<<failures<<'\n';return failures?1:0;
}
