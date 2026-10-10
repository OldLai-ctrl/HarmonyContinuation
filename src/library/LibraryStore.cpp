#include "LibraryStore.h"
#include <fstream>
#include <charconv>
#include <algorithm>
#include <array>
#include <stdexcept>
#if defined(_WIN32)
#include <windows.h>
#include <shlobj.h>
#endif

namespace harmony::library {
namespace {
#if defined(_WIN32)
std::filesystem::path knownFolder(REFKNOWNFOLDERID id) {
    PWSTR value{};
    if (FAILED(SHGetKnownFolderPath(id,0,nullptr,&value))) throw std::runtime_error("Windows data folder unavailable");
    const std::filesystem::path path(value); CoTaskMemFree(value); return path;
}
#endif
int activeVersion(const std::filesystem::path& store) {
    std::ifstream in(store/"active.txt",std::ios::binary);
    if (!in) return 0;
    std::array<char,32> value{}; in.read(value.data(),value.size());
    auto size=static_cast<std::size_t>(in.gcount());
    if (size==value.size()) throw std::runtime_error("invalid active library pointer");
    while (size && (value[size-1]=='\n' || value[size-1]=='\r')) --size;
    int version{}; const auto result=std::from_chars(value.data(),value.data()+size,version);
    if (!size || result.ec!=std::errc{} || result.ptr!=value.data()+size || version<1)
        throw std::runtime_error("invalid active library pointer");
    return version;
}
void replaceFile(const std::filesystem::path& source,const std::filesystem::path& dest) {
#if defined(_WIN32)
    if (!MoveFileExW(source.c_str(),dest.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("cannot activate library; previous library retained");
#else
    std::filesystem::rename(source,dest);
#endif
}
bool identical(const std::filesystem::path& a,const std::filesystem::path& b) {
    if (std::filesystem::file_size(a)!=std::filesystem::file_size(b)) return false;
    std::ifstream x(a,std::ios::binary), y(b,std::ios::binary);
    std::array<char,65536> xb{},yb{};
    while (x) {
        x.read(xb.data(),xb.size()); y.read(yb.data(),yb.size());
        if (x.gcount()!=y.gcount() || !std::equal(xb.begin(),xb.begin()+x.gcount(),yb.begin())) return false;
    }
    return x.eof() && y.eof();
}
struct StoreLock {
    std::filesystem::path path;
#if defined(_WIN32)
    HANDLE handle{INVALID_HANDLE_VALUE};
#endif
    explicit StoreLock(const std::filesystem::path& store):path(store/".update-lock") {
#if defined(_WIN32)
        handle=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL|FILE_FLAG_DELETE_ON_CLOSE,nullptr);
        if (handle==INVALID_HANDLE_VALUE) throw std::runtime_error("another library update is active or store is not writable");
#else
        if (!std::filesystem::create_directory(path)) throw std::runtime_error("another library update is active");
#endif
    }
    ~StoreLock() {
#if defined(_WIN32)
        if (handle!=INVALID_HANDLE_VALUE) CloseHandle(handle);
#else
        std::error_code ec; std::filesystem::remove(path,ec);
#endif
    }
};
}
std::filesystem::path factoryStoreDirectory() {
#if defined(_WIN32)
    return knownFolder(FOLDERID_ProgramData)/"HarmonyContinuation"/"Libraries"/"Factory";
#else
    return "/usr/local/share/HarmonyContinuation/Libraries/Factory";
#endif
}
std::filesystem::path userDatabasePath() {
#if defined(_WIN32)
    return knownFolder(FOLDERID_LocalAppData)/"HarmonyContinuation"/"user.db";
#else
    return "user.db";
#endif
}
FactorySelection loadAvailableFactory(const std::filesystem::path& bundled,const std::filesystem::path& store) {
    FactorySelection result;
    try {
        const auto version=activeVersion(store);
        if (version) {
            result.path=store/std::to_string(version)/"factory.db";
            result.library=loadFactory(result.path);
            if (!result.library || result.library.libraryVersion!=version)
                throw std::runtime_error("installed library invalid: "+result.library.error);
#if defined(HC_BUNDLED_FACTORY_V4)
            // Opt-in V4 development builds use their own newer bundle without
            // activating or overwriting the production ProgramData library.
            if(version<4){auto development=loadFactory(bundled);
                if(development&&development.libraryVersion==4){result.path=bundled;result.library=std::move(development);}}
#endif
            return result;
        }
    } catch (const std::exception& e) { result.warning=std::string(e.what())+"; using bundled library"; }
    result.path=bundled; result.library=loadFactory(bundled); return result;
}
bool installFactoryPackage(const std::filesystem::path& source,const std::filesystem::path& store,std::string& error) {
    std::filesystem::path staging;
    try {
        // Validate before touching installed data, then validate the copied bytes too.
        const auto incoming=loadFactory(source);
        if (!incoming) throw std::runtime_error(incoming.error);
        std::filesystem::create_directories(store);
        StoreLock lock(store);
        int current{};
        try { current=activeVersion(store); } catch (const std::exception&) {
            // A damaged pointer is replaceable after validating the incoming package.
            // The existing version directories remain untouched.
        }
        if (current>incoming.libraryVersion) return true; // Older app repairs must not downgrade its library.
        const auto folder=store/std::to_string(incoming.libraryVersion);
        std::filesystem::create_directories(folder);
        const auto destination=folder/"factory.db";
        if (std::filesystem::exists(destination)) {
            if (!identical(source,destination)) throw std::runtime_error("library version already exists with different content; increase library_version");
        } else {
            staging=folder/"factory.db.new";
            std::filesystem::copy_file(source,staging,std::filesystem::copy_options::overwrite_existing);
            const auto copied=loadFactory(staging);
            if (!copied || copied.libraryVersion!=incoming.libraryVersion || !identical(source,staging))
                throw std::runtime_error("copied library validation failed");
            replaceFile(staging,destination); staging.clear();
        }
        staging=store/"active.txt.new";
        { std::ofstream out(staging,std::ios::binary|std::ios::trunc); out<<incoming.libraryVersion<<'\n'; out.close();
          if (!out) throw std::runtime_error("cannot write active library pointer"); }
        replaceFile(staging,store/"active.txt"); staging.clear();
        return true;
    } catch (const std::exception& e) {
        error=e.what(); if (!staging.empty()) { std::error_code ec; std::filesystem::remove(staging,ec); } return false;
    }
}
} // namespace harmony::library
