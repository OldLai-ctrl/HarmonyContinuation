#include "library/LibraryStore.h"
#include "library/LegacyFactoryIdResolver.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>

int main() {
    using namespace harmony::library;
    const auto root=std::filesystem::temp_directory_path()/("hc-library-store-"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int checks{};
    const auto check=[&](bool ok,const char* label) { ++checks; if(!ok) throw std::runtime_error(label); };
    try {
        std::filesystem::create_directories(root);
        const auto store=root/"store";
        auto initial=loadFactory(HC_FACTORY_DB_PATH);
        check(initial && initial.templates.size()==161 && initial.libraryVersion==2,"bundled library");
        check(loadAvailableFactory(HC_FACTORY_DB_PATH,store).library.templates.size()==161,"portable fallback");
        std::string error;
        check(installFactoryPackage(HC_FACTORY_DB_PATH,store,error),"first install");
        check(installFactoryPackage(HC_FACTORY_DB_PATH,store,error),"same-version repair");
        auto next=canonicalFactoryEntries(initial.templates); next.front().name="Updated name";
        check(compileFactory(root/"update.db",next,error,3),"independent library version");
        UserLibrary user(root/"user.db"); auto item=initial.templates.front();item.id="user-kept";item.sourceType="user";
        check(user.addProgression(item,error),"personal collection saved");
        check(installFactoryPackage(root/"update.db",store,error),"update install");
        auto active=loadAvailableFactory(HC_FACTORY_DB_PATH,store);
        check(active.library.libraryVersion==3 && active.library.templates.front().name=="Updated name","active new library");
        check(std::filesystem::exists(store/"2"/"factory.db") && user.loadAll().templates.size()==1,"old sets and user retained");
        check(installFactoryPackage(HC_FACTORY_DB_PATH,store,error) && loadAvailableFactory(HC_FACTORY_DB_PATH,store).library.libraryVersion==3,"no accidental downgrade");
        check(compileFactory(root/"same-version.db",canonicalFactoryEntries(initial.templates),error,3),"conflicting package fixture");
        error.clear();const bool conflictingPackageRejected=!installFactoryPackage(root/"same-version.db",store,error);
        check(conflictingPackageRejected,"same version cannot replace content");
        check(error.find("library version already exists with different content")!=std::string::npos,
            "same-version rejection is caused by content conflict");
        error.clear();const bool lockReleased=installFactoryPackage(root/"update.db",store,error);
        if(!lockReleased)std::cerr<<"lock reacquire after expected rejection: "<<error<<'\n';
        check(lockReleased,"failed update releases installation lock");
        { std::ofstream out(root/"broken.db");out<<"invalid"; }
        check(!installFactoryPackage(root/"broken.db",store,error) && loadAvailableFactory(HC_FACTORY_DB_PATH,store).library.libraryVersion==3,"invalid update retains active library");
        { std::ofstream out(store/"active.txt");out<<"../../user.db"; }
        active=loadAvailableFactory(HC_FACTORY_DB_PATH,store);
        check(active.library.libraryVersion==2 && !active.warning.empty(),"bad pointer safely falls back");
        const bool repaired=installFactoryPackage(root/"update.db",store,error);
        if(!repaired)std::cerr<<"repair install error: "<<error<<'\n';
        active=loadAvailableFactory(HC_FACTORY_DB_PATH,store);
        if(active.library.libraryVersion!=3)
            std::cerr<<"repair result version="<<active.library.libraryVersion
                <<" warning="<<active.warning<<" path="<<active.path.string()<<'\n';
        check(repaired && active.library.libraryVersion==3,"repair damaged active pointer");
        { std::ofstream out(store/"active.txt");out<<"3\n"; }
        std::filesystem::remove(store/"3"/"factory.db");
        check(loadAvailableFactory(HC_FACTORY_DB_PATH,store).library.libraryVersion==2,"missing active database fallback");
        check(user.loadAll().templates.size()==1,"personal collection still intact");
        std::filesystem::remove_all(root);
        std::cout<<"LibraryStore "<<checks<<" checks PASS\n";return 0;
    } catch(const std::exception& e) {
        std::cerr<<e.what()<<'\n';std::error_code cleanupError;
        for(int attempt=0;attempt<20;++attempt){cleanupError.clear();std::filesystem::remove_all(root,cleanupError);if(!cleanupError)break;std::this_thread::sleep_for(std::chrono::milliseconds(25));}
        if(cleanupError)std::cerr<<"test cleanup also failed: "<<cleanupError.message()<<'\n';
        return 1;
    }
}
