#include "wish/WishSampler.h"
#include "library/ProgressionLibrary.h"
#include "midi/MidiWorkflow.h"
#include "snapshot/RecommendationSnapshot.h"
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <stdexcept>

int runWishAnnotationChecks();
int runWishSamplerChecks();
int main(int argc,char** argv) {
    try {
        using namespace harmony;
        const auto factory=library::loadFactory(argc>2?argv[2]:HC_WISH_FACTORY_DB_PATH);
        if(!factory)throw std::runtime_error(factory.error);
        if(argc>1 && std::string_view(argv[1])=="--bindings") {
            for(const auto* id:{"COMMON_MINOR_007","COMMON_MINOR_001","V4_A08"}) {
                const auto found=std::find_if(factory.templates.begin(),factory.templates.end(),[&](const auto& t){return t.id==id;});
                if(found==factory.templates.end())throw std::runtime_error(std::string("missing real sample: ")+id);
                std::cout<<id<<'\t'<<wish::sourceFingerprint(*found)<<'\n';
            }
            return 0;
        }
        int checks=runWishAnnotationChecks()+runWishSamplerChecks();
        const auto require=[&](bool pass,const char* text){if(!pass)throw std::runtime_error(text);++checks;};
        require(factory.libraryVersion==5 && factory.storageSchemaVersion==2 && factory.templates.size()==658,"real V5 contract");
        const auto annotations=wish::loadAnnotations(HC_WISH_ANNOTATION_PATH);
        require(bool(annotations),annotations.error.c_str());
        require(annotations.catalog.entries.size()==3,"three manually reviewed source samples");
        std::size_t reviewed{},multiple{};
        for(const auto& item:annotations.catalog.entries) {
            const auto source=std::find_if(factory.templates.begin(),factory.templates.end(),[&](const auto& t){return t.id==item.templateId;});
            require(source!=factory.templates.end(),"sample Factory ID is current");
            const auto resolved=wish::resolveAnnotations(*source,annotations.catalog);
            require(resolved.state==wish::AnnotationState::Reviewed,"sample source fingerprint and review bind");
            for(const auto& m:resolved.memberships)if(m.review==wish::ReviewStatus::Reviewed)++reviewed;
            if(resolved.memberships.size()>1)++multiple;
        }
        require(reviewed==4 && multiple==1,"four reviewed associations include one multi-mode source");
        const auto free=wish::makeFreePool(factory.templates,annotations.catalog);
        require(bool(free)&&!free.items.empty()&&free.sourceMismatches==0&&free.invalidSources==0,"real free pool");
        std::cout<<"Factory="<<factory.templates.size()<<" complete unique performances="<<free.items.size()
                 <<" unannotated="<<free.unannotatedSources<<" reviewed associations="<<reviewed<<'\n';
        for(const auto mode:{wish::MusicalMode::Ionian,wish::MusicalMode::Aeolian,wish::MusicalMode::Dorian,wish::MusicalMode::HarmonicMinor}) {
            wish::WishFilter filter;filter.modes={mode};filter.strict=true;
            const auto pool=wish::makeFreePool(factory.templates,annotations.catalog,filter);
            require(bool(pool)&&!pool.items.empty(),"reviewed static mode has strict complete routes");
            for(const auto& item:pool.items) {
                const auto built=preview::buildSequence(item.route.imported,&item.route.candidate,120);
                require(bool(built),"strict sample builds");
                require(std::any_of(item.memberships.begin(),item.memberships.end(),[&](const auto& m){
                    return m.membership.mode==mode&&wish::strictlyCompatible(built.sequence,mode,m.tonicPitchClass);
                }),"membership center and strict full music agree");
            }
            std::cout<<wish::modeName(mode)<<" strict performances="<<pool.items.size()<<'\n';
        }
        std::cout<<"PASS WishCoreFocused checks="<<checks<<'\n';
        return 0;
    } catch(const std::exception& e) {std::cerr<<"FAIL WishCoreFocused: "<<e.what()<<'\n';return 1;}
}
