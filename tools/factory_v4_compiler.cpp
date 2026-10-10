#include "TemplateJson.h"
#include "library/ProgressionLibrary.h"
#include "library/LegacyFactoryIdResolver.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>

using namespace harmony;
namespace {
void require(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);}
std::vector<ProgressionTemplate> read(const std::filesystem::path& folder){
    std::vector<std::filesystem::path> files;
    for(const auto& e:std::filesystem::directory_iterator(folder))if(e.path().extension()==".json")files.push_back(e.path());
    std::sort(files.begin(),files.end());std::vector<ProgressionTemplate> out;
    for(const auto& f:files){std::ifstream in(f,std::ios::binary);require(bool(in),"cannot read "+f.string());
        const std::string text(std::istreambuf_iterator<char>{in},{});auto parsed=dev::parseTemplateJson(text,true);
        require(bool(parsed),f.string()+": "+parsed.error);
        for(auto& item:parsed.templates)out.push_back(std::move(item));}
    return out;
}
int root(const MatchEvent& e,Mode mode){constexpr int major[]{0,2,4,5,7,9,11},minor[]{0,2,3,5,7,8,10};
    return ((mode==Mode::Major?major:minor)[e.degree->degree-1]+e.degree->alteration+12)%12;}
bool is(const MatchEvent& e,int d,int a=0){return e.degree&&e.degree->degree==d&&e.degree->alteration==a;}
// Musical pitches, rhythm and bass must remain unchanged when semantic labels
// or secondary targets are corrected. Target/Borrowed changes are audited separately.
std::string soundingIdentity(const ProgressionTemplate& t){auto copy=t;
    for(auto& e:copy.full){e.target.reset();e.roles&=~flag(Role::Borrowed);}
    return library::canonicalMusicalFingerprint(copy);}
// A new phrase must also differ before rhythm, optional seventh/ninth colors
// and semantic annotations are considered. Bass is part of musical identity.
std::string connectionIdentity(const ProgressionTemplate& t){std::ostringstream out;out<<int(t.mode);
    for(const auto& e:t.full){auto quality=e.quality;
        if(quality==ChordQuality::Major7)quality=ChordQuality::Major;
        if(quality==ChordQuality::Minor7)quality=ChordQuality::Minor;
        out<<'|'<<root(e,t.mode)<<':'<<int(quality)<<':'<<e.bassInterval.value_or(0);}
    return out.str();}
