#include "ProgressionJson.h"
#include "generation/PhraseGenerator.h"
#include "core/ChordPitchSet.h"
#include "core/HarmonyAnalysis.h"
#include "core/ImportedProgression.h"
#include "midi/MidiWorkflow.h"
#include "preview/OfflinePreviewRenderer.h"
#include "preview/PreviewSequence.h"
#include <filesystem>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>

namespace fs=std::filesystem;
using namespace harmony;
namespace {
std::string esc(const std::string& s){std::string o="\"";for(unsigned char c:s){switch(c){case '"':o+="\\\"";break;case '\\':o+="\\\\";break;case '\n':o+="\\n";break;case '\r':o+="\\r";break;case '\t':o+="\\t";break;default:if(c<32){char b[7];std::snprintf(b,sizeof b,"\\u%04x",c);o+=b;}else o+=char(c);}}return o+'"';}
std::string optKey(const std::optional<KeySignature>& k){return k?esc(formatKey(*k)):"null";}
std::string number(double value){std::ostringstream o;o<<std::setprecision(17)<<value;return o.str();}
generation::DevelopmentIntent intent(const std::string& s){using I=generation::DevelopmentIntent;if(s=="natural")return I::Natural;if(s=="open")return I::Open;if(s=="tension")return I::Tension;if(s=="color")return I::Color;return I::BassLine;}
std::string safe(std::string s){for(char& c:s)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'))c='_';return s;}
bool writeBytes(const fs::path& p,const std::vector<std::uint8_t>& b){std::ofstream f(p,std::ios::binary);if(!f)return false;f.write(reinterpret_cast<const char*>(b.data()),static_cast<std::streamsize>(b.size()));return bool(f);}
std::string planJson(const generation::PhrasePlan& p){std::ostringstream out;
 out<<"{\"id\":"<<esc(p.id)<<",\"priorId\":"<<esc(p.priorId)<<",\"variation\":"<<esc(generation::name(p.variation))<<",\"outsideBudget\":"<<p.outsideBudget<<",\"maximumBassLeap\":"<<p.maximumBassLeap<<",\"explanation\":"<<esc(p.explanation)<<",\"slots\":[";
 for(size_t i=0;i<p.slots.size();++i){if(i)out<<',';const auto& s=p.slots[i];out<<"{\"durationQN\":"<<s.durationQN<<",\"rootDegree\":"<<s.rootDegree<<",\"anchorDegree\":"<<s.anchorDegree<<",\"appliedDominant\":"<<(s.appliedDominant?"true":"false")<<",\"bassDegree\":"<<s.bassDegree<<",\"bassMidi\":"<<s.bassMidi<<",\"role\":"<<esc(s.role)<<",\"evidence\":"<<esc(s.evidence)<<'}';}return out.str()+"]}";}
}
int main(int argc,char** argv){
 if(argc<3||argc>6){std::cerr<<"usage: phrase_generate input.json output-dir [intent-name] [rank 1|2]\n       phrase_generate input.json output-dir --style prior.tsv conventional|rich|bold\n";return 2;}
 try{
  std::ifstream in(argv[1],std::ios::binary);if(!in){std::cerr<<"cannot open input\n";return 1;}std::string source((std::istreambuf_iterator<char>(in)),{});
  auto parsed=dev::parseProgressionJson(source);if(!parsed){std::cerr<<parsed.error<<'\n';return 1;}
  generation::GenerationRequest req;req.input=parsed.events; // Preserve authored duration and OPEN exactly.
  req.context.timeSigNumerator=4;req.context.timeSigDenominator=4;
  int selectedRank=1;
  if(argc==6&&std::string(argv[3])=="--style"){
   req.stylePrior=generation::loadStylePrior(argv[4]);req.resultsPerIntent=1;const std::string level=argv[5];
   if(level=="conventional")req.variation=generation::VariationLevel::Conventional;else if(level=="rich")req.variation=generation::VariationLevel::Rich;else if(level=="bold")req.variation=generation::VariationLevel::Bold;else throw std::runtime_error("variation must be conventional, rich, or bold");
  }else{
   if(argc>5)throw std::runtime_error("invalid command arguments");
   if(argc==5){const std::string rank=argv[4];if(rank!="1"&&rank!="2"){std::cerr<<"rank must be 1 or 2\n";return 2;}selectedRank=rank=="2"?2:1;}
   if(argc>=4){const std::string n=argv[3];if(n!="natural"&&n!="open"&&n!="tension"&&n!="color"&&n!="bass-line"){std::cerr<<"intent must be natural, open, tension, color, or bass-line\n";return 2;}req.intents={intent(n)};}
  }
  const auto result=generation::generate(req);if(!result){std::cerr<<result.error<<'\n';return 1;}
  if(result.phrases.empty()){std::cerr<<"no complete qualified phrase within the stated budget\n";return 3;}
  std::map<generation::DevelopmentIntent,int> available;bool exportable=false;for(const auto& p:result.phrases)if(++available[p.intent]>=selectedRank)exportable=true;if(!exportable){std::cerr<<"requested candidate rank unavailable\n";return 3;}
  fs::path out=argv[2];fs::create_directories(out);std::ofstream json(out/"generation.json",std::ios::binary);if(!json){std::cerr<<"cannot create generation.json\n";return 1;}json<<std::setprecision(9);
  json<<"{\"source\":\"generated-no-factory\",\"generatorVersion\":"<<esc(req.stylePrior?"g131-corpus-plan-v1":generation::generatorVersion);
  if(req.stylePrior)json<<",\"stylePrior\":{\"id\":"<<esc(req.stylePrior->id)<<",\"source\":"<<esc(req.stylePrior->source)<<",\"license\":"<<esc(req.stylePrior->license)<<",\"scope\":"<<esc(req.stylePrior->scope)<<",\"variation\":"<<esc(generation::name(req.variation))<<'}';
  if(req.stylePrior)json<<",\"planningBudget\":{\"fixedMaximum\":4096,\"skeletonExpanded\":"<<result.skeletonExpanded<<",\"bassExpanded\":"<<result.bassExpanded<<",\"totalExamined\":"<<result.skeletonExpanded+result.bassExpanded+result.expanded<<",\"totalMaximum\":"<<4096+req.maximumExpansions<<'}';
  json<<",\"meter\":{\"numerator\":4,\"denominator\":4,\"source\":\"CLI default\"},\"contextInterpretations\":[";
  for(size_t i=0;i<result.contextInterpretations.size();++i){if(i)json<<',';json<<esc(formatKey(result.contextInterpretations[i]));}
  json<<"],\"input\":[";
  for(size_t i=0;i<req.input.size();++i){const auto& e=req.input[i];auto c=chordPitches(e);if(i)json<<',';json<<"{\"name\":"<<esc(e.name)<<",\"root\":"<<c.root<<",\"bass\":"<<c.bass<<",\"intervals\":"<<c.intervals<<",\"colorMask\":"<<c.colorMask<<",\"startQN\":"<<number(e.startQN)<<",\"durationQN\":"<<(e.durationQN?number(*e.durationQN):"null")<<",\"open\":"<<(e.openEnded?"true":"false")<<'}';}
  json<<"],\"budget\":{\"beamWidth\":"<<req.beamWidth<<",\"successorLimit\":"<<req.successorLimit<<",\"maximumExpansions\":"<<req.maximumExpansions<<",\"resultsPerIntent\":"<<req.resultsPerIntent<<",\"expanded\":"<<result.expanded<<",\"completeExamined\":"<<result.completeExamined<<",\"constraintRejected\":"<<result.constraintRejected<<",\"structureRejected\":"<<result.structureRejected<<",\"exhausted\":"<<(result.budgetExhausted?"true":"false")<<"},\"elapsedMs\":"<<result.elapsedMs<<",\"phrases\":[";
  std::map<std::string,int> exports;std::vector<const generation::GeneratedPhrase*> selected;size_t written{};bool first=true;
  for(const auto& p:result.phrases){if(!first)json<<',';first=false;
   json<<"{\"id\":"<<esc(p.id)<<",\"intent\":"<<esc(generation::name(p.intent))<<",\"inputIdentity\":"<<esc(p.inputIdentity)<<",\"contextInterpretations\":[";for(size_t i=0;i<p.contextInterpretations.size();++i){if(i)json<<',';json<<esc(formatKey(p.contextInterpretations[i]));}json<<"],\"originCenter\":"<<esc(formatKey(p.originCenter))<<",\"targetCenter\":"<<optKey(p.targetCenter)<<",\"centerEvidence\":"<<esc(p.centerEvidence)<<",\"structure\":"<<esc(p.structureExplanation)<<",\"ending\":"<<esc(phraseEndingName(p.ending))<<",\"endingExplanation\":"<<esc(p.endingExplanation)<<",\"seamQN\":"<<p.seamQN<<",\"durationQN\":"<<p.durationQN<<",\"inputOpenHoldQN\":"<<p.inputOpenHoldQN<<",\"hardConstraintsPassed\":"<<(p.hardConstraintsPassed?"true":"false")<<",\"complete\":"<<(p.complete?"true":"false")<<",\"chords\":[";
   for(size_t i=0;i<p.phrase.size();++i){const auto& e=p.phrase[i];auto c=chordPitches(e);if(i)json<<',';json<<"{\"name\":"<<esc(e.name)<<",\"root\":"<<c.root<<",\"bass\":"<<c.bass<<",\"intervals\":"<<c.intervals<<",\"colorMask\":"<<c.colorMask<<",\"durationQN\":"<<(e.durationQN?number(*e.durationQN):"null")<<",\"startQN\":"<<number(e.startQN)<<'}';}
   json<<"],\"bassMidi\":[";for(size_t i=0;i<p.bassMidi.size();++i){if(i)json<<',';json<<p.bassMidi[i];}json<<"],\"relations\":[";for(size_t i=0;i<p.path.size();++i){const auto& r=p.path[i];if(i)json<<',';json<<"{\"kind\":"<<esc(generation::name(r.kind))<<",\"mechanism\":"<<esc(r.mechanism)<<",\"explanation\":"<<esc(r.explanation)<<",\"expectedRoot\":"<<(r.expectedRoot?std::to_string(*r.expectedRoot):"null")<<",\"proposedCenter\":"<<optKey(r.proposedCenter)<<",\"commonPitchClasses\":"<<r.commonPitchClasses<<",\"bassClassStep\":"<<r.bassClassStep<<",\"connection\":"<<r.connection<<'}';}
   const auto& v=p.evaluation;json<<"],\"evaluation\":{\"connection\":"<<v.connection<<",\"structure\":"<<v.structure<<",\"development\":"<<v.development<<",\"novelty\":"<<v.novelty<<",\"tension\":"<<v.tension<<",\"colorTensionTrend\":"<<(v.colorTensionTrend?number(*v.colorTensionTrend):"null")<<",\"bassContinuity\":"<<v.bassContinuity<<",\"recentSimilarity\":"<<v.recentSimilarity<<",\"historySimilarity\":"<<v.historySimilarity<<",\"uniqueHarmonies\":"<<v.uniqueHarmonies<<",\"outsideChords\":"<<v.outsideChords<<",\"directedBassSteps\":"<<v.directedBassSteps<<"}";
   if(p.plan)json<<",\"plan\":"<<planJson(*p.plan);
   auto& n=exports[generation::name(p.intent)];++n;if(n==selectedRank&&written<5){auto c=generation::previewCandidate(p);auto built=generation::buildPerformance(req.input,p,120);if(!built){std::cerr<<"preview build failed for "<<p.id<<": "<<built.error<<'\n';return 1;}auto audio=preview::renderOffline(built.sequence,48000);const auto stem=safe(p.id.empty()?generation::name(p.intent):p.id);std::string error;if(!preview::writeWav16(audio,out/(stem+".wav"),error)){std::cerr<<"WAV write failed: "<<error<<'\n';return 1;}const auto clip=midi::buildClip(built.sequence,midi::ArrangementMode::VoiceLed,midi::ExportScope::FullPhrase,{4,4},c.key,c.intent);if(!clip){std::cerr<<clip.error;return 1;}const auto payload=midi::makePayload(clip.sequence,"G13");midi::ExportConfig dragConfig;dragConfig.profile=midi::ExportProfile::DAWClip;const auto drag=midi::writeToMemory(clip.sequence,dragConfig);if(!drag){std::cerr<<drag.error;return 1;}if(!writeBytes(out/(stem+"_Annotated.mid"),payload.smfBytes)||!writeBytes(out/(stem+"_DAWClip.mid"),drag.bytes)){std::cerr<<"MIDI write failed\n";return 1;}json<<",\"wav\":"<<esc(stem+".wav")<<",\"midiAnnotated\":"<<esc(stem+"_Annotated.mid")<<",\"midiDAWClip\":"<<esc(stem+"_DAWClip.mid");++written;selected.push_back(&p);}
   json<<",\"rankWithinIntent\":"<<n<<'}';
  }
  if(!written){std::cerr<<"requested candidate rank unavailable\n";return 3;}
  json<<"]}\n";json.close();if(!json){std::cerr<<"generation.json write failed\n";return 1;}
  std::ofstream idx(out/"LISTENING_INDEX.md",std::ios::binary);if(!idx){std::cerr<<"cannot create LISTENING_INDEX.md\n";return 1;}idx<<"# Listening index\n\nInput JSON currently supplies a chord array; the CLI uses its default 4/4 meter. Seam seconds are measured from the first input chord at 120 BPM. Generated phrases are algorithmic drafts; musical quality awaits user listening review.\n\n| ID | Intent | Complete E chords / QN | Seam (s) | Center interpretation | WAV | Annotated MIDI | DAWClip MIDI |\n|---|---|---|---:|---|---|---|---|\n";
  for(const auto* phrase:selected){const auto& p=*phrase;const auto stem=safe(p.id.empty()?generation::name(p.intent):p.id);std::string chords;for(const auto& e:p.phrase){if(!chords.empty())chords+="; ";chords+=e.name+" "+(e.durationQN?std::to_string(*e.durationQN):"OPEN")+" QN";}idx<<"| "<<p.id<<" | "<<generation::name(p.intent)<<" | "<<chords<<" | "<<(p.seamQN-req.input.front().startQN)*60.0/120.0<<" | "<<p.centerEvidence<<" | [WAV]("<<stem<<".wav) | [MIDI]("<<stem<<"_Annotated.mid) | [MIDI]("<<stem<<"_DAWClip.mid) |\n";}
  idx.close();if(!idx){std::cerr<<"LISTENING_INDEX.md write failed\n";return 1;}return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
