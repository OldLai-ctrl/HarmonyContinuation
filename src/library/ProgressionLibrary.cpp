#include "ProgressionLibrary.h"
#include "TemplateJson.h"
#if defined(_WIN32)
#include <winsqlite/winsqlite3.h>
#else
#include <sqlite3.h>
#endif
#include <memory>
#include <stdexcept>
#if defined(_WIN32)
#include <windows.h>
#endif

namespace harmony::library {
namespace {
struct Db {
    sqlite3* value{};
    Db(const std::filesystem::path& path, int flags) {
        const auto utf8 = path.u8string();
        if (sqlite3_open_v2(reinterpret_cast<const char*>(utf8.c_str()), &value, flags, nullptr) != SQLITE_OK)
            throw std::runtime_error(value ? sqlite3_errmsg(value) : "cannot open SQLite database");
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
void initialize(Db& db, const char* kind) {
    db.exec("CREATE TABLE IF NOT EXISTS metadata (key TEXT PRIMARY KEY, value TEXT NOT NULL);");
    db.exec("CREATE TABLE IF NOT EXISTS progressions (id TEXT PRIMARY KEY, payload TEXT NOT NULL);");
    Statement version(db, "SELECT value FROM metadata WHERE key='schema_version'");
    const auto code = sqlite3_step(version.value);
    if (code == SQLITE_ROW) {
        if (std::string(reinterpret_cast<const char*>(sqlite3_column_text(version.value, 0))) != "1")
            throw std::runtime_error("unsupported schema_version; migration required");
    } else if (code == SQLITE_DONE) {
        db.exec("INSERT INTO metadata VALUES ('schema_version','1')");
        db.exec("INSERT INTO metadata VALUES ('library_version','1')");
        Statement type(db, "INSERT INTO metadata VALUES ('library_type',?)");
        sqlite3_bind_text(type.value, 1, kind, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(type.value) != SQLITE_DONE) throw std::runtime_error(sqlite3_errmsg(db.value));
    } else throw std::runtime_error(sqlite3_errmsg(db.value));
    Statement check(db, "SELECT value FROM metadata WHERE key='library_type'");
    if (sqlite3_step(check.value) != SQLITE_ROW ||
        std::string(reinterpret_cast<const char*>(sqlite3_column_text(check.value, 0))) != kind)
        throw std::runtime_error("wrong library_type");
}
void validateReadOnly(Db& db, const char* kind) {
    Statement st(db, "SELECT key,value FROM metadata");
    std::string schema, type, version;
    while (sqlite3_step(st.value) == SQLITE_ROW) {
        const std::string key(reinterpret_cast<const char*>(sqlite3_column_text(st.value, 0)));
        const std::string value(reinterpret_cast<const char*>(sqlite3_column_text(st.value, 1)));
        if (key == "schema_version") schema = value;
        else if (key == "library_type") type = value;
        else if (key == "library_version") version = value;
    }
    if (schema != "1" || version.empty() || type != kind)
        throw std::runtime_error("database version/type incompatible");
}
void bindTemplate(Statement& st, const ProgressionTemplate& input) {
    const auto payload = dev::serializeTemplateJson(input);
    sqlite3_bind_text(st.value, 1, input.id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st.value, 2, payload.c_str(), -1, SQLITE_TRANSIENT);
}
LoadResult readAll(Db& db) {
    LoadResult result;
    Statement st(db, "SELECT payload FROM progressions ORDER BY id");
    int code{};
    while ((code = sqlite3_step(st.value)) == SQLITE_ROW) {
        const auto* payload = reinterpret_cast<const char*>(sqlite3_column_text(st.value, 0));
        if (!payload) throw std::runtime_error("null template payload");
        auto parsed = dev::parseTemplateJson(std::string("[") + payload + "]");
        if (!parsed) throw std::runtime_error("invalid stored template: " + parsed.error);
        result.templates.push_back(std::move(parsed.templates.front()));
    }
    if (code != SQLITE_DONE) throw std::runtime_error(sqlite3_errmsg(db.value));
    return result;
}
void checkTemplate(const ProgressionTemplate& item, const char* kind) {
    if (item.id.empty() || item.sourceType != kind || item.full.size() < 2)
        throw std::runtime_error("invalid template identity/source/events");
    auto parsed = dev::parseTemplateJson("[" + dev::serializeTemplateJson(item) + "]");
    if (!parsed) throw std::runtime_error(parsed.error);
}
} // namespace

bool compileFactory(const std::filesystem::path& output,
                    const std::vector<ProgressionTemplate>& templates, std::string& error) {
    try {
        if (templates.empty()) throw std::runtime_error("factory library is empty");
        const auto temporary = std::filesystem::path(output.string() + ".new");
        std::filesystem::remove(temporary);
        {
            Db db(temporary, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
            initialize(db, "factory");
            db.exec("BEGIN IMMEDIATE");
            try {
                Statement st(db, "INSERT INTO progressions VALUES (?,?)");
                for (const auto& item : templates) {
                    checkTemplate(item, "factory");
                    sqlite3_reset(st.value); sqlite3_clear_bindings(st.value);
                    bindTemplate(st, item);
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
        Db db(path, SQLITE_OPEN_READONLY); validateReadOnly(db, "factory");
        return readAll(db);
    } catch (const std::exception& e) { return {{}, e.what()}; }
}
bool UserLibrary::addProgression(const ProgressionTemplate& input, std::string& error) {
    try {
        checkTemplate(input, "user");
        Db db(path_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE); initialize(db, "user");
        Statement st(db, "INSERT INTO progressions VALUES (?,?)"); bindTemplate(st, input);
        if (sqlite3_step(st.value) != SQLITE_DONE) throw std::runtime_error(sqlite3_errmsg(db.value));
        return true;
    } catch (const std::exception& e) { error = e.what(); return false; }
}
bool UserLibrary::updateProgression(const ProgressionTemplate& input, std::string& error) {
    try {
        checkTemplate(input, "user");
        Db db(path_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE); initialize(db, "user");
        Statement st(db, "UPDATE progressions SET payload=? WHERE id=?");
        const auto payload = dev::serializeTemplateJson(input);
        sqlite3_bind_text(st.value, 1, payload.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st.value, 2, input.id.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st.value) != SQLITE_DONE || sqlite3_changes(db.value) != 1)
            throw std::runtime_error("template not found or update failed");
        return true;
    } catch (const std::exception& e) { error = e.what(); return false; }
}
bool UserLibrary::removeProgression(const TemplateID& id, std::string& error) {
    try {
        Db db(path_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE); initialize(db, "user");
        Statement st(db, "DELETE FROM progressions WHERE id=?");
        sqlite3_bind_text(st.value, 1, id.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st.value) != SQLITE_DONE || sqlite3_changes(db.value) != 1)
            throw std::runtime_error("template not found or delete failed");
        return true;
    } catch (const std::exception& e) { error = e.what(); return false; }
}
LoadResult UserLibrary::listProgressions() const {
    try {
        if (!std::filesystem::exists(path_)) return {};
        Db db(path_, SQLITE_OPEN_READONLY); validateReadOnly(db, "user");
        return readAll(db);
    } catch (const std::exception& e) { return {{}, e.what()}; }
}
} // namespace harmony::library
