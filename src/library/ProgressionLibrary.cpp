#include "ProgressionLibrary.h"
#include "TemplateJson.h"
#include "LegacyFactoryIdResolver.h"
#include "persistence/ChordData.h"
#if defined(_WIN32)
#include <winsqlite/winsqlite3.h>
#else
#include <sqlite3.h>
#endif
#include <memory>
#include <stdexcept>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <charconv>
#include <unordered_set>
#include <algorithm>
#if defined(_WIN32)
#include <windows.h>
#endif

namespace harmony::library {
namespace {
std::string utcNow() {
    const auto now=std::chrono::system_clock::now();
    const auto time=std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
#if defined(_WIN32)
    gmtime_s(&utc,&time);
#else
    gmtime_r(&time,&utc);
#endif
    std::ostringstream out;out<<std::put_time(&utc,"%Y-%m-%dT%H:%M:%SZ");return out.str();
}
struct Db {
    sqlite3* value{};
    Db(const std::filesystem::path& path, int flags) {
        const auto utf8 = path.u8string();
        if (sqlite3_open_v2(reinterpret_cast<const char*>(utf8.c_str()), &value, flags, nullptr) != SQLITE_OK) {
            const std::string error=value ? sqlite3_errmsg(value) : "cannot open SQLite database";
            if (value) sqlite3_close(value);
            value=nullptr;
            throw std::runtime_error(error);
        }
        sqlite3_busy_timeout(value, 2000);
    }
    ~Db() { if (value) sqlite3_close(value); }
    Db(const Db&) = delete;
    Db& operator=(const Db&) = delete;
    void exec(const char* sql) {
        char* message{};
        if (sqlite3_exec(value, sql, nullptr, nullptr, &message) != SQLITE_OK) {
            const std::string error = message ? message : "SQLite error";
            sqlite3_free(message); throw std::runtime_error(error);
        }
    }
};
struct Statement {
    sqlite3_stmt* value{};
    Statement(Db& db, const char* sql) {
        if (sqlite3_prepare_v2(db.value, sql, -1, &value, nullptr) != SQLITE_OK)
            throw std::runtime_error(sqlite3_errmsg(db.value));
    }
    ~Statement() { sqlite3_finalize(value); }
};
int initialize(Db& db, const char* kind) {
    db.exec("CREATE TABLE IF NOT EXISTS metadata (key TEXT PRIMARY KEY, value TEXT NOT NULL);");
    db.exec("CREATE TABLE IF NOT EXISTS progressions (id TEXT PRIMARY KEY, payload TEXT NOT NULL);");
    Statement version(db, "SELECT value FROM metadata WHERE key='schema_version'");
    const auto code = sqlite3_step(version.value);
    int schema=std::string_view(kind)=="user"?userSchemaVersion:1;
    if (code == SQLITE_ROW) {
        const std::string value(reinterpret_cast<const char*>(sqlite3_column_text(version.value,0)));
        if (value!="1"&&value!="2")
            throw std::runtime_error("unsupported schema_version; migration required");
        schema=value=="1"?1:2;
    } else if (code == SQLITE_DONE) {
        db.exec(("INSERT INTO metadata VALUES ('schema_version','"+std::to_string(schema)+"')").c_str());
        if (std::string_view(kind)=="factory") db.exec("INSERT INTO metadata VALUES ('library_version','2')");
        else db.exec("INSERT INTO metadata VALUES ('library_version','1')");
        Statement type(db, "INSERT INTO metadata VALUES ('library_type',?)");
        sqlite3_bind_text(type.value, 1, kind, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(type.value) != SQLITE_DONE) throw std::runtime_error(sqlite3_errmsg(db.value));
        if (std::string_view(kind)=="user") db.exec("CREATE TABLE progression_data (id TEXT PRIMARY KEY REFERENCES progressions(id), data TEXT NOT NULL)");
    } else throw std::runtime_error(sqlite3_errmsg(db.value));
    Statement check(db, "SELECT value FROM metadata WHERE key='library_type'");
    if (sqlite3_step(check.value) != SQLITE_ROW ||
        std::string(reinterpret_cast<const char*>(sqlite3_column_text(check.value, 0))) != kind)
        throw std::runtime_error("wrong library_type");
    return schema;
}
int validateReadOnly(Db& db, const char* kind) {
    Statement st(db, "SELECT key,value FROM metadata");
    std::string schema, type, version;
    int code{};
    while ((code=sqlite3_step(st.value)) == SQLITE_ROW) {
        if (!sqlite3_column_text(st.value,0) || !sqlite3_column_text(st.value,1) ||
            sqlite3_column_bytes(st.value,0)>128 || sqlite3_column_bytes(st.value,1)>128)
            throw std::runtime_error("invalid database metadata");
        const std::string key(reinterpret_cast<const char*>(sqlite3_column_text(st.value, 0)));
        const std::string value(reinterpret_cast<const char*>(sqlite3_column_text(st.value, 1)));
        if (key == "schema_version") schema = value;
        else if (key == "library_type") type = value;
        else if (key == "library_version") version = value;
    }
    int number{};
    const auto parsed=std::from_chars(version.data(),version.data()+version.size(),number);
    if (code!=SQLITE_DONE || (schema!="1"&&schema!="2") || type != kind ||
        parsed.ec!=std::errc{} || parsed.ptr!=version.data()+version.size() || number<1)
        throw std::runtime_error("database version/type incompatible");
    return number;
}
int storedSchema(Db& db) {
    Statement st(db,"SELECT value FROM metadata WHERE key='schema_version'");
    if(sqlite3_step(st.value)!=SQLITE_ROW)throw std::runtime_error("missing schema");
    return std::string(reinterpret_cast<const char*>(sqlite3_column_text(st.value,0)))=="2"?2:1;
}
void bindTemplate(Statement& st, const ProgressionTemplate& input, bool typed=false) {
    const auto payload = dev::serializeTemplateJson(input,typed);
    sqlite3_bind_text(st.value, 1, input.id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st.value, 2, payload.c_str(), -1, SQLITE_TRANSIENT);
}
LoadResult readAll(Db& db, bool typedUser=false) {
    LoadResult result;
    Statement st(db, "SELECT payload,id FROM progressions ORDER BY id");
    std::unordered_set<TemplateID> ids;
    int code{};
    while ((code = sqlite3_step(st.value)) == SQLITE_ROW) {
        if (result.templates.size()>=100000 || sqlite3_column_bytes(st.value,0)>131072)
            throw std::runtime_error("library resource limit exceeded");
        const auto* payload = reinterpret_cast<const char*>(sqlite3_column_text(st.value, 0));
        if (!payload) throw std::runtime_error("null template payload");
        auto parsed = dev::parseTemplateJson(std::string("[") + payload + "]");
        if (!parsed) throw std::runtime_error("invalid stored template: " + parsed.error);
        const auto* storedId=reinterpret_cast<const char*>(sqlite3_column_text(st.value,1));
        if (!storedId || parsed.templates.size()!=1 || parsed.templates.front().id!=storedId ||
            !ids.insert(parsed.templates.front().id).second)
            throw std::runtime_error("invalid or duplicate stored template ID");
        result.templates.push_back(std::move(parsed.templates.front()));
    }
    if (code != SQLITE_DONE) throw std::runtime_error(sqlite3_errmsg(db.value));
    if(typedUser) {
        Statement data(db,"SELECT id,data FROM progression_data ORDER BY id");std::size_t count{};
        while((code=sqlite3_step(data.value))==SQLITE_ROW) {
            if(sqlite3_column_bytes(data.value,1)>131072)throw std::runtime_error("user data limit exceeded");
            const auto* id=reinterpret_cast<const char*>(sqlite3_column_text(data.value,0));
            const auto* json=reinterpret_cast<const char*>(sqlite3_column_text(data.value,1));
            if(!id||!json)throw std::runtime_error("invalid user chord storage");
            const auto it=std::find_if(result.templates.begin(),result.templates.end(),[&](const auto& t){return t.id==id;});
            if(it==result.templates.end())throw std::runtime_error("orphan user chord data");
            persistence::decodeTemplateData(json,*it);prepareTemplate(*it);++count;
        }
        if(code!=SQLITE_DONE||count!=result.templates.size())throw std::runtime_error("incomplete user chord storage");
    }
    return result;
}
void putData(Db& db,const ProgressionTemplate& item) {
    const auto data=persistence::encodeTemplateData(item);auto roundtrip=item;
    persistence::decodeTemplateData(data,roundtrip);
    if(persistence::encodeTemplateData(roundtrip)!=data)throw std::runtime_error("user semantic roundtrip failed");
    Statement st(db,"INSERT OR REPLACE INTO progression_data(id,data) VALUES (?,?)");
    sqlite3_bind_text(st.value,1,item.id.c_str(),-1,SQLITE_TRANSIENT);sqlite3_bind_text(st.value,2,data.c_str(),-1,SQLITE_TRANSIENT);
    if(sqlite3_step(st.value)!=SQLITE_DONE)throw std::runtime_error(sqlite3_errmsg(db.value));
}
void backupV1(Db& db,const std::filesystem::path& path) {
    const auto backup=std::filesystem::path(path.string()+".v1-backup");
    if(std::filesystem::exists(backup)) {
        Db old(backup,SQLITE_OPEN_READONLY);validateReadOnly(old,"user");
        if(storedSchema(old)!=1)throw std::runtime_error("invalid v1 backup");(void)readAll(old);return;
    }
    const auto staging=std::filesystem::path(backup.string()+".new");
    try {
        { Db dest(staging,SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE);
          auto* job=sqlite3_backup_init(dest.value,"main",db.value,"main");if(!job)throw std::runtime_error("cannot start user backup");
          const auto rc=sqlite3_backup_step(job,-1);const auto finish=sqlite3_backup_finish(job);
          if(rc!=SQLITE_DONE||finish!=SQLITE_OK)throw std::runtime_error("user backup failed");
          validateReadOnly(dest,"user");(void)readAll(dest);
        }
        std::filesystem::rename(staging,backup);
    } catch(...) {std::error_code ignored;std::filesystem::remove(staging,ignored);throw;}
}
void ensureUserV2(Db& db,const std::filesystem::path& path) {
    validateReadOnly(db,"user");if(storedSchema(db)==2)return;
    db.exec("BEGIN IMMEDIATE");
    try {
        // Serialize concurrent migrations before backing up or adding storage.
        if(storedSchema(db)==2){db.exec("COMMIT");return;}
        const auto old=readAll(db);
        for(const auto& t:old.templates)if(t.sourceType!="user")throw std::runtime_error("invalid legacy user source");
        {Db source(path,SQLITE_OPEN_READONLY);backupV1(source,path);}
        db.exec("CREATE TABLE progression_data (id TEXT PRIMARY KEY REFERENCES progressions(id), data TEXT NOT NULL)");
        for(const auto& item:old.templates)putData(db,item);
        const auto restored=readAll(db,true);
        if(old.templates.size()!=restored.templates.size())throw std::runtime_error("user migration count mismatch");
        for(std::size_t i=0;i<old.templates.size();++i)
            if(dev::serializeTemplateJson(old.templates[i])!=dev::serializeTemplateJson(restored.templates[i]))throw std::runtime_error("user migration metadata/content mismatch");
        db.exec("UPDATE metadata SET value='2' WHERE key='schema_version'");db.exec("COMMIT");
    } catch(...) {db.exec("ROLLBACK");throw;}
}
void checkTemplate(const ProgressionTemplate& item, const char* kind) {
    if (item.id.empty() || item.sourceType != kind || item.full.size() < 2)
        throw std::runtime_error("invalid template identity/source/events");
    auto parsed = dev::parseTemplateJson("[" + dev::serializeTemplateJson(item) + "]");
    if (!parsed) throw std::runtime_error(parsed.error);
    if(std::string_view(kind)=="factory")for(const auto& e:item.full)
        if(e.quality==ChordQuality::Unknown)throw std::runtime_error("QUESTIONABLE: unsupported factory chord");
}
} // namespace

bool compileFactory(const std::filesystem::path& output,
                    const std::vector<ProgressionTemplate>& templates, std::string& error, int libraryVersion) {
    try {
        if (templates.empty() || libraryVersion<1) throw std::runtime_error("invalid factory library/version");
        if(libraryVersion<3)for(const auto& t:templates) {
            if(!t.rawChords.empty()||std::any_of(t.full.begin(),t.full.end(),[](const auto& e){return e.bassInterval||e.intervalMask||e.colorMask||!e.displaySuffix.empty();}))
                throw std::runtime_error("rich chords require Factory Schema 2 / Library 3");
        }
        if(libraryVersion>=3&&!validateFactoryRedirectTargets(templates,error))throw std::runtime_error(error);
        const auto temporary = std::filesystem::path(output.string() + ".new");
        std::filesystem::remove(temporary);
        {
            Db db(temporary, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
            initialize(db, "factory");
            if(libraryVersion>=3)db.exec("UPDATE metadata SET value='2' WHERE key='schema_version'");
            db.exec(("UPDATE metadata SET value='"+std::to_string(libraryVersion)+"' WHERE key='library_version'").c_str());
            db.exec("BEGIN IMMEDIATE");
            try {
                Statement st(db, "INSERT INTO progressions VALUES (?,?)");
                for (const auto& item : templates) {
                    checkTemplate(item, "factory");
                    sqlite3_reset(st.value); sqlite3_clear_bindings(st.value);
                    bindTemplate(st, item,libraryVersion>=3);
                    if (sqlite3_step(st.value) != SQLITE_DONE) throw std::runtime_error(sqlite3_errmsg(db.value));
                }
                db.exec("COMMIT");
            } catch (...) { db.exec("ROLLBACK"); throw; }
        }
#if defined(_WIN32)
        if (!MoveFileExW(temporary.c_str(), output.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("cannot replace factory database atomically");
#else
        std::filesystem::rename(temporary, output);
#endif
        return true;
    } catch (const std::exception& e) { error = e.what(); return false; }
}
LoadResult loadFactory(const std::filesystem::path& path) {
    try {
        if (std::filesystem::file_size(path)>268435456) throw std::runtime_error("library exceeds 256 MiB");
        Db db(path, SQLITE_OPEN_READONLY);
        const auto version=validateReadOnly(db, "factory");
        auto result=readAll(db); result.libraryVersion=version;result.storageSchemaVersion=storedSchema(db);
        if((version>=3&&result.storageSchemaVersion!=2)||(version<3&&result.storageSchemaVersion!=1))throw std::runtime_error("factory schema/library version mismatch");
        if(version>=3){std::string error;if(!validateFactoryRedirectTargets(result.templates,error))throw std::runtime_error(error);}
        if (result.templates.empty()) throw std::runtime_error("factory library is empty");
        for (const auto& item:result.templates) {
            checkTemplate(item,"factory");
            if (item.nameZh.empty() || item.nameEn.empty()) throw std::runtime_error("factory display names missing");
        }
        return result;
    } catch (const std::exception& e) { return {{}, e.what()}; }
}
bool UserLibrary::addProgression(const ProgressionTemplate& input, std::string& error) {
    try {
        auto item=input;
        if(item.createdAt.empty())item.createdAt=utcNow();
        if(item.updatedAt.empty())item.updatedAt=item.createdAt;
        checkTemplate(item, "user");
        Db db(path_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE); initialize(db, "user");
        ensureUserV2(db,path_);db.exec("BEGIN IMMEDIATE");
        try {
            Statement st(db, "INSERT INTO progressions VALUES (?,?)"); bindTemplate(st, item);
            if (sqlite3_step(st.value) != SQLITE_DONE) throw std::runtime_error(sqlite3_errmsg(db.value));
            putData(db,item);db.exec("COMMIT");
        }catch(...){db.exec("ROLLBACK");throw;}
        return true;
    } catch (const std::exception& e) { error = e.what(); return false; }
}
bool UserLibrary::updateProgression(const ProgressionTemplate& input, std::string& error) {
    try {
        auto item=input;
        checkTemplate(item, "user");
        Db db(path_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE); initialize(db, "user");
        ensureUserV2(db,path_);
        const auto current=readAll(db,true);
        const auto existing=std::find_if(current.templates.begin(),current.templates.end(),[&](const auto& t){return t.id==input.id;});
        if(existing==current.templates.end())throw std::runtime_error("template not found");
        if(dev::serializeTemplateJson(*existing)==dev::serializeTemplateJson(input))return true;
        item.updatedAt=utcNow();db.exec("BEGIN IMMEDIATE");
        try {
        Statement st(db, "UPDATE progressions SET payload=? WHERE id=?");
        const auto payload = dev::serializeTemplateJson(item,false);
        sqlite3_bind_text(st.value, 1, payload.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st.value, 2, input.id.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st.value) != SQLITE_DONE || sqlite3_changes(db.value) != 1)
            throw std::runtime_error("template not found or update failed");
        putData(db,item);db.exec("COMMIT");
        }catch(...){db.exec("ROLLBACK");throw;}
        return true;
    } catch (const std::exception& e) { error = e.what(); return false; }
}
bool UserLibrary::removeProgression(const TemplateID& id, std::string& error) {
    try {
        Db db(path_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE); initialize(db, "user");
        db.exec("BEGIN IMMEDIATE");try {
        if(storedSchema(db)==2) {
            Statement data(db,"DELETE FROM progression_data WHERE id=?");sqlite3_bind_text(data.value,1,id.c_str(),-1,SQLITE_TRANSIENT);
            if(sqlite3_step(data.value)!=SQLITE_DONE)throw std::runtime_error("user data removal failed");
        }
        Statement st(db, "DELETE FROM progressions WHERE id=?");
        sqlite3_bind_text(st.value, 1, id.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st.value) != SQLITE_DONE || sqlite3_changes(db.value) != 1)
            throw std::runtime_error("template not found or delete failed");
        db.exec("COMMIT");}catch(...){db.exec("ROLLBACK");throw;}
        return true;
    } catch (const std::exception& e) { error = e.what(); return false; }
}
LoadResult UserLibrary::listProgressions() const {
    try {
        if (!std::filesystem::exists(path_)) return {};
        Db db(path_, SQLITE_OPEN_READONLY); validateReadOnly(db, "user");
        auto result=readAll(db,storedSchema(db)==2);result.storageSchemaVersion=storedSchema(db);return result;
    } catch (const std::exception& e) { return {{}, e.what()}; }
}
} // namespace harmony::library
