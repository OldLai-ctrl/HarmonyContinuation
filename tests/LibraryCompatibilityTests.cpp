#include "library/ProgressionLibrary.h"
#include "library/LibraryStore.h"
#include "library/LegacyFactoryIdResolver.h"
#include "persistence/ChordData.h"
#include "snapshot/RecommendationSnapshot.h"
#include "snapshot/EnrichmentSnapshot.h"
#include "session/ProductServices.h"
#include "midi/MidiClip.h"
#include "midi/StandardMidiFileWriter.h"
#include "TemplateJson.h"
#if defined(_WIN32)
#include <winsqlite/winsqlite3.h>
#else
#include <sqlite3.h>
#endif
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <algorithm>
#include <sstream>

using namespace harmony;
namespace fs=std::filesystem;
namespace {
int checks{};
void check(bool ok,const std::string& message) {++checks;if(!ok)throw std::runtime_error(message);}
std::vector<ProgressionTemplate> fixtures() {
    std::vector<ProgressionTemplate> out;
    for(const auto& e:fs::directory_iterator(HC_FACTORY_SOURCE))if(e.path().extension()==".json") {
        std::ifstream in(e.path(),std::ios::binary);const std::string text(std::istreambuf_iterator<char>{in},{});
        auto parsed=dev::parseTemplateJson(text,true);check(static_cast<bool>(parsed),parsed.error);
        out.insert(out.end(),parsed.templates.begin(),parsed.templates.end());
    }
    check(out.size()==161,"historical 161 rows");return out;
}
struct Sql {
    sqlite3* db{};
    explicit Sql(const fs::path& path){const auto p=path.u8string();check(sqlite3_open(reinterpret_cast<const char*>(p.c_str()),&db)==SQLITE_OK,"open test DB");}
    ~Sql(){sqlite3_close(db);}
    void exec(const std::string& sql){char* error{};const int rc=sqlite3_exec(db,sql.c_str(),nullptr,nullptr,&error);const std::string message=error?error:"";sqlite3_free(error);check(rc==SQLITE_OK,message);}
    std::string scalar(const char* sql){sqlite3_stmt* st{};check(sqlite3_prepare_v2(db,sql,-1,&st,nullptr)==SQLITE_OK,"prepare scalar");check(sqlite3_step(st)==SQLITE_ROW,"scalar row");const auto* value=sqlite3_column_text(st,0);std::string out=value?reinterpret_cast<const char*>(value):"";sqlite3_finalize(st);return out;}
    void insert(const ProgressionTemplate& item){sqlite3_stmt* st{};check(sqlite3_prepare_v2(db,"INSERT INTO progressions VALUES (?,?)",-1,&st,nullptr)==SQLITE_OK,"legacy insert");const auto json=dev::serializeTemplateJson(item,false);sqlite3_bind_text(st,1,item.id.c_str(),-1,SQLITE_TRANSIENT);sqlite3_bind_text(st,2,json.c_str(),-1,SQLITE_TRANSIENT);check(sqlite3_step(st)==SQLITE_DONE,"legacy fixture saved");sqlite3_finalize(st);}
};
void libraries(const fs::path& root,library::LoadResult& v2,library::LoadResult& v3) {
    const auto old=fixtures();auto canonical=library::canonicalFactoryEntries(old);std::string error;
    check(canonical.size()==155,"V3 155 unique rows");
    check(library::compileFactory(root/"2.db",old,error,2),error);
    check(library::compileFactory(root/"3.db",canonical,error,3),error);
    v2=library::loadFactory(root/"2.db");v3=library::loadFactory(root/"3.db");
    check(v2&&v2.storageSchemaVersion==1&&v2.templates.size()==161,"schema1 load");
    check(v3&&v3.storageSchemaVersion==2&&v3.templates.size()==155,"schema2 load");
}
ChordEvent chord(std::string name,int root,int bass,ChordQuality quality,unsigned mask) {
    ChordEvent e;e.name=std::move(name);e.root=static_cast<PitchClass>(root);e.bass=static_cast<PitchClass>(bass);
    e.quality=quality;e.durationQN=2;e.openEnded=false;
    std::ostringstream value;value<<"0x"<<std::hex<<mask;e.extensions.mask=value.str();
    e.extensions.type="structured";e.extensions.color="test";
    e.keyNoteValue=root+60;e.bassNoteValue=bass+24;e.rawId="fixture";e.rawName=e.name;
    e.rawKeyNote=std::to_string(*e.keyNoteValue);e.rawBassNote=std::to_string(*e.bassNoteValue);
    e.rawProjectTime="0";e.rawTimeDomain="quarterNotes";return e;
}
std::vector<ChordEvent> newChords() {
    return {chord("C",0,0,ChordQuality::Major,0x091),chord("C/E",0,4,ChordQuality::Major,0x091),
        chord("C/G",0,7,ChordQuality::Major,0x091),chord("C7/Bb",0,10,ChordQuality::Dominant7,0x491),
        chord("C/D",0,2,ChordQuality::Major,0x091),chord("Cmaj7",0,0,ChordQuality::Major7,0x891),
        chord("Cm7",0,0,ChordQuality::Minor7,0x489),chord("C7",0,0,ChordQuality::Dominant7,0x491),
        chord("C6",0,0,ChordQuality::Major,0x291),chord("Cm6",0,0,ChordQuality::Minor,0x289),
        chord("C9",0,0,ChordQuality::Dominant7,0x495),chord("Cmaj9",0,0,ChordQuality::Major7,0x895),
        chord("Cm9",0,0,ChordQuality::Minor7,0x48d),chord("Cdim7",0,0,ChordQuality::Diminished7,0x249),
        chord("Cm7b5",0,0,ChordQuality::HalfDiminished7,0x449),chord("Csus4",0,0,ChordQuality::Sus4,0x0a1),
        chord("Ab/C",8,0,ChordQuality::Major,0x091),chord("D7/F#",2,6,ChordQuality::Dominant7,0x491)};
}
ProgressionTemplate userEntry(const ChordEvent& e,int number) {
    ImportedProgressionSession imported;auto c=e;c.startQN=0;
    check(imported.replace({c},TimelineCoordinateMode::RelativeToSelection),"new source");
    ContinuationCandidate candidate;candidate.id="COMMON_MAJOR_020";candidate.primaryTemplate=candidate.id;
    candidate.key={PitchClass::C,Mode::Major};candidate.continuation.push_back(realizeContinuation(dev::parseNotationEvent("V",Mode::Major,2),candidate.key,2));
    candidate.suggestedCurrentChordDurationQN=2;
    session::SaveMetadata metadata;metadata.name="个人 "+std::to_string(number);metadata.tags={"comma,tag","中文","quoted\"tag"};metadata.note="保留\r\n备注\ttext";metadata.style=Style::Jazz;
    auto result=session::makeUserProgression(imported,candidate,metadata);check(static_cast<bool>(result),result.error);
    result.item.id="user_"+std::to_string(number);result.item.favorite=true;result.item.createdAt="2020-01-02T03:04:05Z";result.item.updatedAt=result.item.createdAt;return result.item;
}
void userRestoreSmoke(const fs::path& root) {
    auto old=userEntry(chord("C",0,0,ChordQuality::Major,0x091),0);
    old.tags={"legacy","中文"}; // Schema 1 fixture uses its historical plain tag format.
    const auto path=root/"legacy-user.db";
    {
        Sql fixture(path);
        fixture.exec("CREATE TABLE metadata(key TEXT PRIMARY KEY,value TEXT NOT NULL);CREATE TABLE progressions(id TEXT PRIMARY KEY,payload TEXT NOT NULL);INSERT INTO metadata VALUES('schema_version','1'),('library_type','user'),('library_version','1');");
        fixture.insert(old);
    }
    library::UserLibrary reopened(path);const auto loaded=reopened.loadAll();
    check(loaded&&loaded.storageSchemaVersion==1&&loaded.templates.size()==1,"legacy user restore");
    const auto& restored=loaded.templates.front();
    check(restored.id==old.id&&restored.name==old.name&&restored.note==old.note&&
          restored.favorite==old.favorite&&restored.tags==old.tags&&restored.full.size()==old.full.size(),
          "legacy user content and metadata preserved");
    std::cout<<"One Schema 1 User Library restore PASS; read-only, no migration\n";
}
void resolver(const fs::path& root) {
    library::LoadResult v2,v3;libraries(root,v2,v3);
    for(const auto& m:library::legacyFactoryIds) {
        const auto a=library::resolveFactoryId(m.requested,v2.templates),b=library::resolveFactoryId(m.requested,v3.templates);
        check(a&&a.resolved==m.requested,"Library2 direct lookup");check(b&&b.resolved==m.canonical,"Library3 canonical redirect");
        std::cout<<m.requested<<" -> "<<b.resolved<<" active=3 PASS\n";
    }
    const std::array<library::LegacyMapping,2> chain{{{"A","B"},{"B","C"}}},cycle{{{"A","B"},{"B","A"}}},duplicate{{{"A","B"},{"A","C"}}};
    check(!library::validRedirects(chain)&&!library::validRedirects(cycle)&&!library::validRedirects(duplicate),"invalid redirects rejected");
    check(!library::resolveFactoryId("missing",v3.templates),"missing safe lookup");
    std::string error;auto bad=v3.templates;std::erase_if(bad,[](const auto& t){return t.id=="COMMON_MAJOR_020";});
    check(!library::compileFactory(root/"bad.db",bad,error,3),"missing targets rejected at compile");
    check(!library::compileFactory(root/"duplicate.db",v2.templates,error,3),"legacy rows prohibited in V3");
}
snapshot::RecommendationSnapshot oldSnapshot(std::string id) {
    snapshot::RecommendationSnapshot s;s.factoryLibraryVersion=2;s.schemaVersion=2;
    check(s.imported.replace({chord("C/E",0,4,ChordQuality::Major,0x091)},TimelineCoordinateMode::RelativeToSelection),"snapshot original");
    s.candidate.primaryTemplate=id;s.candidate.id=id;s.candidate.supportingTemplates={id};s.candidate.key={PitchClass::C,Mode::Major};
    s.candidate.continuation.push_back(realizeContinuation(dev::parseNotationEvent("V",Mode::Major,2),s.candidate.key,2));
    s.candidate.rankingScore=70;s.candidate.supportCount=1;s.match=MatchResult{};s.match->templateId=id;return s;
}
std::string oldJson(const snapshot::RecommendationSnapshot& s) {
    auto json=benchmark::json::parse(snapshot::serialize(s));json.object["schemaVersion"]=2;
    json.object.erase("factoryLibraryVersion");json.object.erase("chordData");
    for(auto& event:json.object["candidate"].object["continuation"].array)event.object.erase("harmonicData");
    return benchmark::json::dump(json);
}
void snapshots(const fs::path& root) {
    library::LoadResult v2,v3;libraries(root,v2,v3);
    for(const auto& m:library::legacyFactoryIds) {
        const auto old=oldJson(oldSnapshot(std::string(m.requested)));
        const auto a=snapshot::deserialize(old,&v2),b=snapshot::deserialize(old,&v3),back=snapshot::deserialize(old,&v2);
        check(a&&a.value.candidateAvailable&&a.value.candidate.primaryTemplate==m.requested,"old snapshot + Library2");
        check(b&&b.value.candidateAvailable&&b.value.candidate.primaryTemplate==m.canonical,"old snapshot + Library3");
        check(back&&back.value.candidate.primaryTemplate==m.requested,"Library2 -> 3 -> 2 restores old reference");
        const auto written=snapshot::serialize(b.value);check(written.find(m.requested)==std::string::npos,"V3 snapshot canonical IDs only");
        const auto newer=snapshot::deserialize(written,&v3),newOld=snapshot::deserialize(written,&v2);
        check(newer&&newer.value.schemaVersion==3&&newer.value.candidateAvailable,"new snapshot + Library3");
        check(newOld&&newOld.value.candidateAvailable,"new snapshot + Library2 canonical entry");
    }
    auto missing=oldSnapshot("V3_ONLY_FIXTURE");missing.factoryLibraryVersion=3;
    const auto absent=snapshot::deserialize(snapshot::serialize(missing),&v2);
    check(absent&&!absent.value.candidateAvailable&&absent.value.imported.events.size()==1,"missing preserves phrase");
    library::LoadResult broken;broken.error="factory unavailable";
    const auto unavailable=snapshot::deserialize(snapshot::serialize(missing),&broken);
    check(unavailable&&!unavailable.value.candidateAvailable,"unavailable library preserves snapshot state");
    auto withOnly=v3;auto v3Only=withOnly.templates.front();v3Only.id="V3_ONLY_FIXTURE";withOnly.templates.push_back(v3Only);
    const auto present=snapshot::deserialize(snapshot::serialize(missing),&withOnly);
    check(present&&present.value.candidateAvailable,"new V3-only snapshot + Library3");
    auto rich=oldSnapshot("ROCK_001");rich.factoryLibraryVersion=3;rich.imported.events[0]=newChords()[11];
    rich.candidate.continuation[0].harmonicData=newChords()[17];rich.candidate.continuation[0].label="D7/F#";
    const auto richDecoded=snapshot::deserialize(snapshot::serialize(rich),&v3);
    check(richDecoded&&persistence::encodeChords(richDecoded.value.imported.events)==persistence::encodeChords(rich.imported.events),"Continuation3 full chord data");
    check(persistence::encodeChords({*richDecoded.value.candidate.continuation[0].harmonicData})==persistence::encodeChords({*rich.candidate.continuation[0].harmonicData}),"Continuation3 concrete extension/bass");
    snapshot::EnrichmentSnapshot en;en.original=rich.imported;en.candidate.id="enrichment";en.candidate.progression=rich.imported.events;
    en.candidate.operations.push_back({});const auto encoded=snapshot::serialize(en);
    auto restored=snapshot::deserializeEnrichment(encoded);check(restored&&restored.value.schemaVersion==2,"Enrichment2 decode");
    check(persistence::encodeChords(restored.value.candidate.progression)==persistence::encodeChords(en.candidate.progression),"Enrichment2 complete raw data");
    auto legacy=benchmark::json::parse(encoded);legacy.object["schemaVersion"]=1;
    for(auto* list:{&legacy.object["original"],&legacy.object["candidate"].object["progression"]})
        for(auto& c:list->array)for(const auto key:{"mask","pitches","type","color","keyNote","bassNote","source","rawId","rawName","rawKeyNote","rawBassNote","rawProjectTime","rawTimeDomain"})c.object.erase(key);
    restored=snapshot::deserializeEnrichment(benchmark::json::dump(legacy));check(restored&&restored.value.schemaVersion==2,"Enrichment1 migration");
    session::PluginSessionState state;auto oldCandidate=oldSnapshot("ROCK_001").candidate;
    check(state.pin(oldCandidate.id,session::continuationFingerprint(oldCandidate)),"old pin");state.factoryLibraryVersion=3;
    auto canonical=oldCandidate;canonical.id="COMMON_MAJOR_020";canonical.primaryTemplate=canonical.id;RecommendationSet set;set.groups[0].push_back(canonical);
    state.resolvePins(set);check(state.pinnedCandidateIds.size()==1&&state.pinnedCandidateIds[0]==canonical.id,"pin fingerprint redirected");
    const auto saved=session::serialize(state);const auto loaded=session::deserialize(saved);check(loaded&&loaded.state.pinnedCandidateIds[0]==canonical.id,"session5 canonical reference");
    state.resolvePins({});check(state.pinnedCandidateIds.size()==1,"unavailable pin retained");
    set.groups[0][0].continuation[0].durationQN=8;state.resolvePins(set);check(state.pinnedCandidateIds.empty(),"changed path is never silently substituted for a pin");
    session::PluginSessionState duplicates;duplicates.factoryLibraryVersion=3;
    auto other=oldCandidate;other.id="POP_010";other.primaryTemplate=other.id;
    duplicates.pin(oldCandidate.id,session::continuationFingerprint(oldCandidate));duplicates.pin(other.id,session::continuationFingerprint(other));
    const auto deduped=session::deserialize(session::serialize(duplicates));check(deduped&&deduped.state.pinnedCandidateIds.size()==1,"converging pins do not break session5");
    auto personal=oldSnapshot("USER_fixture");auto personalLibrary=v3;auto user=personalLibrary.templates.front();user.id=personal.candidate.primaryTemplate;user.sourceType="user";personalLibrary.templates.push_back(user);
    const auto userSnap=snapshot::deserialize(snapshot::serialize(personal),&personalLibrary);check(userSnap&&userSnap.value.candidateAvailable&&userSnap.value.candidate.primaryTemplate=="USER_fixture","personal snapshot references remain personal");
}
void userPersistence(const fs::path& root) {
    std::string error;std::vector<ProgressionTemplate> inputs;const auto kinds=newChords();
    ImportedProgressionSession completed;completed.replace({kinds.front()},TimelineCoordinateMode::RelativeToSelection);
    ContinuationCandidate proposal;proposal.key={PitchClass::C,Mode::Major};proposal.suggestedCurrentChordDurationQN=8;
    proposal.continuation.push_back(realizeContinuation(dev::parseNotationEvent("V",Mode::Major,2),proposal.key,2));
    session::SaveMetadata completedMetadata;completedMetadata.name="closed phrase";
    const auto completedSaved=session::makeUserProgression(completed,proposal,completedMetadata);
    check(completedSaved&&completedSaved.item.rawChords.front().durationQN==completed.events.front().durationQN,"closed source duration is preserved on save");
    { library::UserLibrary user(root/"new-user.db");int i{};for(const auto& c:kinds){auto item=userEntry(c,i++);check(user.addProgression(item,error),error);inputs.push_back(std::move(item));} }
    library::UserLibrary reopened(root/"new-user.db");auto loaded=reopened.loadAll();check(loaded&&loaded.storageSchemaVersion==2&&loaded.templates.size()==18,"reopen richer user schema2");
    for(const auto& input:inputs) {
        const auto it=std::find_if(loaded.templates.begin(),loaded.templates.end(),[&](const auto& t){return t.id==input.id;});check(it!=loaded.templates.end(),"saved user present");
        check(dev::serializeTemplateJson(*it)==dev::serializeTemplateJson(input),"exact content and metadata "+input.id);
        const auto before=midi::previewFromTemplate(input,*input.rawChordKey,120),after=midi::previewFromTemplate(*it,*it->rawChordKey,120);
        check(before&&after&&before.sequence.events.size()==after.sequence.events.size(),"preview after reopen "+input.id);
        const auto expected=chordPitches(input.rawChords.front());const auto& actual=after.sequence.events.front().chord;
        check(actual.root==expected.root&&actual.bass==expected.bass&&actual.intervals==expected.intervals&&actual.colorMask==expected.colorMask,"bass/extension preview "+input.id);
        const auto concrete=realizeContinuation(it->full.front(),*it->rawChordKey,2);
        check(concrete.harmonicData.has_value(),"user matching projection carries new fields");
        const auto projected=chordPitches(*concrete.harmonicData);
        check(projected.root==expected.root&&projected.bass==expected.bass&&projected.intervals==expected.intervals,"saved user candidate realization "+input.id);
        const auto midiBefore=midi::buildClip(before.sequence,midi::ArrangementMode::VoiceLed,midi::ExportScope::CurrentOnly,{}),midiAfter=midi::buildClip(after.sequence,midi::ArrangementMode::VoiceLed,midi::ExportScope::CurrentOnly,{});
        check(midiBefore&&midiAfter,"MIDI semantic export "+input.id);
        const auto bytesBefore=midi::writeToMemory(midiBefore.sequence),bytesAfter=midi::writeToMemory(midiAfter.sequence);
        check(bytesBefore&&bytesAfter&&bytesBefore.bytes==bytesAfter.bytes,"MIDI bytes exact after reopen "+input.id);
        check(reopened.updateProgression(*it,error),error); // No-change save preserves timestamps.
        std::cout<<input.rawChords.front().name<<" user save/reopen/preview/MIDI PASS\n";
    }
    const auto firstPayload=dev::serializeTemplateJson(loaded.templates.front());
    {Sql sql(root/"new-user.db");sql.exec("CREATE TRIGGER fail_data BEFORE INSERT ON progression_data BEGIN SELECT RAISE(ABORT,'injected failure'); END;");}
    auto changed=loaded.templates.front();changed.note="must roll back";
    check(!reopened.updateProgression(changed,error),"write failure injected");
    check(dev::serializeTemplateJson(reopened.loadAll().templates.front())==firstPayload,"failed update rolls back both tables");
    auto extra=inputs.front();extra.id="must-not-exist";check(!reopened.addProgression(extra,error),"failed insert");check(reopened.loadAll().templates.size()==18,"insert rollback no partial row");
    // A real schema1 layout and schema1 payload, without new fields.
    auto historical=fixtures().front();historical.id="legacy_user";historical.sourceType="user";historical.name="旧名字";historical.note="旧备注";historical.tags={"old","中文"};historical.favorite=true;
    historical.createdAt="2019-01-01T00:00:00Z";historical.updatedAt="2020-01-01T00:00:00Z";
    const auto oldPath=root/"old-user.db";std::string oldRaw;
    {Sql old(oldPath);old.exec("CREATE TABLE metadata(key TEXT PRIMARY KEY,value TEXT NOT NULL);CREATE TABLE progressions(id TEXT PRIMARY KEY,payload TEXT NOT NULL);INSERT INTO metadata VALUES('schema_version','1'),('library_type','user'),('library_version','1');");old.insert(historical);oldRaw=old.scalar("SELECT payload FROM progressions");}
    library::UserLibrary oldUser(oldPath);const auto oldLoaded=oldUser.loadAll();check(oldLoaded&&oldLoaded.storageSchemaVersion==1,"legacy schema1 opens");
    // Inject a migration conflict; raw data and version must survive rollback.
    {Sql old(oldPath);old.exec("CREATE TABLE progression_data(dummy TEXT)");}
    check(!oldUser.updateProgression(oldLoaded.templates.front(),error),"migration failure injected");
    {Sql old(oldPath);check(old.scalar("SELECT value FROM metadata WHERE key='schema_version'")=="1","migration version rolled back");check(old.scalar("SELECT payload FROM progressions")==oldRaw,"old raw survives failed migration");old.exec("DROP TABLE progression_data");}
    check(oldUser.updateProgression(oldLoaded.templates.front(),error),error);const auto migrated=oldUser.loadAll();
    check(migrated&&migrated.storageSchemaVersion==2&&migrated.templates.size()==1,"migration complete");
    check(dev::serializeTemplateJson(migrated.templates.front())==dev::serializeTemplateJson(oldLoaded.templates.front()),"old names/tags/styles/intents/favorite/note/timestamps/content retained");
    {Sql old(oldPath);check(old.scalar("SELECT payload FROM progressions")==oldRaw,"old raw unchanged after migration");check(old.scalar("SELECT value FROM metadata WHERE key='schema_version'")!="1","frozen v0.8 rejects schema2 safely");}
    const auto backup=fs::path(oldPath.string()+".v1-backup");check(fs::exists(backup),"one-time v1 backup");const auto stamp=fs::last_write_time(backup);
    check(oldUser.updateProgression(migrated.templates.front(),error),error);check(fs::last_write_time(backup)==stamp,"backup not repeated");
    check(!fs::exists(root/"new-user.db.v1-backup"),"fresh DB has no redundant backup");
}
void factoryRich(const fs::path& root) {
    auto entries=library::canonicalFactoryEntries(fixtures());auto& t=entries.front();
    t.full.front().quality=ChordQuality::Major7;t.full.front().bassInterval=4;t.full.front().intervalMask=0x895;t.full.front().colorMask=4;t.full.front().displaySuffix="maj9";
    std::string error;check(library::compileFactory(root/"rich.db",entries,error,3),error);
    const auto loaded=library::loadFactory(root/"rich.db");check(static_cast<bool>(loaded),loaded.error);
    const auto it=std::find_if(loaded.templates.begin(),loaded.templates.end(),[&](const auto& x){return x.id==t.id;});
    check(it!=loaded.templates.end()&&it->full.front().bassInterval==4&&it->full.front().intervalMask==0x895,"Factory2 typed realization persisted");
    const auto result=midi::previewFromTemplate(*it,{PitchClass::C,it->mode},120);check(static_cast<bool>(result),result.error);
    const auto& c=result.sequence.events.front().chord;check((c.bass-c.root+12)%12==4&&(c.intervals&(1<<2)),"factory inversion and ninth preview");
    auto colorOnly=dev::parseNotationEvent("I",Mode::Major,2);colorOnly.colorMask=4;
    const auto color=realizeContinuation(colorOnly,{PitchClass::C,Mode::Major},2);
    check(color.harmonicData&&chordPitches(*color.harmonicData).intervals==0x095,"standalone color metadata retains chord core");
    auto snap=oldSnapshot(it->id);snap.factoryLibraryVersion=3;snap.candidate.continuation={realizeContinuation(it->full.front(),snap.candidate.key,2)};
    const auto restored=snapshot::deserialize(snapshot::serialize(snap),&loaded);check(restored&&restored.value.candidate.continuation.front().harmonicData.has_value(),"rich Factory2 -> Snapshot3 data transport");
    auto questionable=entries;questionable.front().full.front().quality=ChordQuality::Unknown;
    check(!library::compileFactory(root/"questionable.db",questionable,error,3),"unsupported factory quality requires QUESTIONABLE");
    library::LoadResult v2,v3;libraries(root,v2,v3);
    const auto store=root/"store";check(library::installFactoryPackage(root/"2.db",store,error),error);check(library::installFactoryPackage(root/"3.db",store,error),error);
    auto active=library::loadAvailableFactory(root/"2.db",store);check(active.library.libraryVersion==3,"active V3 independent store");
    {std::ofstream out(store/"active.txt");out<<"2\n";}active=library::loadAvailableFactory(root/"3.db",store);check(active.library.libraryVersion==2,"active switch back to historical2");
}
}
int main(int argc,char** argv) {
    const auto root=fs::temp_directory_path()/("hc-v3-bridge-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {fs::create_directories(root);const std::string group=argc>1?argv[1]:"";
        if(group=="user-smoke")userRestoreSmoke(root);else if(group=="resolver")resolver(root);else if(group=="snapshot")snapshots(root);else if(group=="user")userPersistence(root);else if(group=="factory")factoryRich(root);else throw std::runtime_error("test group required");
        fs::remove_all(root);std::cout<<group<<" "<<checks<<" checks PASS\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<" after "<<checks<<" checks\n";std::error_code ignored;fs::remove_all(root,ignored);return 1;}
}
