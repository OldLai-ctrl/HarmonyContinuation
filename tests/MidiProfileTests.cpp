#include "midi/MidiWorkflow.h"
#include "midi/StandardMidiFileReader.h"
#include <fstream>
#include <algorithm>
#include <iostream>
#include <stdexcept>

using namespace harmony;
using namespace harmony::midi;
namespace {
void check(bool okay,const char* message){if(!okay)throw std::runtime_error(message);}
std::uint32_t vlq(const std::vector<std::uint8_t>& b,std::size_t& pos){std::uint32_t n{};for(int i=0;i<4;++i){check(pos<b.size(),"VLQ bounds");const auto v=b[pos++];n=(n<<7)|(v&127);if(!(v&128))return n;}throw std::runtime_error("VLQ limit");}
void minimal(const std::vector<std::uint8_t>& b){
    check(b.size()>22&&b[9]==0&&b[11]==1,"DAWClip format 0 / one track");
    std::size_t pos=22;bool ended=false;
    while(pos<b.size()){
        vlq(b,pos);check(pos<b.size(),"event bounds");const auto status=b[pos++];
        if(status==255){check(pos<b.size()&&b[pos++]==47,"only End Of Track meta");check(vlq(b,pos)==0,"empty EOT");ended=true;break;}
        check((status&240)==128||(status&240)==144,"only note events");check(pos+2<=b.size(),"note bounds");pos+=2;
    }
    check(ended&&pos==b.size(),"single legal EOT");
}
std::vector<int> metaTypes(const std::vector<std::uint8_t>& b){
    std::vector<int> result;std::size_t track=14;
    while(track<b.size()){
        check(track+8<=b.size(),"track bounds");std::uint32_t length{};for(int i=4;i<8;++i)length=(length<<8)|b[track+i];
        std::size_t pos=track+8;const auto end=pos+length;check(end<=b.size(),"track length");
        while(pos<end){vlq(b,pos);const auto status=b.at(pos++);if(status==255){result.push_back(b.at(pos++));const auto size=vlq(b,pos);pos+=size;}
            else {check((status&240)==128||(status&240)==144,"known note event");pos+=2;}}
        check(pos==end,"meta bounds");track=end;
    }
    return result;
}
void sameMusic(const std::vector<std::uint8_t>& a,const std::vector<std::uint8_t>& b){
    const auto x=readFromMemory(a),y=readFromMemory(b);check(bool(x)&&bool(y),"both profiles readable");
    check(x.file.ppq==y.file.ppq&&x.file.totalQN==y.file.totalQN&&x.file.notes.size()==y.file.notes.size(),"same length and timing division");
    for(std::size_t i=0;i<x.file.notes.size();++i){const auto& n=x.file.notes[i];const auto& m=y.file.notes[i];
        check(n.pitch==m.pitch&&n.velocity==m.velocity&&n.channel==m.channel&&n.startQN==m.startQN&&n.durationQN==m.durationQN,"same notes and timing");}
    // Reader supplies a display name when SMF has no track-name event; raw minimal()
    // proves there is no such event rather than mistaking that fallback for metadata.
    check(y.file.tempos.empty()&&y.file.meters.empty()&&y.file.tracks.size()==1,"no tempo or meter events");
}
Progression phrase(){Progression p;for(const auto* name:{"C","G7"}){ChordEvent c;c.name=name;c.durationQN=2;c.openEnded=false;c.startQN=p.size()*2.;p.push_back(c);}return p;}
}
int main(){try{
    CandidateExportContext context;check(context.imported.replace(phrase(),TimelineCoordinateMode::RelativeToSelection),"imported fixture");context.key=KeySignature{PitchClass::C,Mode::Major};
    ContinuationCandidate c;c.id="profile";c.key=*context.key;c.intent=PhraseIntent::Resolve;c.continuation.push_back({"Cmaj7",3,{1,0},ChordQuality::Major7,{}});
    enrichment::EnrichmentCandidate e;e.progression=phrase();e.progression[0].name="Cmaj7";
    int cases{};
    for(const auto mode:{ArrangementMode::VoiceLed,ArrangementMode::BlockChords})for(const auto scope:{ExportScope::FullPhrase,ExportScope::CurrentOnly,ExportScope::ContinuationOnly}){
        context.mode=mode;context.scope=scope;const auto value=candidatePayload(context,c);check(bool(value),"continuation payload");
        minimal(value.payload.dawClipBytes);sameMusic(value.payload.smfBytes,value.payload.dawClipBytes);
        const auto types=metaTypes(value.payload.smfBytes);for(int type:{81,88,89,1})check(std::find(types.begin(),types.end(),type)!=types.end(),"save annotated metadata retained");
        if(scope==ExportScope::FullPhrase)check(std::find(types.begin(),types.end(),6)!=types.end(),"save boundary retained");++cases;
    }
    context.scope=ExportScope::FullPhrase;
    for(const auto mode:{ArrangementMode::VoiceLed,ArrangementMode::BlockChords}){context.mode=mode;const auto value=candidatePayload(context,e);check(bool(value),"enrichment payload");
        minimal(value.payload.dawClipBytes);sameMusic(value.payload.smfBytes,value.payload.dawClipBytes);check(value.payload.chordCount==2,"complete transformed progression");++cases;}
    const auto value=candidatePayload(context,c);const auto path=std::filesystem::path(HC_TEST_OUTPUT_DIR)/"profile-drag";
    const auto drag=createDragFile(value.payload,path);check(bool(drag),"drag file created");std::ifstream file(drag.path,std::ios::binary);
    const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(file),{}};check(bytes==value.payload.dawClipBytes,"actual drag file uses DAWClip");minimal(bytes);
    auto missing=value.payload;missing.dawClipBytes.clear();check(!createDragFile(missing,path),"no annotated fallback on drag");
    std::cout<<"MIDI profiles "<<cases<<"/8 PASS; actual drag file minimal; annotated save preserved\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}}
