#include "library/LibraryStore.h"
#include <iostream>
#if defined(_WIN32)
#include <windows.h>
#endif

namespace {
int run(const std::string& command,const std::filesystem::path& path,const std::filesystem::path& store) {
    using namespace harmony::library;
    if (command=="inspect") {
        const auto loaded=loadFactory(path);
        if (!loaded) { std::cerr<<loaded.error<<'\n'; return 1; }
        std::cout<<"library_version="<<loaded.libraryVersion<<" schema="<<schemaVersion
            <<" progressions="<<loaded.templates.size()<<'\n'; return 0;
    }
    if (command=="install") {
        std::string error;
        if (!installFactoryPackage(path,store,error)) { std::cerr<<error<<'\n'; return 1; }
        std::cout<<"Library installed; user collections and previous versions retained\n"; return 0;
    }
    if (command=="check-plugin") {
#if defined(_WIN32)
        if (!std::filesystem::exists(path)) return 0;
        const auto handle=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE|DELETE,0,nullptr,OPEN_EXISTING,0,nullptr);
        if (handle==INVALID_HANDLE_VALUE) { std::cerr<<"Close your DAW before updating\n"; return 1; }
        CloseHandle(handle);
#endif
        return 0;
    }
    std::cerr<<"usage: library_manager inspect DB | install DB [STORE] | check-plugin BINARY\n"; return 2;
}
}
#if defined(_WIN32)
int wmain(int argc,wchar_t** argv) {
    try {
        if (argc<3 || argc>4) return run("",{},{});
        const std::wstring cmd(argv[1]);
        return run(std::string(cmd.begin(),cmd.end()),argv[2],argc==4?std::filesystem::path(argv[3]):harmony::library::factoryStoreDirectory());
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
#else
int main(int argc,char** argv) {
    try {
        if(argc<3 || argc>4) return run("",{},{});
        return run(argv[1],argv[2],argc==4?std::filesystem::path(argv[3]):harmony::library::factoryStoreDirectory());
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
#endif
