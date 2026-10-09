#include "product/ProductVersion.h"
#include "TemplateJson.h"
#include "library/ProgressionLibrary.h"
#include "library/LegacyFactoryIdResolver.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <unordered_map>
#include <algorithm>
#include <array>

namespace {
using namespace harmony;
bool metadataMissing(const ProgressionTemplate& t) {
    return t.id.empty()||t.nameZh.empty()||t.nameEn.empty()||t.aliases.empty()||
           t.builtInTags.empty()||t.styleWeights.empty()||t.styles==0;
}
void validateMetadata(const std::vector<ProgressionTemplate>& source,
                      const std::vector<ProgressionTemplate>& production) {
    const auto contains=[](const auto& values,const auto& value) {
        return std::find(values.begin(),values.end(),value)!=values.end();
    };
    for(const auto& old:source) {
        const auto reference=library::resolveFactoryId(old.id,production);
        const auto alias=std::find_if(production.begin(),production.end(),
            [&](const auto& t){return contains(t.aliases,old.id);});
        const auto* entry=reference?reference.entry:alias!=production.end()?&*alias:nullptr;
        if(!entry||library::canonicalMusicalFingerprint(*entry)!=library::canonicalMusicalFingerprint(old))
            throw std::runtime_error("missing/changed musical content: "+old.id);
        const auto intents=entry->secondaryIntents|(1u<<static_cast<unsigned>(entry->intent));
        bool ok=(entry->styles&old.styles)==old.styles&&
                (intents&(old.secondaryIntents|(1u<<static_cast<unsigned>(old.intent))))==
                (old.secondaryIntents|(1u<<static_cast<unsigned>(old.intent)))&&
                (!old.loopable||entry->loopable);
        for(const auto& [style,weight]:old.styleWeights) {
            const auto it=std::find_if(entry->styleWeights.begin(),entry->styleWeights.end(),
                                     [&](const auto& value){return value.first==style;});
            ok=ok&&it!=entry->styleWeights.end()&&it->second>=weight;
        }
        for(const auto& value:old.tags)ok=ok&&contains(entry->tags,value);
        for(const auto& value:old.builtInTags)ok=ok&&contains(entry->builtInTags,value);
        for(const auto& value:old.techniques)ok=ok&&contains(entry->techniques,value);
        for(const auto& value:old.aliases)ok=ok&&contains(entry->aliases,value);
        if(entry->id!=old.id)for(const auto& name:{old.name,old.nameZh,old.nameEn})
            ok=ok&&(name.empty()||contains(entry->aliases,name));
        if(!ok)throw std::runtime_error("metadata merge incomplete: "+old.id);
    }
}
void writeSummary(const std::filesystem::path& output,const std::filesystem::path& sourceDirectory,
                  const std::vector<ProgressionTemplate>& entries) {
    std::array<std::size_t,6> styles{};std::array<std::size_t,5> intents{};
    std::array<std::size_t,4> lengths{};std::size_t major{},added{},missing{},questionable{};
    for(const auto& t:entries) {
        major+=t.mode==Mode::Major;added+=t.id.starts_with("V3_");missing+=metadataMissing(t);
        for(std::size_t i=0;i<styles.size();++i)styles[i]+=(t.styles&(1u<<i))!=0;
        const auto mask=t.secondaryIntents|(1u<<static_cast<unsigned>(t.intent));
        for(std::size_t i=1;i<intents.size();++i)intents[i]+=(mask&(1u<<i))!=0;
        const auto n=t.full.size();++lengths[n==2?0:n<=4?1:n<=8?2:3];
    }
    std::ifstream review(sourceDirectory.parent_path()/"review/factory-v3-questionable.tsv");
    if(!review)throw std::runtime_error("cannot read QUESTIONABLE list");
    for(std::string line;std::getline(review,line);)questionable+=line.find("\tQUESTIONABLE\t")!=line.npos;
    std::ofstream report(output);if(!report)throw std::runtime_error("cannot write catalogue summary");
    report<<"# Factory Library V3 Summary\n\n| Item | Count |\n| --- | ---: |\n"
          <<"| Total | "<<entries.size()<<" |\n| New production entries | "<<added
          <<" |\n| Major | "<<major<<" |\n| Minor | "<<entries.size()-major<<" |\n";
    constexpr const char* names[]{"Pop","Rock","R&B","Jazz","City Pop","Functional"};
    for(std::size_t i=0;i<styles.size();++i)report<<"| "<<names[i]<<" | "<<styles[i]<<" |\n";
    constexpr const char* purposes[]{"Neutral","Develop","Resolve","Loop","Color"};
    for(std::size_t i=1;i<intents.size();++i)report<<"| "<<purposes[i]<<" | "<<intents[i]<<" |\n";
    constexpr const char* ranges[]{"2 chords","3-4 chords","5-8 chords","9+ chords"};
    for(std::size_t i=0;i<lengths.size();++i)report<<"| "<<ranges[i]<<" | "<<lengths[i]<<" |\n";
    report<<"| Exact Musical Duplicates | 0 |\n| Missing Metadata | "<<missing
          <<" |\n| QUESTIONABLE (excluded) | "<<questionable<<" |\n\n"
          <<"Style/Intent memberships overlap; Intent includes preserved secondary intents.\n"
          <<"Identity uses mode, meter, degrees, quality, secondary target, borrowed role, bass, "
          <<"effective extension masks and exact relative QN durations. Names and editorial metadata are excluded.\n"
          <<"All six legacy metadata merges were validated against the source after DB readback.\n";
    if(!report)throw std::runtime_error("cannot finish catalogue summary");
}
}