void maintain(ProgressionTemplate& t,std::ostream& audit){
    const auto oldTech=t.techniques;const auto oldSkeleton=t.skeletonIndices;std::size_t targets{};
    const auto add=[&](const std::string& tag){if(std::find(t.techniques.begin(),t.techniques.end(),tag)==t.techniques.end())t.techniques.push_back(tag);};
    for(std::size_t i=0;i<t.full.size();++i){auto& e=t.full[i];
        if(e.target&&hasRole(e.roles,Role::SecondaryLeadingTone)){
            add("SecondaryLeadingTone");
            if(std::find(t.skeletonIndices.begin(),t.skeletonIndices.end(),i)==t.skeletonIndices.end())t.skeletonIndices.push_back(i);
        }
        if(i+1>=t.full.size())continue;const auto& next=t.full[i+1];
        // Only the ten audited #ii/#iv approaches; ordinary tonic/descending
        // diminished chords are not relabeled as secondary leading tones.
        if(t.id.starts_with("V3_SPARK_")&&e.quality==ChordQuality::Diminished7&&!e.target&&
           ((is(e,2,1)&&is(next,3))||(is(e,4,1)&&is(next,5)))){
            e.target=next.degree;e.roles=(e.roles&~flag(Role::Borrowed))|flag(Role::SecondaryLeadingTone)|flag(Role::Approach);
            e.function=HarmonicFunction::ChromaticDominant;++targets;add("SecondaryLeadingTone");
        }
        if(t.mode==Mode::Major&&is(e,4)&&
           (e.quality==ChordQuality::Minor||e.quality==ChordQuality::Minor7))add("ModalMixture");
        if(is(e,5)&&(e.quality==ChordQuality::Major||e.quality==ChordQuality::Dominant7)&&is(next,6))add("DeceptiveResolution");
        if(i+2<t.full.size()&&t.mode==Mode::Major&&is(e,4)&&e.quality==ChordQuality::Minor7&&
           is(next,7,-1)&&next.quality==ChordQuality::Dominant7&&is(t.full[i+2],1))add("Backdoor");
        if(t.mode==Mode::Major&&is(e,2,-1)&&e.quality==ChordQuality::Dominant7&&is(next,1))add("TritoneSubstitution");
    }
    if(t.cadence==CadenceType::Half)add("OpenEnding");
    // Last events can also carry a borrowed minor-IV identity.
    if(t.mode==Mode::Major&&std::any_of(t.full.begin(),t.full.end(),[](const auto& e){return is(e,4)&&
        (e.quality==ChordQuality::Minor||e.quality==ChordQuality::Minor7);}))add("ModalMixture");
    std::sort(t.skeletonIndices.begin(),t.skeletonIndices.end());prepareTemplate(t);
    if(oldTech!=t.techniques||oldSkeleton!=t.skeletonIndices||targets){
        audit<<t.id<<'\t'<<(oldTech.empty()&&!t.techniques.empty())<<'\t'<<(oldTech!=t.techniques)<<'\t'<<targets<<'\t'
             <<(oldSkeleton!=t.skeletonIndices)<<'\t';
        for(const auto& tag:t.techniques)audit<<tag<<',';audit<<'\n';
    }
}
}
int main(int argc,char** argv){try{
    require(argc==6,"usage: factory_v4_compiler legacy-dir v3-dir v4-dir output.db maintenance.tsv");
    auto entries=read(argv[1]);auto v3=read(argv[2]);entries.insert(entries.end(),v3.begin(),v3.end());
    entries=library::canonicalFactoryEntries(std::move(entries));require(entries.size()==629,"V3 baseline must have 629 canonical entries");
    const auto baseline=entries;std::ofstream audit(argv[5]);require(bool(audit),"cannot write maintenance audit");
    audit<<"id\tempty_filled\ttechniques_changed\ttargets_corrected\tskeleton_changed\ttechniques\n";
    for(auto& t:entries)maintain(t,audit);
    for(std::size_t i=0;i<baseline.size();++i)require(soundingIdentity(baseline[i])==soundingIdentity(entries[i]),"legacy music changed: "+entries[i].id);
    auto additions=read(argv[3]);require(additions.size()==28,"expected 28 A/B candidates");
    std::set<std::string> connections;
    for(const auto& t:entries)connections.insert(connectionIdentity(t));
    for(const auto& t:additions){require(t.id.starts_with("V4_")&&t.version==4,"V4 identity required");
        require(connections.insert(connectionIdentity(t)).second,"new phrase duplicates a connection family: "+t.id);
        require(t.full.size()>=4&&t.skeletonIndices.size()==t.full.size(),"V4 conservative full skeleton required: "+t.id);
        for(const auto& e:t.full){require(e.quality!=ChordQuality::Unknown,"unknown chord: "+t.id);
            if(e.intervalMask)require((e.intervalMask&1)!=0,"explicit pitch set must contain root: "+t.id);}}
    entries.insert(entries.end(),additions.begin(),additions.end());
    std::set<std::string> musical,ids,names;
    for(const auto& t:entries){require(ids.insert(t.id).second,"duplicate ID: "+t.id);
        require(names.insert(t.nameEn).second,"duplicate name: "+t.id);
        require(musical.insert(library::canonicalMusicalFingerprint(t)).second,"musical duplicate: "+t.id);}
    std::string error;require(library::compileFactory(argv[4],entries,error,4),error);
    const auto loaded=library::loadFactory(argv[4]);require(bool(loaded),loaded.error);
    require(loaded.libraryVersion==4&&loaded.storageSchemaVersion==2&&loaded.templates.size()==657,"V4 database contract");
    for(const auto& t:entries){const auto it=std::find_if(loaded.templates.begin(),loaded.templates.end(),[&](const auto& x){return x.id==t.id;});
        require(it!=loaded.templates.end()&&library::canonicalMusicalFingerprint(*it)==library::canonicalMusicalFingerprint(t),"readback changed music: "+t.id);
        require(it->techniques==t.techniques&&it->skeletonIndices==t.skeletonIndices,"readback changed metadata: "+t.id);}
    audit.close();require(bool(audit),"maintenance report write failed");
    std::cout<<"Factory V4 READY: legacy=629 additions=28 total=657 exact_duplicates=0 new_connection_duplicates=0 schema=2; pitch/rhythm/bass preservation and typed readback PASS\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
