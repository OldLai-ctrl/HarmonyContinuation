#include "library/ProgressionLibrary.h"
#include "library/LegacyFactoryIdResolver.h"
#include "library/LibraryStore.h"
#include "core/ContinuationEngine.h"
#include "midi/MidiClip.h"
#include "midi/StandardMidiFileWriter.h"
#include "midi/StandardMidiFileReader.h"
#include "preview/PreviewSynth.h"
#include "snapshot/RecommendationSnapshot.h"
#include "enrichment/ProgressionEnrichmentEngine.h"
#include "ui/WhyExplanation.h"
#include "product/ProductVersion.h"
#include "session/ProductServices.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace harmony;
namespace {
void require(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);}
const ProgressionTemplate& find(const library::LoadResult& db,const std::string& id){
    const auto it=std::find_if(db.templates.begin(),db.templates.end(),[&](const auto& t){return t.id==id;});
    require(it!=db.templates.end(),"missing "+id);return *it;
}
ChordEvent chord(const MatchEvent& e,KeySignature key,double at){
    auto c=realizeContinuation(e,key,e.durationQN.value_or(4));auto out=c.harmonicData.value_or(ChordEvent{});
    out.name=c.label;out.quality=c.quality;out.startQN=at;out.durationQN=c.durationQN;out.openEnded=false;return out;
}
void exactOutput(const preview::Sequence& sequence){
    const auto clip=midi::buildClip(sequence,midi::ArrangementMode::VoiceLed,midi::ExportScope::FullPhrase,{});
    require(bool(clip),"MIDI build");
    for(const auto& e:sequence.events){unsigned actual{};int bass=-1;
        for(const auto& n:clip.sequence.notes)if(std::abs(n.startQN-e.startQN)<1e-7){
            require(n.midiNote>=0&&n.midiNote<=127&&n.durationQN>0,"MIDI range");
            if(n.bass)bass=n.midiNote%12;else actual|=1u<<((n.midiNote-e.chord.root+120)%12);}
        require(bass==e.chord.bass,"MIDI bass lost: "+e.chord.label);
        if(e.chord.exactIntervals)require(actual==e.chord.intervals,"MIDI exact pitch set lost: "+e.chord.label);
    }
    const auto smf=midi::writeToMemory(clip.sequence);require(smf&&smf.bytes.size()>14,"SMF write");
    const auto reread=midi::readFromMemory(smf.bytes);
    require(reread&&reread.file.notes.size()==clip.sequence.notes.size(),"SMF note readback");
    for(const auto& n:clip.sequence.notes)require(std::any_of(reread.file.notes.begin(),reread.file.notes.end(),[&](const auto& actual){
        return actual.pitch==n.midiNote&&std::abs(actual.startQN-n.startQN)<1.0/reread.file.ppq&&
               std::abs(actual.durationQN-n.durationQN)<2.0/reread.file.ppq;
    }),"SMF changed pitch or timing");
    preview::PreviewSynth synth;synth.prepare(48000);require(synth.start(sequence),"preview synth start");
    float l[4096]{},r[4096]{};synth.process(l,r,4096);
    require(std::any_of(std::begin(l),std::end(l),[](float x){return std::abs(x)>1e-5f;}),"silent preview");
}
void reachable(const library::LoadResult& db,const CandidateIndex& index,const char* id,std::size_t prefix,PitchClass tonic,std::size_t limit=3){
    const auto& t=find(db,id);KeySignature key{tonic,t.mode};Progression input;double at{};
    for(std::size_t i=0;i<prefix;++i){auto c=chord(t.full[i],key,at);at+=*c.durationQN;input.push_back(c);}
    input.back().durationQN.reset();input.back().openEnded=true;
    AnalysisContext context;context.forcedKey=key;
    const auto query=makeMatchQuery(input,context);
    RecommendationWeights weights;weights.perGroup=limit;
    const auto results=recommendContinuations(query,index,{{},t.intent,{}},weights);
    const ContinuationCandidate* chosen{};std::size_t rank{};
    for(const auto& group:results.groups)for(std::size_t i=0;i<group.size();++i){const auto& c=group[i];
        if(c.primaryTemplate==id||std::find(c.supportingTemplates.begin(),c.supportingTemplates.end(),id)!=c.supportingTemplates.end()){
            chosen=&c;rank=i+1;}}
    if(!chosen){std::cerr<<"interpretations="<<query.interpretations.size()<<" matches="<<results.matches.size()<<'\n';
        for(const auto& k:query.interpretations){for(const auto& e:k.full)std::cerr<<formatMatchEvent(e)<<" fn="<<int(e.function)<<" role="<<e.roles<<' ';std::cerr<<'\n';}
        for(const auto& m:results.matches)if(m.templateId==id)std::cerr<<id<<" match="<<m.similarity<<" start="<<m.templateMatchStart<<" end="<<m.templateMatchEnd<<" suffix="<<m.continuationLength<<'\n';
        for(const auto& g:results.groups)for(const auto& c:g)std::cerr<<c.primaryTemplate<<' '<<c.rankingScore<<'\n';
        RecommendationWeights expanded;expanded.perGroup=100;
        const auto diagnostic=recommendContinuations(query,index,{{},t.intent,{}},expanded);
        for(const auto& g:diagnostic.groups)for(const auto& c:g)if(c.primaryTemplate==id||std::find(c.supportingTemplates.begin(),c.supportingTemplates.end(),id)!=c.supportingTemplates.end())
            std::cerr<<"expanded "<<c.primaryTemplate<<" score="<<c.rankingScore<<" bass="<<c.subscores.match<<" skeleton="<<c.subscores.skeleton<<'\n';}
    require(chosen!=nullptr,std::string("not visible under default recommendation limits: ")+id);
    require(chosen->continuationStart==prefix,"matched wrong occurrence: "+std::string(id));
    require(chosen->continuation.size()==t.full.size()-prefix,"incomplete path: "+std::string(id));
    const auto whyEN=ui::realizationWhy(*chosen,session::Locale::EnUS);
    const auto whyZH=ui::realizationWhy(*chosen,session::Locale::ZhCN);
    const bool rich=std::any_of(chosen->continuation.begin(),chosen->continuation.end(),[](const auto& c){return c.harmonicData.has_value();});
    if(rich)require(!whyEN.empty()&&!whyZH.empty()&&whyEN!=whyZH&&whyEN.find("whyV4.")==std::string::npos,"localized realization Why");
    for(std::size_t i=0;i<chosen->continuation.size();++i){
        const auto expected=chordPitches(chord(t.full[prefix+i],key,0));
        const auto& c=chosen->continuation[i];const auto actual=c.harmonicData?chordPitches(*c.harmonicData):chordPitches(c.label,c.quality);
        require(actual.root==expected.root&&actual.bass==expected.bass&&actual.intervals==expected.intervals,"recommended realization changed");}
    ImportedProgressionSession imported;require(imported.replace(input,TimelineCoordinateMode::RelativeToSelection),"input timeline");
    auto captured=snapshot::capture(imported,*chosen,results.matches,120,4,4,key);captured.factoryLibraryVersion=4;
    const auto restored=snapshot::deserialize(snapshot::serialize(captured),&db);
    require(restored&&restored.value.candidate.continuation.size()==chosen->continuation.size(),"snapshot V4 restore");
    for(std::size_t i=0;i<chosen->continuation.size();++i){const auto& before=chosen->continuation[i];const auto& after=restored.value.candidate.continuation[i];
        const auto a=before.harmonicData?chordPitches(*before.harmonicData):chordPitches(before.label,before.quality);
        const auto b=after.harmonicData?chordPitches(*after.harmonicData):chordPitches(after.label,after.quality);
        require(a.root==b.root&&a.bass==b.bass&&a.intervals==b.intervals,"snapshot realization changed");}
    const auto preview=preview::buildSequence(imported,chosen,120);require(bool(preview),"recommended preview");exactOutput(preview.sequence);
    std::cout<<id<<" tonic="<<static_cast<int>(tonic)<<" perGroup="<<limit<<" rank="<<rank<<" full path -> preview -> exact MIDI PASS\n";
}
}
int main(int argc,char** argv){try{
    require(product::factoryLibraryVersion==4,"development product library identity");
    const auto db=library::loadFactory(HC_FACTORY_DB_PATH);require(db&&db.templates.size()==657&&db.libraryVersion==4&&db.storageSchemaVersion==2,"V4 load");
    std::size_t added{};for(const auto& t:db.templates)if(t.id.starts_with("V4_")){
        ++added;require(t.skeletonIndices.size()==t.full.size(),"definition lost in skeleton");}
    require(added==28,"28 additions");
    // Development selection must use the bundle without switching production's
    // active pointer. This store is isolated under the build directory.
    const auto store=std::filesystem::path(HC_FACTORY_DB_PATH).parent_path()/
        ("v4-loader-case-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(store/"3");
    std::vector<ProgressionTemplate> inherited;
    for(const auto& t:db.templates)if(!t.id.starts_with("V4_"))inherited.push_back(t);
    std::string compileError;require(library::compileFactory(store/"3"/"factory.db",inherited,compileError,3),compileError);
    {std::ofstream pointer(store/"active.txt");pointer<<"3\n";}
    const auto selection=library::loadAvailableFactory(HC_FACTORY_DB_PATH,store);
    require(selection.library&&selection.library.libraryVersion==4&&selection.path==HC_FACTORY_DB_PATH,"development bundle selection");
    {std::ifstream pointer(store/"active.txt");std::string version;pointer>>version;require(version=="3","production pointer changed");}
    require(library::loadFactory(store/"3"/"factory.db").libraryVersion==3,"installed history changed");
    const auto& chain=find(db,"V4_B01");
    for(std::size_t i=0;i<3;++i)require(chain.full[i].target&&chain.full[i].target->degree==chain.full[i+1].degree->degree,"chain target");
    const auto& n6=find(db,"V4_A14");require(n6.full[3].bassInterval==4,"Neapolitan sixth missing");
    require(n6.full[3].function==HarmonicFunction::ChromaticPredominant,"N6 lost predominant function");
    const auto bassLine=[&](const char* id,std::initializer_list<int> expected){
        const auto& t=find(db,id);require(t.full.size()==expected.size(),"bass-line length");std::size_t i{};
        for(const int bass:expected)require(chordPitches(chord(t.full[i++],{PitchClass::C,Mode::Major},0)).bass==bass,"authored bass-line identity");};
    bassLine("V4_B08",{0,11,10,9,8,7,7,0});
    bassLine("V4_B09",{4,5,6,7,9,11,0});
    bassLine("V4_B10",{0,0,0,0,0,0});
    const auto& slip=find(db,"V4_B11");
    require(slip.full[0].degree->degree==1&&slip.full[1].degree->degree==2&&slip.full[1].degree->alteration==-1&&
        slip.full[2].degree->degree==1&&slip.full[0].intervalMask==slip.full[1].intervalMask&&
        slip.full[0].intervalMask==slip.full[2].intervalMask&&slip.full[0].intervalMask==((1u<<0)|(1u<<2)|(1u<<4)|(1u<<7)|(1u<<11)),"Side-slipping out-and-back identity");
    const auto& dim=find(db,"V3_SPARK_036");require(dim.full[2].target&&dim.full[2].target->degree==5&&hasRole(dim.full[2].roles,Role::SecondaryLeadingTone)&&!hasRole(dim.full[2].roles,Role::Borrowed),"secondary-leading correction");
    const auto& preserved=find(db,"COMMON_MAJOR_018");require(preserved.skeletonIndices.size()==preserved.full.size(),"leading connector still removed");
    require(std::find(find(db,"JAZZ_010").techniques.begin(),find(db,"JAZZ_010").techniques.end(),"Backdoor")!=find(db,"JAZZ_010").techniques.end(),"Backdoor tag");
    const CandidateIndex index(db.templates);
    if(argc==2){
        const std::filesystem::path output(argv[1]);std::filesystem::create_directories(output);
        std::ofstream table(output/"discoverability.tsv");
        table<<"id\tprefix_length\tstyle\tintent\tgroup_rank\tstatus\tprefix\tcontinuation\n";
        std::size_t defaults{},more{},blocked{};
        for(const auto& t:db.templates)if(t.id.starts_with("V4_")){
            const KeySignature key{PitchClass::C,t.mode};
            // Feature-bearing prefixes, rather than a generic final V-I query.
            std::size_t prefix=3;
            if(t.id=="V4_B01"||t.id=="V4_B02"||t.id=="V4_B08"||t.id=="V4_B11")prefix=2;
            if(t.id=="V4_B09"||t.id=="V4_A05"||t.id=="V4_A06"||t.id=="V4_A10"||t.id=="V4_A14")prefix=4;
            Progression input;double at{};std::string labels;
            for(std::size_t i=0;i<prefix;++i){auto c=chord(t.full[i],key,at);at+=*c.durationQN;labels+=(i?" - ":"")+c.name;input.push_back(c);}
            input.back().durationQN.reset();input.back().openEnded=true;
            AnalysisContext ctx;ctx.forcedKey=key;
            const auto style=t.styleWeights.front().first;
            RecommendationWeights weights;weights.perGroup=10; // Actual RecommendationWorker retention limit.
            const auto results=recommendContinuations(makeMatchQuery(input,ctx),index,{style,t.intent,{}},weights);
            const auto shown=session::presentationIndices(results,{});
            const ContinuationCandidate* candidate{};std::size_t rank{};bool visible{};
            for(std::size_t g=0;g<results.groups.size();++g)for(std::size_t i=0;i<results.groups[g].size();++i){
                const auto& c=results.groups[g][i];
                if(c.primaryTemplate==t.id||std::find(c.supportingTemplates.begin(),c.supportingTemplates.end(),t.id)!=c.supportingTemplates.end()){
                    candidate=&c;rank=i+1;visible=std::find(shown[g].begin(),shown[g].end(),i)!=shown[g].end();}}
            const char* status=candidate?(visible?"DEFAULT_VISIBLE":"MORE_ACCESSIBLE"):"BLOCKED";
            std::string suffix;
            if(candidate){
                require(candidate->continuationStart==prefix&&candidate->continuation.size()==t.full.size()-prefix+(t.loopable?1:0),"batch alignment: "+t.id);
                for(std::size_t i=0;i<candidate->continuation.size();++i){const auto& c=candidate->continuation[i];
                    const auto expected=chordPitches(chord(t.full[(prefix+i)%t.full.size()],key,0));
                    const auto actual=c.harmonicData?chordPitches(*c.harmonicData):chordPitches(c.label,c.quality);
                    require(expected.root==actual.root&&expected.bass==actual.bass&&expected.intervals==actual.intervals,"batch identity: "+t.id);
                    suffix+=(i?" - ":"")+c.label;}
                ImportedProgressionSession imported;require(imported.replace(input,TimelineCoordinateMode::RelativeToSelection),"batch input");
                const auto sequence=preview::buildSequence(imported,candidate,120);require(bool(sequence),"batch preview");exactOutput(sequence.sequence);
                if(t.id=="V4_B01"||t.id=="V4_B08"||t.id=="V4_B09"||t.id=="V4_B11"){
                    const auto clip=midi::buildClip(sequence.sequence,midi::ArrangementMode::VoiceLed,midi::ExportScope::FullPhrase,{});
                    const auto bytes=midi::writeToMemory(clip.sequence);std::string error;
                    require(midi::writeToFile(bytes.bytes,output/(t.id+".mid"),error),error);}
                visible?++defaults:++more;
            }else {++blocked;
                for(const auto& k:makeMatchQuery(input,ctx).interpretations){std::cout<<"  skeleton=";for(auto i:k.skeletonIndices)std::cout<<i<<',';std::cout<<'\n';for(const auto& e:k.full)std::cout<<"  "<<formatMatchEvent(e)<<" fn="<<int(e.function)<<" roles="<<e.roles<<'\n';}
                for(const auto& m:results.matches)if(m.templateId==t.id)std::cout<<"  match="<<m.similarity<<" end="<<m.templateMatchEnd<<" suffix="<<m.continuationLength<<'\n';
                weights.perGroup=100;const auto diagnostic=recommendContinuations(makeMatchQuery(input,ctx),index,{style,t.intent,{}},weights);
                if(t.id=="V4_B02")for(const auto& g:diagnostic.groups)for(const auto& c:g){std::cout<<"  "<<c.primaryTemplate<<" score="<<c.rankingScore<<" : ";for(const auto& e:c.continuation)std::cout<<e.label<<' ';std::cout<<'\n';}
                for(const auto& g:diagnostic.groups)for(std::size_t i=0;i<g.size();++i)if(g[i].primaryTemplate==t.id||std::find(g[i].supportingTemplates.begin(),g[i].supportingTemplates.end(),t.id)!=g[i].supportingTemplates.end())std::cout<<"  diagnostic rank="<<i+1<<" score="<<g[i].rankingScore<<'\n';
            }
            table<<t.id<<'\t'<<prefix<<'\t'<<static_cast<unsigned>(style)<<'\t'<<intentName(t.intent)<<'\t'<<rank<<'\t'<<status<<'\t'<<labels<<'\t'<<suffix<<'\n';
            std::cout<<t.id<<' '<<status<<" rank="<<rank<<'\n';
        }
        std::cout<<"DEFAULT_VISIBLE="<<defaults<<" MORE_ACCESSIBLE="<<more<<" BLOCKED="<<blocked<<'\n';
        return blocked?2:0;
    }
    reachable(db,index,"V4_B01",2,PitchClass::C);
    reachable(db,index,"V4_B02",3,PitchClass::C);
    reachable(db,index,"V4_B08",2,PitchClass::C);
    reachable(db,index,"V4_B08",2,PitchClass::D);
    // The UI worker retains ten choices for More candidates, normally shows three.
    reachable(db,index,"V4_B09",4,PitchClass::C,10);
    reachable(db,index,"V4_B10",3,PitchClass::C);
    reachable(db,index,"V4_B11",2,PitchClass::C);
    reachable(db,index,"V4_B04",3,PitchClass::C);
    // Negative bass and pitch-set discrimination use an otherwise identical pair.
    auto exact=find(db,"V4_B08"),wrong=exact;wrong.id="wrong-bass";wrong.full[1].bassInterval=7;
    Progression p{chord(exact.full[0],{PitchClass::C,Mode::Major},0),chord(exact.full[1],{PitchClass::C,Mode::Major},2)};
    AnalysisContext context;context.forcedKey=KeySignature{PitchClass::C,Mode::Major};
    auto q=makeMatchQuery(p,context);const auto matches=matchProgression(q,CandidateIndex({wrong,exact}));
    require(matches.size()==2&&matches[0].templateId==exact.id&&matches[0].similarity>matches[1].similarity,"bass not discriminated");
    const auto rec=recommendContinuations(q,CandidateIndex({wrong}));
    require(std::all_of(rec.groups.begin(),rec.groups.end(),[](const auto& g){return g.empty();}),"contradictory bass accepted");
    auto colors=find(db,"V4_B11"),plain=colors;plain.id="wrong-color";plain.full[1].intervalMask&=~(1u<<2);
    p={chord(colors.full[0],{PitchClass::C,Mode::Major},0),chord(colors.full[1],{PitchClass::C,Mode::Major},3)};
    const auto cm=matchProgression(makeMatchQuery(p,context),CandidateIndex({plain,colors}));
    require(cm.size()==2&&cm[0].templateId==colors.id&&cm[0].similarity>cm[1].similarity,"exact colors not discriminated");
    // An explicit phrase can be enriched elsewhere, but authored bass/mask events
    // and the adjacency between them cannot be destructively rewritten.
    for(const auto* id:{"V4_B08","V4_B11"}){
        const auto& t=find(db,id);Progression phrase;double at{};
        for(const auto& e:t.full){auto c=chord(e,{PitchClass::C,Mode::Major},at);at+=*c.durationQN;phrase.push_back(std::move(c));}
        const auto enriched=enrichment::enrichProgression(phrase,analyzeHarmony(phrase,context));
        require(enriched.error.empty(),"exact phrase enrichment error");
        for(const auto& g:enriched.groups)for(const auto& c:g)for(const auto& e:phrase){
            if(!e.bass&&!e.extensions.mask&&!e.extensions.pitches&&e.name.find('/')==std::string::npos)continue;
            const auto it=std::find_if(c.progression.begin(),c.progression.end(),[&](const auto& x){return std::abs(x.startQN-e.startQN)<1e-7;});
            require(it!=c.progression.end(),"enrichment removed exact node");const auto a=chordPitches(e),b=chordPitches(*it);
            require(a.root==b.root&&a.bass==b.bass&&a.intervals==b.intervals&&it->durationQN==e.durationQN,"enrichment rewrote exact node");
        }
    }
    std::cout<<"V4 focused contracts PASS: 28 definitions, targets, metadata, skeleton, bass, colors, transpose, recommendation, preview and MIDI\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
