#include "plugin/VstXmlChordParser.h"
#include "plugin/RecommendationWorker.h"
#include "session/ProductServices.h"
#include "library/ProgressionLibrary.h"
#include "core/ChordPitchSet.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <thread>
#include "midi/MidiHarmonyExtractor.h"
using namespace harmony;
// Adversarial synthetic fixtures, NEVER represented as captured Cubase input.
int main(int argc,char** argv){
 std::filesystem::path out=argc>1?argv[1]:"cubase-reliability";std::filesystem::create_directories(out);
 const auto db=library::loadFactory(HC_FACTORY_DB_PATH);if(!db)return 2;CandidateIndex index(db.templates);
 std::ofstream log(out/"comparison.tsv"),ev(out/"events.tsv");
 log<<"case\tvariant\trhythm\topen\tforced\tcount\tdefaultCount\tmoreCount\tmatched\tretained\tdefaultVisible\tmoreAccessible\trank\tsimilarity\trhythmScore\tstartIndex\ttailPassed\ttonesPassed\tscorePassed\tstatus\n";
 ev<<"case\tvariant\tname\tkeyNote\tbassNote\tpitches\tmask\troot\tquality\tbass\tintervalMask\tcolorMask\tstart\tduration\topen\n";
 int failures=0;
 const std::vector<std::vector<std::string>> cases={{"C","Cmaj7/B"},{"C","Cmaj7/B","C7/Bb"},{"C","Cmaj7/B","C7/Bb","F/A"},{"C","Cmaj7/B","C7/Bb","F/A","Fm/Ab"},{"Cmaj9","Dbmaj9"},{"C","F","F#dim7","G"},{"C","Am","F","G"},{"Dm7","G7","Cmaj7"}};
 for(size_t ci=0;ci<cases.size();++ci)for(int variant=0;variant<5;++variant)for(int rhythm=0;rhythm<5;++rhythm)for(int open=0;open<2;++open)for(int forced=0;forced<2;++forced){
  std::ostringstream xml;xml<<"<vst-xml version=\"1.4\"><sourceApp>CubaseSyntheticAdversarial</sourceApp>";double at=rhythm==3?.375:0.;
  constexpr const char* notes[]={"C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B"};
  for(size_t i=0;i<cases[ci].size();++i){auto tones=chordPitches(cases[ci][i]);unsigned mask=tones.intervals;int bass=tones.bass;
   if(variant==1)bass=(tones.root+4)%12; // auto voicing differs from explicit slash
   if(variant==2)mask&=~(1u<<7); // omitted fifth
   if(variant==3){bass=(tones.root+7)%12;mask&=~(1u<<7);}
   if(variant==4&&i==2&&ci<4)mask=(1u<<0)|(1u<<4)|(1u<<7); // name C7/Bb, incomplete performed seventh
   xml<<"<chord id=\""<<i<<"\"><name>"<<cases[ci][i]<<"</name><keyNote>"<<tones.root<<"</keyNote><bassNote>"<<bass<<"</bassNote><pitches>";
   for(int n=0;n<12;++n)if(mask&(1u<<n))xml<<notes[(tones.root+n)%12]<<';';
   xml<<"</pitches><mask>0x"<<std::hex<<mask<<std::dec<<"</mask><projectTime domain=\"quarterNotes\">"<<at<<"</projectTime></chord>";
   at+=rhythm==0?4.:rhythm==1?(i%2?4./3:8./3):rhythm==2?(i%2?1.:7.):rhythm==3?(i%2?1.5:2.5):1.375;
  }xml<<"</vst-xml>";
  auto parsed=plugin::VstXmlChordParser{}.parse(xml.str());if(!parsed.ok())return 3;auto p=parsed.chords;
  if(!open){p.back().durationQN=4.;p.back().openEnded=false;}
  AnalysisContext ctx;if(forced)ctx.forcedKey=KeySignature{PitchClass::C,Mode::Major};
  auto query=makeMatchQuery(p,ctx);RecommendationWeights w;w.perGroup=10;auto rec=recommendContinuations(query,index,{},w);
  if(argc>2&&variant==2&&rhythm==0&&!forced) {
   plugin::RecommendationWorker worker(HC_FACTORY_DB_PATH,out/("worker-"+std::to_string(ci)+"-"+std::to_string(open)+".db"));
   auto generation=worker.submit(p,ctx);std::optional<plugin::WorkerResult> result;
   for(int n=0;n<1000&&!result;++n){result=worker.takeLatest();if(!result)std::this_thread::sleep_for(std::chrono::milliseconds(5));}
   if(!result||!result->error.empty()||result->generation!=generation||result->recommendations.stats.output!=rec.stats.output)++failures;
   if(ci<5&&result){bool found=false;for(const auto& group:result->recommendations.groups)for(const auto& c:group)
    if(std::find(c.supportingTemplates.begin(),c.supportingTemplates.end(),ci<4?"V4_B08":"V4_B11")!=c.supportingTemplates.end())found=true;if(!found)++failures;}
   auto diagnostics=session::recommendationDiagnostics(p,ctx,{},rec);
   if(diagnostics.find("pitches=")==std::string::npos||diagnostics.find("disposition=")==std::string::npos||diagnostics.find("STAGES")==std::string::npos)++failures;
   std::ofstream(out/("diagnostic-"+std::to_string(ci)+"-"+std::to_string(open)+".txt"))<<diagnostics;
  }
  auto shown=session::presentationIndices(rec,{});session::RecommendationVisibilityPolicy more;more.maxPerGroup=10;auto all=session::presentationIndices(rec,more);
  const char* target=ci<4?"V4_B08":ci==4?"V4_B11":"";
  bool matched=false,retained=false,visible=false,accessible=false;int rank=-1;float sim=0,rs=0;size_t start=0,count=0,dc=0,mc=0;
  for(auto& m:rec.matches)if(m.templateId==target){matched=true;sim=m.similarity;rs=m.subScores.rhythmSimilarity;start=m.continuationStartIndex;}
  for(size_t g=0;g<4;++g){count+=rec.groups[g].size();dc+=shown[g].size();mc+=all[g].size();for(size_t n=0;n<rec.groups[g].size();++n){auto& c=rec.groups[g][n];if(std::find(c.supportingTemplates.begin(),c.supportingTemplates.end(),target)!=c.supportingTemplates.end()){
   retained=true;rank=int(n+1);visible=std::find(shown[g].begin(),shown[g].end(),n)!=shown[g].end();accessible=std::find(all[g].begin(),all[g].end(),n)!=all[g].end();
   if(argc>2&&(c.continuationStart!=p.size()|| (ci==4&&c.continuation.front().label!="Cmaj9")))++failures;
  }}}
  const char* status=!count?"NO_RECOMMENDATIONS":!target[0]?"NO_TARGET_REQUIRED":!retained?"NOT_RETAINED":visible?"DEFAULT_VISIBLE":accessible?"MORE_ACCESSIBLE":"TARGET_RETAINED";
  log<<ci<<'\t'<<variant<<'\t'<<rhythm<<'\t'<<open<<'\t'<<forced<<'\t'<<count<<'\t'<<dc<<'\t'<<mc<<'\t'<<matched<<'\t'<<retained<<'\t'<<visible<<'\t'<<accessible<<'\t'<<rank<<'\t'<<sim<<'\t'<<rs<<'\t'<<start<<'\t'<<rec.stats.tailPassed<<'\t'<<rec.stats.realizationPassed<<'\t'<<rec.stats.scorePassed<<'\t'<<status<<'\n';
  if(rhythm==0&&!forced)for(auto& e:p){auto n=normalizeChord(e);ev<<ci<<'\t'<<variant<<'\t'<<e.name<<'\t'<<e.keyNoteValue.value_or(-1)<<'\t'<<e.bassNoteValue.value_or(-1)<<'\t'<<e.extensions.pitches.value_or("")<<'\t'<<e.extensions.mask.value_or("")<<'\t'<<int(n.root.value_or(PitchClass::C))<<'\t'<<int(n.quality)<<'\t'<<int(n.bass.value_or(PitchClass::C))<<'\t'<<n.intervalMask<<'\t'<<n.colorMask<<'\t'<<e.startQN<<'\t'<<e.durationQN.value_or(-1)<<'\t'<<e.openEnded<<'\n';}
  if(argc>2&&(!count||(ci<5&&(!retained||!accessible))))++failures;
 }

 if(argc>2){
  auto host=[](const char* name,const char* pitches,int bass){ChordEvent e;e.name=name;e.rawName=e.name;e.source=ChordSource::CubaseDrop;e.keyNoteValue=0;e.bassNoteValue=bass;e.extensions.pitches=pitches;return e;};
  auto a=host("Cmaj9","C;E;B;D;",4);auto n=normalizeChord(a);
  if(n.quality!=ChordQuality::Major7||n.explicitBass||n.performedBass!=PitchClass::E||!(n.matchingIntervalMask&(1u<<7))||n.intervalMask&(1u<<7))++failures;
  auto b=normalizeChord(host("C9","C;E;Bb;D;",7));if(b.quality!=ChordQuality::Dominant7||b.matchingIntervalMask==n.matchingIntervalMask)++failures;
  auto conflict=normalizeChord(host("Cmaj9","C;Eb;Bb;D;",0));if(!conflict.fieldConflict||conflict.compatibilityUncertainty<.35f)++failures;
  auto slash=normalizeChord(host("C7/Bb","C;E;Bb;",4));if(!slash.explicitBass||slash.bass!=static_cast<PitchClass>(10))++failures;
  auto altered=normalizeChord(host("C7#5","C;E;Ab;Bb;",0));if(altered.matchingIntervalMask&(1u<<7)||!(altered.matchingIntervalMask&(1u<<8)))++failures;
  // Explicit wrong slash/quality still cannot identify the protected template.
  for(const auto wrong:{"C7/A","Cm7/Bb","Cmaj7/Bb"}) {
   Progression p;for(const auto name:{"C","Cmaj7/B",wrong}){auto e=host(name,"",0);e.extensions.pitches.reset();e.startQN=p.size()*4.;p.push_back(e);}
   auto rec=recommendContinuations(makeMatchQuery(p),index);bool retained=false;
   for(const auto& g:rec.groups)for(const auto& c:g)if(std::find(c.supportingTemplates.begin(),c.supportingTemplates.end(),"V4_B08")!=c.supportingTemplates.end())retained=true;
   if(retained)++failures;
  }
  auto six=host("A6/9","A;Db;Gb;B;",9);six.keyNoteValue=9;auto sixn=normalizeChord(six);
  if(sixn.explicitBass||!(sixn.matchingIntervalMask&(1u<<2))||!(sixn.matchingIntervalMask&(1u<<9)))++failures;
  Progression skeleton;for(auto label:{"C","F","F#dim7","G"}){ChordEvent e;e.name=label;e.startQN=skeleton.size()*4.;e.durationQN=4.;e.openEnded=false;skeleton.push_back(e);}
  if(analyzeHarmony(skeleton).skeletonIndices.size()!=4)++failures;
  // Direct MIDI extraction is separate evidence from the Cubase XML route.
  std::ofstream ml(out/"midi-timing.tsv");ml<<"variant\timported\tname\tstart\tduration\trecommendations\n";
  for(int variant=0;variant<5;++variant){midi::MidiFile f;f.ppq=960;f.format=0;f.totalQN=16.;f.tracks.push_back({0,"Synthetic timing",16,0});
   for(size_t i=0;i<skeleton.size();++i){auto tones=chordPitches(skeleton[i]);std::vector<int> pitches{36+tones.bass};for(int k=0;k<12;++k)if(tones.intervals&(1u<<k))pitches.push_back(60+tones.root+k);
    for(size_t k=0;k<pitches.size();++k){double shift=variant==2?k*.012:variant==4?.035:0.;double end=skeleton[i].startQN+4.+(variant==1?-.08:variant==3&&i<3?.06:0.);f.notes.push_back({pitches[k],90,0,0,skeleton[i].startQN+shift,end-skeleton[i].startQN-shift});}}
   auto extracted=midi::extractHarmony(f);auto rec=recommendContinuations(makeMatchQuery(extracted.chords),index);
   if(!extracted||extracted.chords.size()!=4||!rec.stats.output)++failures;
   for(auto& e:extracted.chords)ml<<variant<<'\t'<<extracted.chords.size()<<'\t'<<e.name<<'\t'<<e.startQN<<'\t'<<e.durationQN.value_or(-1)<<'\t'<<rec.stats.output<<'\n';
  }
 }
 std::cout<<"failures="<<failures<<'\n';return failures?1:0;
}
