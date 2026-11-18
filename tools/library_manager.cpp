#include "library/LibraryStore.h"
#include <iostream>
#if defined(_WIN32)
#include <windows.h>
#endif

// Installer-only identity check: compare every schema object, metadata value and
// raw payload byte. Version/count alone never authorizes adoption. Read-only;
// SQLite page layout (e.g. VACUUM) is deliberately not part of identity.
#if defined(_WIN32)
#include <winsqlite/winsqlite3.h>
#else
#include <sqlite3.h>
#endif
#include <vector>
#include <fstream>
#include <memory>
namespace {
std::vector<std::string> factoryIdentity(const std::filesystem::path& path) {
    if (std::filesystem::file_size(path)>64*1024*1024) throw std::runtime_error("Factory exceeds identity-check size limit");
    const auto utf8=path.u8string(); sqlite3* raw{};
    const int opened=sqlite3_open_v2(reinterpret_cast<const char*>(utf8.c_str()),&raw,SQLITE_OPEN_READONLY,nullptr);
    std::unique_ptr<sqlite3,decltype(&sqlite3_close)> db(raw,sqlite3_close);
    if(opened!=SQLITE_OK) throw std::runtime_error("Cannot open Factory read-only");
    sqlite3_limit(raw,SQLITE_LIMIT_LENGTH,1024*1024);
    if(sqlite3_exec(raw,"PRAGMA trusted_schema=OFF; PRAGMA query_only=ON; BEGIN",nullptr,nullptr,nullptr)!=SQLITE_OK)
        throw std::runtime_error("Cannot begin read-only identity check");
    sqlite3_stmt* guard{};
    const char* guardSql="SELECT count(*) FROM sqlite_master WHERE type NOT IN ('table','index') OR (type='table' AND name NOT IN ('metadata','progressions')) OR (type='index' AND name NOT IN ('sqlite_autoindex_metadata_1','sqlite_autoindex_progressions_1'))";
    if(sqlite3_prepare_v2(raw,guardSql,-1,&guard,nullptr)!=SQLITE_OK)throw std::runtime_error("Invalid Factory schema");
    std::unique_ptr<sqlite3_stmt,decltype(&sqlite3_finalize)> guardStatement(guard,sqlite3_finalize);
    if(sqlite3_step(guard)!=SQLITE_ROW || sqlite3_column_int(guard,0)!=0)
        throw std::runtime_error("Unrecognized Factory schema objects");
    std::vector<std::string> rows;
    for (const auto* sql : {"PRAGMA quick_check", "PRAGMA user_version", "PRAGMA application_id", "SELECT type,name,tbl_name,sql FROM sqlite_master ORDER BY type,name",
         "SELECT key,value FROM metadata ORDER BY key", "SELECT id,payload FROM progressions ORDER BY id"}) {
        sqlite3_stmt* statement{};
        if(sqlite3_prepare_v2(raw,sql,-1,&statement,nullptr)!=SQLITE_OK) throw std::runtime_error("Invalid Factory schema");
        std::unique_ptr<sqlite3_stmt,decltype(&sqlite3_finalize)> st(statement,sqlite3_finalize);
        int code{};
        while((code=sqlite3_step(statement))==SQLITE_ROW) {
            std::string row;
            for(int i=0;i<sqlite3_column_count(statement);++i) {
                const int type=sqlite3_column_type(statement,i), n=sqlite3_column_bytes(statement,i);
                row+=std::to_string(type)+":"+std::to_string(n)+":";
                if(n) row.append(static_cast<const char*>(sqlite3_column_blob(statement,i)),n);
                row+='|';
            }
            rows.push_back(std::move(row));
            if(rows.size()>100000)throw std::runtime_error("Factory row limit exceeded");
        }
        if(code!=SQLITE_DONE)throw std::runtime_error("Cannot fully read Factory");
        rows.push_back("<query-end>");
    }
    return rows;
}
}
namespace {
int run(const std::string& command,const std::filesystem::path& path,const std::filesystem::path& store) {
    using namespace harmony::library;
    if (command=="verify-factory") {
        const auto identity=factoryIdentity(path);
        for(const auto& entry:std::filesystem::directory_iterator(store)) {
            if(entry.path().extension()==".db" && identity==factoryIdentity(entry.path())) {
                std::cout<<"Exact official Factory content: "<<entry.path().filename().string()<<'\n'; return 0;
            }
        }
        std::cerr<<"No complete official Factory content match\n"; return 1;
    }
    if (command=="describe-factory") {
        const auto loaded=loadFactory(path);
        std::ofstream report(store,std::ios::binary);
        if(!report)return 1;
        if(loaded)report<<"Library "<<loaded.libraryVersion<<" / Schema "<<loaded.storageSchemaVersion<<" / "<<loaded.templates.size()<<" entries (identity unverified)";
        else report<<"Unrecognized or unreadable Factory; metadata is not proof of origin";
        return report?0:1;
    }
    if (command=="inspect") {
        const auto loaded=loadFactory(path);
        if (!loaded) { std::cerr<<loaded.error<<'\n'; return 1; }
        std::cout<<"library_version="<<loaded.libraryVersion<<" schema="<<loaded.storageSchemaVersion
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
    std::cerr<<"usage: library_manager inspect DB | install DB [STORE] | check-plugin BINARY | verify-factory DB REFERENCE_DIR | describe-factory DB REPORT\n"; return 2;
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