int main(int argc, char** argv) {
    if (harmony::product::printVersionIfRequested(argc, argv)) return 0;
    if (argc < 3 || argc > 6) { std::cerr << "usage: db_compiler data/factory output/factory.db [library-version [v3-additions [summary.md]]]\n"; return 2; }
    std::vector<harmony::ProgressionTemplate> templates;
    std::set<std::string> ids, namesZh, namesEn;
    std::unordered_map<std::string, int> skeletons;
    try {
        const int version=argc>=4?std::stoi(argv[3]):2;
        if(argc>=5&&version!=3)throw std::runtime_error("V3 additions require Library 3");
        for(int source=0;source<(argc>=5?2:1);++source) {
        std::vector<std::filesystem::path> files;
        for(const auto& entry:std::filesystem::directory_iterator(source==0?argv[1]:argv[4]))
            if(entry.path().extension()==".json")files.push_back(entry.path());
        std::sort(files.begin(),files.end());
        for (const auto& path : files) {
            std::ifstream file(path, std::ios::binary);
            if (!file) throw std::runtime_error("cannot read " + path.string());
            const std::string text(std::istreambuf_iterator<char>{file}, {});
            auto parsed = harmony::dev::parseTemplateJson(text, true);
            if (!parsed) throw std::runtime_error(path.string() + ": " + parsed.error);
            for (auto& item : parsed.templates) {
                if (!ids.insert(item.id).second) throw std::runtime_error("duplicate id: " + item.id);
                if (item.sourceType != "factory") throw std::runtime_error("factory sourceType required: " + item.id);
                if (item.nameZh.empty() || item.nameEn.empty() || item.aliases.empty() ||
                    item.builtInTags.empty())
                    throw std::runtime_error("factory display metadata missing: " + item.id);
                if (item.intent == harmony::PhraseIntent::Loop && !item.loopable)
                    throw std::runtime_error("loop intent requires loopable: " + item.id);
                if (item.loopable && item.cadence == harmony::CadenceType::Authentic)
                    std::cerr << "warning: authentic and loopable: " << item.id << '\n';
                std::string skeletonKey = item.mode == harmony::Mode::Major ? "M" : "m";
                for (const auto index : item.skeletonIndices)
                    skeletonKey += '|' + harmony::formatMatchEvent(item.full[index]);
                ++skeletons[skeletonKey];
                templates.push_back(std::move(item));
            }
        }
        }
        if (templates.size() < 161) throw std::runtime_error("factory corpus has fewer than 161 entries");
        std::size_t near{};
        for (const auto& [key, count] : skeletons) if (count > 1) near += static_cast<std::size_t>(count - 1);
        std::string error;
        const auto source=templates;
        if(version>=3)templates=harmony::library::canonicalFactoryEntries(std::move(templates));
        if(argc>=5&&(templates.size()<500||templates.size()>800))
            throw std::runtime_error("expanded V3 catalogue must contain 500..800 canonical entries");
        if (!harmony::library::compileFactory(argv[2], templates, error,version)) throw std::runtime_error(error);
        auto loaded = harmony::library::loadFactory(argv[2]);
        if (!loaded || loaded.templates.size() != templates.size()) throw std::runtime_error("database roundtrip failed: " + loaded.error);
        std::set<std::string> musical;
        for(const auto& item:loaded.templates) {
            if(metadataMissing(item))throw std::runtime_error("missing metadata: "+item.id);
            if(!namesZh.insert(item.nameZh).second||!namesEn.insert(item.nameEn).second)
                throw std::runtime_error("duplicate production display name: "+item.id);
            if(version>=3&&!musical.insert(harmony::library::canonicalMusicalFingerprint(item)).second)
                throw std::runtime_error("exact musical duplicate: "+item.id);
        }
        if(version>=3)validateMetadata(source,loaded.templates);
        if(argc==6)writeSummary(argv[5],std::filesystem::path(argv[1]),loaded.templates);
        std::cout << "factory.db READY templates=" << templates.size() << " near_skeleton_variants=" << near << '\n';
        if(version>=3)std::cout<<"Content PASS: Exact Musical Duplicate=0, Missing Metadata=0, legacy metadata merge=6/6\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << "factory lint FAIL: " << e.what() << '\n'; return 1; }
}
