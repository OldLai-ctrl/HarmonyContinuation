#include "wish/WishSampler.h"
#include "library/ProgressionLibrary.h"
#include "midi/MidiWorkflow.h"
#include "snapshot/RecommendationSnapshot.h"
#include "TemplateJson.h"
#include <algorithm>
#include <array>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>

int runWishAnnotationChecks();
int runWishSamplerChecks();
int main(int argc,char** argv) {
    try {
        using namespace harmony;
        const auto factory=library::loadFactory(argc>2?argv[2]:HC_WISH_FACTORY_DB_PATH);
        if(!factory)throw std::runtime_error(factory.error);
        if(argc>1 && std::string_view(argv[1])=="--bindings") {
            for(const auto& item:factory.templates)
                std::cout<<item.id<<'\t'<<wish::sourceFingerprint(item)<<'\n';
            return 0;
        }
        if(argc>1 && std::string_view(argv[1])=="--catalog") {
            for(const auto& item:factory.templates)
                std::cout<<item.id<<'\t'<<wish::sourceFingerprint(item)<<'\t'
                         <<dev::serializeTemplateJson(item)<<'\n';
            return 0;
        }
        int checks=runWishAnnotationChecks()+runWishSamplerChecks();
        const auto require=[&](bool pass,const char* text){if(!pass)throw std::runtime_error(text);++checks;};
        require(factory.libraryVersion==5 && factory.storageSchemaVersion==2 && factory.templates.size()==658,"real V5 contract");
        const auto annotations=wish::loadAnnotations(HC_WISH_ANNOTATION_PATH);
        require(bool(annotations),annotations.error.c_str());
        std::map<std::string,const wish::ModeAnnotation*> annotationsById;
        for(const auto& entry:annotations.catalog.entries)
            require(annotationsById.emplace(entry.templateId,&entry).second,
                    "each Factory ID has at most one annotation entry");
        require(annotationsById.size()==factory.templates.size(),
                "annotation catalog covers every current Factory source exactly once");

        std::size_t reviewedSources{},unreviewedSources{},unconfirmedSources{},multipleSources{};
        std::size_t reviewedMemberships{},pendingMemberships{},unconfirmedMemberships{};
        std::map<std::string,std::size_t> modeCounts,usageCounts;
        for(const auto& item:factory.templates) {
            const auto entry=annotationsById.find(item.id);
            require(entry!=annotationsById.end(),"every current Factory ID has an annotation entry");
            require(entry->second->sourceFingerprint==wish::sourceFingerprint(item),
                    "annotation source fingerprint binds to current complete Factory source");
            for(const auto& membership:entry->second->memberships) {
                std::string error;
                const bool valid=wish::validMembership(membership,error);
                require(valid,valid?"valid membership":error.c_str());
                ++modeCounts[wish::modeName(membership.mode)];
                const auto usage=[&] {
                    switch(membership.usage) {
                        case wish::EditorialUsage::High:return "high";
                        case wish::EditorialUsage::Medium:return "medium";
                        case wish::EditorialUsage::Low:return "low";
                        case wish::EditorialUsage::Unrated:return "unrated";
                    }
                    return "invalid";
                }();
                ++usageCounts[usage];
                if(membership.review==wish::ReviewStatus::Reviewed)++reviewedMemberships;
                else if(membership.review==wish::ReviewStatus::Pending)++pendingMemberships;
                else if(membership.review==wish::ReviewStatus::Unconfirmed)++unconfirmedMemberships;
            }
            if(entry->second->memberships.size()>1)++multipleSources;
            const auto resolved=wish::resolveAnnotations(item,annotations.catalog);
            require(resolved.memberships.size()==entry->second->memberships.size(),
                    "all current source memberships resolve after source validation");
            switch(resolved.state) {
                case wish::AnnotationState::Reviewed:++reviewedSources;break;
                case wish::AnnotationState::Unreviewed:++unreviewedSources;break;
                case wish::AnnotationState::Unconfirmed:++unconfirmedSources;break;
                case wish::AnnotationState::Unannotated:
                case wish::AnnotationState::SourceMismatch:
                case wish::AnnotationState::Invalid:
                    throw std::runtime_error("Factory annotation has missing, mismatched, or invalid state");
            }
        }
        const auto hasReviewed=[&](const char* id,int offset,wish::MusicalMode mode) {
            const auto found=annotationsById.find(id);
            return found!=annotationsById.end()&&std::any_of(found->second->memberships.begin(),
                found->second->memberships.end(),[&](const auto& membership) {
                    return membership.tonicOffset==offset&&membership.mode==mode&&
                        membership.review==wish::ReviewStatus::Reviewed;
                });
        };
        require(hasReviewed("COMMON_MINOR_007",0,wish::MusicalMode::Aeolian),
                "COMMON_MINOR_007 has reviewed Aeolian membership at offset zero");
        require(hasReviewed("COMMON_MINOR_007",3,wish::MusicalMode::Ionian),
                "COMMON_MINOR_007 has reviewed Ionian membership at offset three");
        require(hasReviewed("COMMON_MINOR_001",0,wish::MusicalMode::HarmonicMinor),
                "COMMON_MINOR_001 has reviewed harmonic-minor membership at offset zero");
        require(hasReviewed("V4_A08",0,wish::MusicalMode::Dorian),
                "V4_A08 has reviewed Dorian membership at offset zero");
        require(hasReviewed("V3_CYCLE_032",2,wish::MusicalMode::Dorian),
                "V3_CYCLE_032 has reviewed Dorian membership at offset two");
        require(hasReviewed("V3_CYCLE_030",5,wish::MusicalMode::Lydian),
                "V3_CYCLE_030 has reviewed Lydian membership at offset five");
        require(hasReviewed("V3_CYCLE_034",4,wish::MusicalMode::Phrygian),
                "V3_CYCLE_034 has reviewed Phrygian membership at offset four");
        require(hasReviewed("COMMON_MAJOR_020",0,wish::MusicalMode::Mixolydian),
                "COMMON_MAJOR_020 has reviewed Mixolydian membership at offset zero");
        require(!hasReviewed("COMMON_MAJOR_017",0,wish::MusicalMode::Lydian),
                "applied dominant alone does not establish Lydian membership");

        const auto weak=std::find_if(factory.templates.begin(),factory.templates.end(),
            [](const auto& item){return item.id=="ROCK_017";});
        require(weak!=factory.templates.end(),"real unconfirmed source exists");
        const std::array weakSource{*weak};
        wish::WishFilter weakFilter;weakFilter.fixedTonic=PitchClass::C;
        const auto weakFree=wish::makeFreePool(weakSource,annotations.catalog,weakFilter);
        require(bool(weakFree)&&weakFree.items.size()==1,
                "real unconfirmed source remains available without a mode constraint");
        weakFilter.modes={wish::MusicalMode::Mixolydian};
        const auto weakMode=wish::makeFreePool(weakSource,annotations.catalog,weakFilter);
        require(bool(weakMode)&&weakMode.items.empty(),
                "unconfirmed real membership never silently enters a mode pool");

        const auto free=wish::makeFreePool(factory.templates,annotations.catalog);
        require(bool(free)&&!free.items.empty()&&free.sourceMismatches==0&&free.invalidSources==0,"real free pool");
        std::cout<<"Factory sources="<<factory.templates.size()
                 <<" annotation sources reviewed="<<reviewedSources
                 <<" pending-review="<<unreviewedSources
                 <<" unconfirmed="<<unconfirmedSources
                 <<" multi-membership="<<multipleSources
                 <<" reviewed-memberships="<<reviewedMemberships
                 <<" pending-memberships="<<pendingMemberships
                 <<" unconfirmed-memberships="<<unconfirmedMemberships
                 <<" unrestricted complete music="<<free.items.size()<<'\n';
        std::cout<<"mode counts:";
        for(const auto& [mode,count]:modeCounts)std::cout<<' '<<mode<<'='<<count;
        std::cout<<"\nusage counts:";
        for(const auto& [usage,count]:usageCounts)std::cout<<' '<<usage<<'='<<count;
        std::cout<<'\n';

        constexpr std::array staticModes{
            wish::MusicalMode::Ionian,wish::MusicalMode::Dorian,wish::MusicalMode::Phrygian,
            wish::MusicalMode::Lydian,wish::MusicalMode::Mixolydian,wish::MusicalMode::Aeolian,
            wish::MusicalMode::Locrian,wish::MusicalMode::HarmonicMinor,
            wish::MusicalMode::MelodicMinorAscending};
        for(const auto mode:staticModes) {
            wish::WishFilter filter;filter.modes={mode};filter.strict=true;
            const auto pool=wish::makeFreePool(factory.templates,annotations.catalog,filter);
            require(bool(pool),"strict static-mode pool is valid");
            if(mode==wish::MusicalMode::Ionian||mode==wish::MusicalMode::Aeolian||
               mode==wish::MusicalMode::Dorian||mode==wish::MusicalMode::HarmonicMinor||
               mode==wish::MusicalMode::Lydian||mode==wish::MusicalMode::Phrygian||
               mode==wish::MusicalMode::Mixolydian)
                require(!pool.items.empty(),"known reviewed static mode has strict complete routes");
            for(const auto& item:pool.items) {
                const auto built=preview::buildSequence(item.route.imported,&item.route.candidate,120);
                require(bool(built),"strict sample builds");
                require(std::any_of(item.memberships.begin(),item.memberships.end(),[&](const auto& m){
                    return m.membership.mode==mode&&m.membership.review==wish::ReviewStatus::Reviewed&&
                        wish::strictlyCompatible(built.sequence,mode,m.tonicPitchClass);
                }),"membership center and strict full music agree");
            }
            std::cout<<wish::modeName(mode)<<" strict complete music="<<pool.items.size()<<'\n';
        }
        std::cout<<"PASS WishCoreFocused checks="<<checks<<'\n';
        return 0;
    } catch(const std::exception& e) {std::cerr<<"FAIL WishCoreFocused: "<<e.what()<<'\n';return 1;}
}
