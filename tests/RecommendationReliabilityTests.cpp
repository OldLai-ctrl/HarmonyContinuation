#include "midi/StandardMidiFileWriter.h"
#include "midi/StandardMidiFileReader.h"
#include "midi/MidiHarmonyExtractor.h"
#include "midi/MidiImportWorkflow.h"
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
using namespace harmony;
int main(int argc,char** argv) {
 const std::filesystem::path out=argc>1?argv[1]:"reliability";
 std::filesystem::create_directories(out);
 const auto db=library::loadFactory(HC_FACTORY_DB_PATH); if(!db)return 2;
 const CandidateIndex index(db.templates);
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
 std::cout<<"matrix failures="<<failures<<'\n';return failures?1:0;
}
