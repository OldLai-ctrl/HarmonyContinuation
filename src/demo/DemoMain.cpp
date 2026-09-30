#include "DemoScenario.h"
#include "plugin/RecommendationWorker.h"
#include "session/ProductServices.h"
#include "library/LibraryStore.h"
#include "ui/MainView.h"
#include "preview/OfflinePreviewRenderer.h"
#include "midi/StandardMidiFileWriter.h"
#include "midi/MidiImportWorkflow.h"
#include "snapshot/RecommendationSnapshot.h"
#include "snapshot/EnrichmentSnapshot.h"
#include "benchmark/Benchmark.h"
#include "vstgui/lib/cframe.h"
#include "vstgui/lib/platform/platformfactory.h"
#include "vstgui/lib/platform/win32/win32factory.h"
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <mmsystem.h>
#include <commdlg.h>
#include <chrono>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <string_view>

namespace {
using namespace harmony;
std::optional<std::filesystem::path> savePath(HWND owner,std::wstring name,const wchar_t* filter,const wchar_t* extension) {
    wchar_t autosave[32768]{};
    const auto length=GetEnvironmentVariableW(L"HC_DEMO_AUTOSAVE_DIR",autosave,32768);
    if (length && length<32768) return std::filesystem::path(autosave)/name;
    wchar_t path[32768]{};
    const auto count=std::min<std::size_t>(name.size(),32766);
    std::copy_n(name.begin(),count,path);
    OPENFILENAMEW dialog{}; dialog.lStructSize=sizeof(dialog); dialog.hwndOwner=owner;
    dialog.lpstrFilter=filter; dialog.lpstrFile=path; dialog.nMaxFile=32768;
    dialog.lpstrDefExt=extension; dialog.Flags=OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&dialog)) return std::nullopt;
    return std::filesystem::path(path);
}
std::optional<std::filesystem::path> openSnapshotPath(HWND owner) {
    wchar_t automated[32768]{};
    const auto length=GetEnvironmentVariableW(L"HC_DEMO_SNAPSHOT_FILE",automated,32768);
    if (length && length<32768) return std::filesystem::path(automated);
    wchar_t path[32768]{};
    OPENFILENAMEW dialog{}; dialog.lStructSize=sizeof(dialog); dialog.hwndOwner=owner;
    dialog.lpstrFilter=L"HarmonyContinuation snapshot\0*.hcrec.json\0JSON files\0*.json\0\0";
    dialog.lpstrFile=path; dialog.nMaxFile=32768; dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&dialog)) return std::nullopt;
    return std::filesystem::path(path);
}
class DemoApp {
public:
    explicit DemoApp(std::filesystem::path executable)
        : factoryPath_(executable.parent_path()/"factory.db"),
          userPath_(library::userDatabasePath()),worker_(factoryPath_,userPath_) {}
    ~DemoApp() { stopAudition(); if (frame_) { frame_->close(); frame_=nullptr; main_=nullptr; } }
    bool open(HWND host) {
        hostWindow_=GetParent(host);
        auto* factory=VSTGUI::getPlatformFactory().asWin32Factory();
        if (factory) { factory->disableDirectComposition(); factory->useD2DHardwareRenderer(false); }
        frame_=new VSTGUI::CFrame(VSTGUI::CRect(0,0,1100,900),nullptr);
        harmony::ui::MainView::Actions actions;
        actions.stateChanged=[this](const session::PluginSessionState& next){ applyState(next); };
        actions.save=[this](const ContinuationCandidate& c,const session::SaveMetadata& meta) { return save(c,meta); };
        actions.updateUser=[this](const ProgressionTemplate& t) { return update(t); };
        actions.deleteUser=[this](const std::string& id) { return remove(id); };
        actions.audition=[this](const ContinuationCandidate& c) { audition(c); };
        actions.exportMidi=[this](const ContinuationCandidate& c) { return exportRecommendation(c); };
        actions.saveSnapshot=[this](const ContinuationCandidate& c) { return saveRecommendationSnapshot(c); };
        actions.exportLibraryMidi=[this](const ProgressionTemplate& t) { return exportLibrary(t); };
        actions.benchmarkSelect=[this](const ContinuationCandidate& c) { ratingCandidate_=c; };
        actions.auditionEnrichment=[this](const enrichment::EnrichmentCandidate& c) { auditionEnrichment(c); };
        actions.exportEnrichmentMidi=[this](const enrichment::EnrichmentCandidate& c) { return exportEnrichment(c); };
        actions.midiPayload=[this](const ContinuationCandidate& c){return midi::candidatePayload(exportContext(),c);};
        actions.enrichmentMidiPayload=[this](const enrichment::EnrichmentCandidate& c){return midi::candidatePayload(exportContext(),c);};
        actions.saveMidiPayload=[this](const midi::MidiClipPayload& payload){return savePayload(payload);};
        actions.importMidi=[this](bool open,const std::filesystem::path& path){importMidi(open,path);};
        actions.saveEnrichmentSnapshot=[this](const enrichment::EnrichmentCandidate& c) { return saveEnrichment(c); };
        main_=new harmony::ui::MainView(VSTGUI::CRect(0,0,1100,900),std::move(actions));
        frame_->addView(main_);
        if (!frame_->open(host,VSTGUI::PlatformType::kHWND)) { frame_=nullptr; main_=nullptr; return false; }
        main_->setHostText("HarmonyContinuation Demo · 120 BPM · simulated transport");
        loadLibrary(); return true;
    }
    void resize(int width,int height) {
        if(width<1||height<1)return;
        if(frame_)frame_->setSize(width,height);
        if(main_)main_->resizeLayout(width,height);
        state_.editorWidth=static_cast<std::uint32_t>(std::clamp(width,900,2200));
        state_.editorHeight=static_cast<std::uint32_t>(std::clamp(height,640,1400));
        if(main_)main_->setEditorSizeState(static_cast<int>(state_.editorWidth),static_cast<int>(state_.editorHeight));
    }
    void simulateScale(double scale) { if(main_)main_->setSimulatedContentScale(scale); }
    bool zoomSmoke(std::uint32_t percent) {if(!main_)return false;main_->setUserZoom(percent);return main_->runZoomSmoke();}
    bool midiWorkflowSmoke(const std::filesystem::path& fixture) {
        if(!main_||!main_->runMidiWorkflowSmoke())return false;
        if(fixture.empty())return true;
        importMidi(false,fixture);if(state_.imported.events.size()!=4||state_.imported.events.back().openEnded||!state_.constraints.melody.empty())return false;
        const auto before=session::serialize(state_);importMidi(false,fixture.parent_path()/"missing.mid");
        if(session::serialize(state_)!=before)return false;
        importMidi(true,fixture);return state_.imported.events.size()==4&&state_.imported.events.back().openEnded&&!state_.imported.events.back().durationQN;
    }
    bool load(char which) {
        stopAudition();
        benchmarkId_.clear();benchmarkDescription_.clear();ratingCandidate_.reset();
        snapshotLoaded_=false;
        if (main_) main_->setSnapshotMode(false);
        if (which<'A' || which>'H') return false;
        auto path=std::filesystem::path(HC_DEMO_FIXTURE_DIR)/(std::string("case_")+static_cast<char>(which-'A'+'a')+".json");
        auto loaded=demo::loadScenario(path);
        if (!loaded) { if (main_) main_->setDropReport("Scenario error",loaded.error); return false; }
        scenario_=std::move(loaded.scenario);
        const auto width=state_.editorWidth,height=state_.editorHeight;
        state_={};state_.editorWidth=width;state_.editorHeight=height;
        state_.forcedKey=scenario_.forcedKey; state_.style=scenario_.style; state_.intent=scenario_.intent;
        state_.constraints=scenario_.constraints;state_.tendency=scenario_.tendency;
        state_.meterNumerator=scenario_.meterNumerator; state_.meterDenominator=scenario_.meterDenominator;
        state_.imported.replace(std::move(scenario_.chords),TimelineCoordinateMode::RelativeToSelection);
        projectQN_=state_.imported.events.front().startQN; playing_=false;
        if (main_) { main_->setSessionState(state_); main_->setPlaybackPosition(projectQN_,false);
            main_->setAnalysis({}); main_->setMatches({},"Analyzing…"); main_->setRecommendations({});
            main_->setHostText(scenario_.name+" · "+std::to_string(static_cast<int>(scenario_.tempo))+" BPM · simulated transport"); }
        submit(); return true;
    }
    bool loadBenchmark(const std::filesystem::path& path) {
        try {
            const auto item=benchmark::loadCase(path);
            stopAudition();snapshotLoaded_=false;ratingCandidate_.reset();benchmarkId_=item.id;
            benchmarkDescription_=item.name+"\n"+item.notes+"\n";
            if(main_)main_->setSnapshotMode(false);
            scenario_=item.scenario;
            const auto width=state_.editorWidth,height=state_.editorHeight;
            state_={};state_.editorWidth=width;state_.editorHeight=height;
            state_.forcedKey=scenario_.forcedKey;state_.style=scenario_.style;state_.intent=scenario_.intent;
            state_.meterNumerator=scenario_.meterNumerator;state_.meterDenominator=scenario_.meterDenominator;
            state_.imported.replace(scenario_.chords,TimelineCoordinateMode::RelativeToSelection);
            projectQN_=state_.imported.events.front().startQN;playing_=false;
            analysis_={};recommendations_={};
            if(main_){main_->setSessionState(state_);main_->setPlaybackPosition(projectQN_,false);
                main_->setAnalysis({});main_->setMatches({},"Analyzing…");main_->setRecommendations({});
                main_->setHostText("BENCHMARK "+benchmarkId_+" · "+scenario_.name);}
            submit();return true;
        }catch(const std::exception& e){if(main_)main_->setActionStatus(std::string("Benchmark: ")+e.what());return false;}
    }
    std::string benchmarkId() const {return benchmarkId_;}
    std::string benchmarkInput() const {
        std::string out=benchmarkDescription_;for(const auto& c:state_.imported.events){out+=c.name;
            out+="   ";out+=c.durationQN?std::to_string(*c.durationQN)+" QN":"OPEN";out+='\n';}
        return out;
    }
    std::string selectedBenchmarkCandidate() const {
        if(!ratingCandidate_)return "Select a recommendation";
        std::string out=std::string(intentName(ratingCandidate_->intent))+" · ";
        for(const auto& c:ratingCandidate_->continuation){if(out.back()!=' ')out+=" → ";out+=c.label;}
        return out;
    }
    std::string saveBenchmarkRating(std::array<int,5> scores,std::string verdict,std::string issue,std::string note) {
        try {
        if(benchmarkId_.empty()||!ratingCandidate_)return "Select a benchmark candidate first";
        benchmark::Rating rating;rating.benchmarkId=benchmarkId_;
        rating.candidateFingerprint=session::continuationFingerprint(*ratingCandidate_);
        rating.naturalness=scores[0];rating.intentFit=scores[1];rating.rhythmFit=scores[2];
        rating.distinctiveness=scores[3];rating.usability=scores[4];
        rating.verdict=std::move(verdict);rating.issueCategory=std::move(issue);rating.note=std::move(note);
        rating.libraryVersion=static_cast<int>(state_.factoryLibraryVersion);
        rating.recommendation=snapshot::capture(state_.imported,*ratingCandidate_,recommendations_.matches,
            scenario_.tempo,scenario_.meterNumerator,scenario_.meterDenominator,
            state_.forcedKey?state_.forcedKey:std::optional(ratingCandidate_->key),state_.style,state_.intent);
        std::string error;
        if(!benchmark::saveRating(rating,HC_BENCH_RATING_DIR,error))return "Rating: "+error;
        return "Rating saved: "+benchmark::ratingFilename(rating);
        }catch(const std::exception& e){return std::string("Rating: ")+e.what();}
    }
    std::string exportSelectedSnapshot() {
        return ratingCandidate_?saveRecommendationSnapshot(*ratingCandidate_):"Select a benchmark candidate first";
    }
    void tick() {
        if (frame_) frame_->idle();
        if (auto result=worker_.takeLatest()) {
            if (!snapshotLoaded_) {
            ratingCandidate_.reset();
            analysis_=std::move(result->analysis); recommendations_=std::move(result->recommendations);
            if (main_) { main_->setAnalysis(analysis_); main_->setMatches(recommendations_.matches,
                result->error.empty()?"Ready":result->error); main_->setRecommendations(recommendations_);
                main_->setEnrichments(result->enrichments);
                main_->setWorkerStatus(result->generation,result->computationMs,false,result->factoryCount,result->userCount); }
            if(main_&&!midiImportSummary_.empty()){main_->setActionStatus(midiImportSummary_+" · "+std::string(localization::text(state_.locale,"midi.key"))+" "+
                (analysis_.selectedKey?formatKey(analysis_.selectedKey->key):"?"));midiImportSummary_.clear();}
            }
        }
        const auto now=std::chrono::steady_clock::now();
        const auto seconds=std::chrono::duration<double>(now-lastTick_).count(); lastTick_=now;
        if (playing_ && !state_.imported.events.empty()) {
            projectQN_+=std::clamp(seconds,0.0,0.2)*scenario_.tempo/60.0;
            if (main_) main_->setPlaybackPosition(projectQN_,true);
        }
        if (!auditionId_.empty()) {
            const auto elapsed=std::chrono::duration<double>(now-auditionStart_).count();
            if (elapsed>=auditionSeconds_+0.15) stopAudition();
            else if (main_) main_->setPreviewPosition(auditionId_,elapsed*scenario_.tempo/60.0,auditionQN_);
        }
    }
    void seek(double qn) { projectQN_=qn; if (main_) main_->setPlaybackPosition(projectQN_,playing_); }
    bool togglePlay() { playing_=!playing_; if (main_) main_->setPlaybackPosition(projectQN_,playing_); return playing_; }
    double projectQN() const { return projectQN_; }
    void showStatus(std::string status) { if (main_) main_->setActionStatus(std::move(status)); }
    std::string exportCurrent() {
        const auto built=preview::buildSequence(state_.imported,nullptr,scenario_.tempo);
        if (!built) return "Current MIDI: "+built.error;
        const auto key=state_.forcedKey?state_.forcedKey:
            (analysis_.selectedKey?std::optional(analysis_.selectedKey->key):std::nullopt);
        return exportSequence(built.sequence,nullptr,midi::ExportScope::CurrentOnly,key);
    }
    std::string openSnapshot() {
        const auto path=openSnapshotPath(hostWindow_);
        if (!path) return "Snapshot open cancelled";
        const auto decoded=snapshot::loadFile(*path);
        if (!decoded) return "Snapshot load: "+decoded.error;
        stopAudition(); snapshotLoaded_=true; playing_=false;
        const auto& snap=decoded.value;
        const auto width=state_.editorWidth,height=state_.editorHeight;
        state_={};state_.editorWidth=width;state_.editorHeight=height;
        state_.imported=snap.imported; state_.forcedKey=snap.key;
        state_.style=snap.style; state_.intent=snap.intent;
        state_.constraints=snap.candidate.constraints;
        state_.meterNumerator=snap.meterNumerator; state_.meterDenominator=snap.meterDenominator;
        scenario_.name=path->filename().string(); scenario_.tempo=snap.tempoBPM;
        scenario_.meterNumerator=snap.meterNumerator; scenario_.meterDenominator=snap.meterDenominator;
        projectQN_=0;
        analysis_=analyzeHarmony(state_.imported.events,state_.analysisContext());
        recommendations_={};
        const auto group=snap.candidate.intent==PhraseIntent::Develop?1:snap.candidate.intent==PhraseIntent::Loop?2:
            snap.candidate.intent==PhraseIntent::Color?3:0;
        recommendations_.groups[group].push_back(snap.candidate);
        if (snap.match) recommendations_.matches.push_back(*snap.match);
        if (main_) {
            main_->setSnapshotMode(true); main_->setSessionState(state_);
            main_->setPlaybackPosition(projectQN_,false); main_->setAnalysis(analysis_);
            main_->setMatches(recommendations_.matches,"Frozen recommendation snapshot");
            main_->setRecommendations(recommendations_);
            main_->setWorkerStatus(0,0,false,0,0);
            main_->setHostText("Snapshot · "+scenario_.name+" · "+std::to_string(static_cast<int>(scenario_.tempo))+" BPM");
        }
        return "Snapshot loaded";
    }
private:
    HWND hostWindow_{};
    std::filesystem::path auditionFile_;
    std::string auditionId_;
    double auditionSeconds_{},auditionQN_{};
    std::chrono::steady_clock::time_point auditionStart_{};
    std::filesystem::path factoryPath_,userPath_;
    plugin::RecommendationWorker worker_;
    VSTGUI::CFrame* frame_{};
    harmony::ui::MainView* main_{};
    session::PluginSessionState state_;
    demo::Scenario scenario_;
    HarmonicAnalysisResult analysis_;
    RecommendationSet recommendations_;
    bool playing_{};
    bool snapshotLoaded_{};
    std::string benchmarkId_;
    std::string benchmarkDescription_;
    std::string midiImportSummary_;
    std::optional<ContinuationCandidate> ratingCandidate_;
    double projectQN_{};
    std::chrono::steady_clock::time_point lastTick_{std::chrono::steady_clock::now()};
    void stopAudition() {
        PlaySoundW(nullptr,nullptr,0);
        auditionId_.clear(); auditionSeconds_=auditionQN_=0;
        if (main_) main_->setPreviewPosition({},0,0);
        if (hostWindow_) SetTimer(hostWindow_,1,playing_?50:250,nullptr);
        if (!auditionFile_.empty()) { std::error_code ec; std::filesystem::remove(auditionFile_,ec); auditionFile_.clear(); }
    }
    void audition(const ContinuationCandidate& candidate) {
        if (candidate.id==auditionId_) { stopAudition(); return; }
        const auto built=preview::buildSequence(state_.imported,&candidate,scenario_.tempo);
        playPreview(candidate.id,built);
    }
    void auditionEnrichment(const enrichment::EnrichmentCandidate& candidate) {
        if (candidate.id==auditionId_) { stopAudition(); return; }
        ImportedProgressionSession transformed;
        if (!transformed.replace(candidate.progression,TimelineCoordinateMode::RelativeToSelection)) return;
        playPreview(candidate.id,preview::buildSequence(transformed,nullptr,scenario_.tempo,candidate.constraints));
    }
    void playPreview(const std::string& id,const preview::BuildResult& built) {
        stopAudition();
        if (!built) { if (main_) main_->setDropReport("Preview",built.error,false); return; }
        const auto audio=preview::renderOffline(built.sequence,48000);
        if (audio.left.empty()) { if (main_) main_->setDropReport("Preview","Render failed",false); return; }
        auditionFile_=std::filesystem::temp_directory_path()/
            ("HarmonyContinuationDemo-"+std::to_string(GetCurrentProcessId())+".wav");
        std::string error;
        if (!preview::writeWav16(audio,auditionFile_,error) ||
            !PlaySoundW(auditionFile_.c_str(),nullptr,SND_ASYNC|SND_FILENAME|SND_NODEFAULT)) {
            if (main_) main_->setDropReport("Preview",error.empty()?"Audio playback failed":error,false);
            stopAudition(); return;
        }
        auditionId_=id; auditionQN_=built.sequence.totalQN;
        auditionSeconds_=audio.left.size()/audio.sampleRate;
        auditionStart_=std::chrono::steady_clock::now();
        if (main_) main_->setPreviewPosition(auditionId_,0,auditionQN_);
        if (hostWindow_) SetTimer(hostWindow_,1,40,nullptr);
    }
    void submit(bool rankingOnly=false) {
        if (snapshotLoaded_) return;
        if (state_.imported.events.empty()) return;
        RecommendationRequest request{state_.style,state_.intent,state_.constraints,state_.tendency};
        const auto generation=worker_.submit(state_.imported.events,state_.analysisContext(),request,
            rankingOnly,state_.imported.revision);
        if (main_) main_->setWorkerStatus(generation,0,true,0,0);
    }
    void applyState(const session::PluginSessionState& next) {
        const auto recompute=session::recomputeScope(state_,next);
        const auto width=state_.editorWidth,height=state_.editorHeight;
        state_=next;state_.editorWidth=width;state_.editorHeight=height;
        if (main_) main_->setSessionState(state_);
        if (!snapshotLoaded_ && recompute!=session::RecomputeScope::None) submit(recompute==session::RecomputeScope::Ranking);
    }
    std::string exportSequence(const preview::Sequence& sequence,const ContinuationCandidate* candidate,
                               midi::ExportScope scope,std::optional<KeySignature> key) {
        const auto built=midi::buildClip(sequence,midi::ArrangementMode::VoiceLed,scope,
            {scenario_.meterNumerator,scenario_.meterDenominator},key,candidate?std::optional(candidate->intent):std::nullopt);
        if (!built) return "MIDI export: "+built.error;
        const auto name=midi::suggestedFilename(candidate?std::optional(candidate->intent):std::nullopt,key,1);
        const auto path=savePath(hostWindow_,std::wstring(name.begin(),name.end()),
            L"MIDI files\0*.mid\0\0",L"mid");
        if (!path) return "MIDI export cancelled";
        const auto payload=midi::makePayload(built.sequence,name);
        std::string error;
        if (payload.smfBytes.empty()||!midi::writeToFile(payload.smfBytes,*path,error))
            return "MIDI export: "+(error.empty()?"invalid payload":error);
        return "MIDI saved: "+path->filename().string();
    }
    midi::CandidateExportContext exportContext() const {
        const auto key=state_.forcedKey?state_.forcedKey:analysis_.selectedKey?std::optional(analysis_.selectedKey->key):std::nullopt;
        return {state_.imported,scenario_.tempo,{scenario_.meterNumerator,scenario_.meterDenominator},key};
    }
    std::string savePayload(const midi::MidiClipPayload& payload) {
        const auto text=[this](const char* key){return std::string(localization::text(state_.locale,key));};
        const auto path=savePath(hostWindow_,std::wstring(payload.suggestedFilename.begin(),payload.suggestedFilename.end()),L"MIDI files\0*.mid\0\0",L"mid");
        if(!path)return text("midi.cancelled");std::string error;
        if(!midi::writeToFile(payload.smfBytes,*path,error))return text("midi.saveFailed");
        return text("midi.saved");
    }
    std::string exportRecommendation(const ContinuationCandidate& candidate) {
        const auto result=midi::candidatePayload(exportContext(),candidate);
        return result?savePayload(result.payload):std::string(localization::text(state_.locale,"midi.saveFailed"));
    }
    std::string exportEnrichment(const enrichment::EnrichmentCandidate& candidate) {
        const auto result=midi::candidatePayload(exportContext(),candidate);
        return result?savePayload(result.payload):std::string(localization::text(state_.locale,"midi.saveFailed"));
    }
    void importMidi(bool open,const std::filesystem::path& supplied) {
        try {
            const auto path=supplied.empty()?midi::chooseMidiFile(hostWindow_):std::optional(supplied);if(!path)return;
            const auto result=midi::importFile(*path,open);auto next=state_;
            const auto text=[this](const char* key){return std::string(localization::text(state_.locale,key));};
            if(!midi::applyImport(result,next.imported)){main_->setActionStatus(text(result.midi.status==midi::ReadStatus::Unsupported?"midi.unsupported":"midi.failed"));return;}
            stopAudition();snapshotLoaded_=false;main_->setSnapshotMode(false);next.tab=session::Tab::Recommend;
            if(!result.midi.file.meters.empty()) {const auto m=result.midi.file.meters.front();
                if(m.numerator<=32&&m.denominator<=32){next.meterNumerator=m.numerator;next.meterDenominator=m.denominator;
                    scenario_.meterNumerator=m.numerator;scenario_.meterDenominator=m.denominator;}}
            applyState(next);
            const auto warnings=result.midi.file.warnings.size()+result.extraction.warnings.size();
            auto summary=text("midi.recognized")+" "+std::to_string(result.extraction.chords.size())+" · "+text("midi.track")+" "+result.extraction.selectedTrackName;
            if(warnings)summary+=" · "+text("midi.uncertain")+" "+std::to_string(warnings);
            std::string details;for(const auto& w:result.midi.file.warnings)details+=w+'\n';for(const auto& w:result.extraction.warnings)details+=w+'\n';
            for(const auto& slice:result.extraction.slices)details+=slice.chord+" confidence="+std::to_string(slice.confidence)+'\n';
            midiImportSummary_=summary;main_->setDropReport(summary,details,false);main_->setActionStatus(summary);
        } catch(...){main_->setActionStatus(std::string(localization::text(state_.locale,"midi.failed")));}
    }
    std::string saveEnrichment(const enrichment::EnrichmentCandidate& candidate) {
        try {
            const auto key=state_.forcedKey?state_.forcedKey:
                analysis_.selectedKey?std::optional(analysis_.selectedKey->key):std::nullopt;
            const auto snap=snapshot::captureEnrichment(state_.imported,candidate,scenario_.tempo,
                scenario_.meterNumerator,scenario_.meterDenominator,key,state_.style);
            const auto filename=candidate.id+".hcenrich.json";
            const auto path=savePath(hostWindow_,std::wstring(filename.begin(),filename.end()),
                L"Enrichment snapshot\0*.hcenrich.json\0JSON files\0*.json\0\0",L"json");
            if (!path) return "Snapshot save cancelled";
            std::string error;
            if (!snapshot::saveFile(snap,*path,error)) return "Snapshot save: "+error;
            return "Snapshot saved: "+path->filename().string();
        } catch (const std::exception& e) {return std::string("Snapshot save: ")+e.what();}
    }
    std::string exportLibrary(const ProgressionTemplate& item) {
        const auto key=state_.forcedKey.value_or(analysis_.selectedKey?analysis_.selectedKey->key:
            KeySignature{item.mode==Mode::Major?PitchClass::C:PitchClass::A,item.mode});
        const auto preview=midi::previewFromTemplate(item,key,scenario_.tempo);
        if (!preview) return "Library MIDI: "+preview.error;
        const auto clip=midi::buildClip(preview.sequence,midi::ArrangementMode::VoiceLed,midi::ExportScope::CurrentOnly,
            {item.meterNumerator,item.meterDenominator},key,item.intent);
        if (!clip) return "Library MIDI: "+clip.error;
        const auto name=midi::suggestedFilename(item.intent,key,1);
        const auto path=savePath(hostWindow_,std::wstring(name.begin(),name.end()),L"MIDI files\0*.mid\0\0",L"mid");
        if (!path) return "Library MIDI cancelled";
        const auto payload=midi::makePayload(clip.sequence,name);
        std::string error;
        if (payload.smfBytes.empty()||!midi::writeToFile(payload.smfBytes,*path,error))
            return "Library MIDI: "+(error.empty()?"invalid payload":error);
        return "Library MIDI saved: "+path->filename().string();
    }
    std::string saveRecommendationSnapshot(const ContinuationCandidate& candidate) {
        try {
            const auto snap=snapshot::capture(state_.imported,candidate,recommendations_.matches,
                scenario_.tempo,scenario_.meterNumerator,scenario_.meterDenominator,
                state_.forcedKey?state_.forcedKey:std::optional(candidate.key),state_.style,state_.intent);
            auto name=midi::suggestedFilename(candidate.intent,candidate.key,1);
            name.replace(name.size()-4,4,".hcrec.json");
            const auto path=savePath(hostWindow_,std::wstring(name.begin(),name.end()),
                L"HarmonyContinuation snapshot\0*.hcrec.json\0JSON files\0*.json\0\0",L"json");
            if (!path) return "Snapshot save cancelled";
            std::string error;
            if (!snapshot::saveFile(snap,*path,error)) return "Snapshot save: "+error;
            return "Snapshot saved: "+path->filename().string();
        } catch (const std::exception& e) {return std::string("Snapshot save: ")+e.what();}
    }
    void loadLibrary() {
        auto selected=library::loadAvailableFactory(factoryPath_); auto& factory=selected.library;
        if(factory) state_.factoryLibraryVersion=factory.libraryVersion;
        if(main_) main_->setSessionState(state_);
        auto user=library::UserLibrary(userPath_).loadAll();
        if (main_) main_->setLibrary(std::move(factory.templates),std::move(user.templates),
            !factory?factory.error:!user?user.error:selected.warning);
    }
    std::string save(const ContinuationCandidate& c,const session::SaveMetadata& meta) {
        if (!userPath_.parent_path().empty()) std::filesystem::create_directories(userPath_.parent_path());
        library::UserLibrary library(userPath_); std::string error;
        if (!session::saveRecommendation(library,state_.imported,c,meta,error)) return "Save failed: "+error;
        worker_.invalidateLibrary(); loadLibrary(); submit(); return "Saved to User Library";
    }
    std::string update(const ProgressionTemplate& item) {
        std::string error; if (!library::UserLibrary(userPath_).updateProgression(item,error)) return "Update failed: "+error;
        worker_.invalidateLibrary(); loadLibrary(); submit(); return "User progression updated";
    }
    std::string remove(const std::string& id) {
        std::string error; if (!library::UserLibrary(userPath_).removeProgression(id,error)) return "Delete failed: "+error;
        worker_.invalidateLibrary(); loadLibrary(); submit(); return "User progression deleted";
    }
};
std::unique_ptr<DemoApp> app;
HWND slider{},playButton{},demoContent{},caseCombo{},currentButton{},snapshotButton{},statusLabel{};
bool benchmarkMode{};
std::vector<std::filesystem::path> benchmarkCases;
std::size_t benchmarkIndex{};
HWND benchmarkPanel{},benchmarkIdLabel{},benchmarkInputLabel{},benchmarkSelectedLabel{},benchmarkNote{};
std::array<HWND,5> benchmarkScores{};
HWND benchmarkVerdict{},benchmarkIssue{};
std::wstring wide(std::string_view value){
    if(value.empty())return {};
    const auto count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),nullptr,0);
    if(count<=0)return L"?";
    std::wstring out(static_cast<std::size_t>(count),L'\0');
    MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),out.data(),count);
    return out;
}
std::string utf8(std::wstring_view value){
    if(value.empty())return {};
    const auto count=WideCharToMultiByte(CP_UTF8,0,value.data(),static_cast<int>(value.size()),nullptr,0,nullptr,nullptr);
    std::string out(static_cast<std::size_t>(count),'\0');
    WideCharToMultiByte(CP_UTF8,0,value.data(),static_cast<int>(value.size()),out.data(),count,nullptr,nullptr);
    return out;
}
bool loadBenchmarkIndex(std::size_t index){
    if(!app||index>=benchmarkCases.size()||!app->loadBenchmark(benchmarkCases[index]))return false;
    benchmarkIndex=index;
    if(benchmarkIdLabel)SetWindowTextW(benchmarkIdLabel,wide(app->benchmarkId()+"  "+
        std::to_string(index+1)+" / "+std::to_string(benchmarkCases.size())).c_str());
    if(benchmarkInputLabel)SetWindowTextW(benchmarkInputLabel,wide(app->benchmarkInput()).c_str());
    if(benchmarkSelectedLabel)SetWindowTextW(benchmarkSelectedLabel,L"Select a recommendation");
    return true;
}
LRESULT CALLBACK windowProc(HWND hwnd,UINT message,WPARAM w,LPARAM l) {
    switch(message) {
        case WM_GETMINMAXINFO: {
            auto* limits=reinterpret_cast<MINMAXINFO*>(l);
            RECT minimum{0,0,benchmarkMode?1180:920,benchmarkMode?850:705};
            RECT maximum{0,0,benchmarkMode?2500:2220,1465};
            AdjustWindowRectEx(&minimum,GetWindowLongW(hwnd,GWL_STYLE),FALSE,GetWindowLongW(hwnd,GWL_EXSTYLE));
            AdjustWindowRectEx(&maximum,GetWindowLongW(hwnd,GWL_STYLE),FALSE,GetWindowLongW(hwnd,GWL_EXSTYLE));
            limits->ptMinTrackSize={minimum.right-minimum.left,minimum.bottom-minimum.top};
            limits->ptMaxTrackSize={maximum.right-maximum.left,maximum.bottom-maximum.top};
            return 0;
        }
        case WM_SIZE: {
            const int width=LOWORD(l),height=HIWORD(l);
            if(width<1||height<1)return 0;
            if(benchmarkPanel)MoveWindow(benchmarkPanel,10,55,250,std::max(1,height-65),TRUE);
            const int contentX=benchmarkMode?270:10;
            const int contentWidth=std::max(1,width-(benchmarkMode?280:20));
            if(demoContent)MoveWindow(demoContent,contentX,55,contentWidth,std::max(1,height-65),TRUE);
            const int comboWidth=std::min(285,std::max(180,width/4));
            if(caseCombo)MoveWindow(caseCombo,12,8,comboWidth,300,TRUE);
            if(playButton)MoveWindow(playButton,benchmarkMode?335:comboWidth+19,8,65,32,TRUE);
            const int sliderX=benchmarkMode?410:comboWidth+92,sliderRight=benchmarkMode?width-24:width-248;
            if(slider)MoveWindow(slider,sliderX,4,std::max(80,sliderRight-sliderX),40,TRUE);
            if(currentButton)MoveWindow(currentButton,width-240,8,112,32,TRUE);
            if(snapshotButton)MoveWindow(snapshotButton,width-122,8,112,32,TRUE);
            if(statusLabel){MoveWindow(statusLabel,benchmarkMode?12:930,14,benchmarkMode?300:180,25,TRUE);
                ShowWindow(statusLabel,benchmarkMode||width>=1080?SW_SHOW:SW_HIDE);}
            if(app)app->resize(contentWidth,height-65);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(w)==101 && HIWORD(w)==CBN_SELCHANGE) {
                const auto index=SendMessageW(reinterpret_cast<HWND>(l),CB_GETCURSEL,0,0);
                if (app && index>=0 && app->load(static_cast<char>('A'+index))) {
                    SendMessageW(slider,TBM_SETPOS,TRUE,0); SetWindowTextW(playButton,L"Play"); SetTimer(hwnd,1,250,nullptr);
                }
            } else if (LOWORD(w)==102 && app) {
                const bool playing=app->togglePlay(); SetWindowTextW(playButton,playing?L"Pause":L"Play");
                SetTimer(hwnd,1,playing?50:250,nullptr);
            } else if (LOWORD(w)==104 && app) {
                app->showStatus(app->exportCurrent());
            } else if (LOWORD(w)==105 && app) {
                app->showStatus(app->openSnapshot());
            } else if(benchmarkMode&&app&&LOWORD(w)>=201&&LOWORD(w)<=203&&!benchmarkCases.empty()){
                std::size_t next=benchmarkIndex;
                if(LOWORD(w)==201)next=(next+benchmarkCases.size()-1)%benchmarkCases.size();
                else if(LOWORD(w)==202)next=(next+1)%benchmarkCases.size();
                else {static std::mt19937 random{std::random_device{}()};
                    next=std::uniform_int_distribution<std::size_t>(0,benchmarkCases.size()-1)(random);}
                if(loadBenchmarkIndex(next)){SetWindowTextW(playButton,L"Play");SetTimer(hwnd,1,250,nullptr);}
            } else if(benchmarkMode&&app&&LOWORD(w)==204){
                std::array<int,5> scores{};
                for(std::size_t i=0;i<scores.size();++i)scores[i]=static_cast<int>(SendMessageW(benchmarkScores[i],CB_GETCURSEL,0,0))+1;
                constexpr const char* verdicts[]{"KEEP","QUESTIONABLE","BAD"};
                constexpr const char* issues[]{"Harmony","Matching","Ranking","Intent","Rhythm","Diversity",
                    "Voicing","Sound","UI","Other"};
                const auto verdict=std::clamp<int>(static_cast<int>(SendMessageW(benchmarkVerdict,CB_GETCURSEL,0,0)),0,2);
                const auto issue=std::clamp<int>(static_cast<int>(SendMessageW(benchmarkIssue,CB_GETCURSEL,0,0)),0,9);
                wchar_t note[2049]{};GetWindowTextW(benchmarkNote,note,2049);
                app->showStatus(app->saveBenchmarkRating(scores,verdicts[verdict],issues[issue],utf8(note)));
            } else if(benchmarkMode&&app&&LOWORD(w)==205){
                app->showStatus(app->exportSelectedSnapshot());
            }
            return 0;
        case WM_HSCROLL:
            if (reinterpret_cast<HWND>(l)==slider && app) app->seek(static_cast<double>(SendMessageW(slider,TBM_GETPOS,0,0))/4.0);
            return 0;
        case WM_TIMER:
            if (app) { app->tick(); SendMessageW(slider,TBM_SETPOS,FALSE,static_cast<LPARAM>(app->projectQN()*4));
                if(benchmarkMode&&benchmarkSelectedLabel)SetWindowTextW(benchmarkSelectedLabel,
                    wide(app->selectedBenchmarkCandidate()).c_str()); }
            return 0;
        case WM_DESTROY: KillTimer(hwnd,1); app.reset(); PostQuitMessage(0); return 0;
        default: return DefWindowProcW(hwnd,message,w,l);
    }
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show) {
    VSTGUI::initPlatform(instance);
    INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_BAR_CLASSES}; InitCommonControlsEx(&controls);
    WNDCLASSW wc{}; wc.lpfnWndProc=windowProc; wc.hInstance=instance; wc.lpszClassName=L"HarmonyContinuationDemoWindow";
    wc.hCursor=LoadCursorW(nullptr,IDC_ARROW); RegisterClassW(&wc);
    auto hwnd=CreateWindowExW(0,wc.lpszClassName,L"HarmonyContinuation Demo · offline",WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,CW_USEDEFAULT,1125,1000,nullptr,nullptr,instance,nullptr);
    if (!hwnd) return 1;
    auto combo=CreateWindowExW(0,L"COMBOBOX",nullptr,WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST|WS_VSCROLL,
        12,8,285,300,hwnd,reinterpret_cast<HMENU>(101),instance,nullptr);
    caseCombo=combo;
    const wchar_t* cases[]{L"Case A · C → Am → Dm",L"Case B · C → Am → A7 → Dm",L"Case C · Dm7 → G7",
        L"Case D · C → F → Fm",L"Case E · C → G → Am → F",L"Case F · A minor",
        L"Case G · anomalous F#",L"Case H · ambiguous key"};
    for (auto value:cases) SendMessageW(combo,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(value));
    playButton=CreateWindowExW(0,L"BUTTON",L"Play",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,305,8,65,32,hwnd,
        reinterpret_cast<HMENU>(102),instance,nullptr);
    slider=CreateWindowExW(0,TRACKBAR_CLASSW,L"Project QN",WS_CHILD|WS_VISIBLE|TBS_HORZ,380,4,305,40,hwnd,
        reinterpret_cast<HMENU>(103),instance,nullptr);
    SendMessageW(slider,TBM_SETRANGE,TRUE,MAKELPARAM(0,256));
    currentButton=CreateWindowExW(0,L"BUTTON",L"Current MIDI",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,690,8,112,32,hwnd,
        reinterpret_cast<HMENU>(104),instance,nullptr);
    snapshotButton=CreateWindowExW(0,L"BUTTON",L"Open Snapshot",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,808,8,112,32,hwnd,
        reinterpret_cast<HMENU>(105),instance,nullptr);
    statusLabel=CreateWindowExW(0,L"STATIC",L"120 BPM · simulated",WS_CHILD|WS_VISIBLE,930,14,180,25,hwnd,nullptr,instance,nullptr);
    auto content=CreateWindowExW(0,L"STATIC",nullptr,WS_CHILD|WS_VISIBLE,10,55,1100,900,hwnd,nullptr,instance,nullptr);
    demoContent=content;
    int argc{}; auto args=CommandLineToArgvW(GetCommandLineW(),&argc);
    char initial='D'; int requestedWidth=1100,requestedHeight=900; double scale=1;
    std::filesystem::path resizeSmokeReport,benchmarkSmokeReport,zoomSmokeReport,midiSmokeReport,midiSmokeInput;
    std::string initialBenchmarkId;
    for (int i=1;i<argc;++i) {
        const std::wstring_view key(args[i]);
        if(key==L"--benchmark")benchmarkMode=true;
        else if(key==L"--benchmark-case"&&i+1<argc){benchmarkMode=true;initialBenchmarkId=utf8(args[++i]);}
        else if(key==L"--benchmark-smoke"&&i+1<argc){benchmarkMode=true;benchmarkSmokeReport=args[++i];}
        else if(key==L"--zoom-smoke"&&i+1<argc)zoomSmokeReport=args[++i];
        else if(key==L"--midi-workflow-smoke"&&i+1<argc)midiSmokeReport=args[++i];
        else if(key==L"--midi-smoke-input"&&i+1<argc)midiSmokeInput=args[++i];
        else if (key==L"--case" && i+1<argc && wcslen(args[i+1])==1)initial=static_cast<char>(args[++i][0]);
        else if(key==L"--size"&&i+1<argc) {
            int width{},height{};
            if(swscanf_s(args[++i],L"%dx%d",&width,&height)==2) {
                requestedWidth=std::clamp(width,900,2200);requestedHeight=std::clamp(height,640,1400);
            }
        } else if(key==L"--scale"&&i+1<argc) {
            double value{};if(swscanf_s(args[++i],L"%lf",&value)==1&&std::isfinite(value))scale=std::clamp(value,1.,2.);
        } else if(key==L"--resize-smoke"&&i+1<argc)resizeSmokeReport=args[++i];
    }
    if (args) LocalFree(args);
    if(benchmarkMode){
        try{benchmarkCases=benchmark::caseFiles(HC_BENCH_CASE_DIR);}
        catch(...){DestroyWindow(hwnd);VSTGUI::exitPlatform();return 6;}
        if(benchmarkCases.empty()){DestroyWindow(hwnd);VSTGUI::exitPlatform();return 6;}
        if(!initialBenchmarkId.empty()){
            const auto found=std::find_if(benchmarkCases.begin(),benchmarkCases.end(),[&](const auto& path){
                return path.stem().string()==initialBenchmarkId;});
            if(found==benchmarkCases.end()){DestroyWindow(hwnd);VSTGUI::exitPlatform();return 6;}
            benchmarkIndex=static_cast<std::size_t>(found-benchmarkCases.begin());
        }
    }
    wchar_t executable[32768]{}; GetModuleFileNameW(nullptr,executable,32768);
    app=std::make_unique<DemoApp>(std::filesystem::path(executable));
    if (!app->open(content) || !(benchmarkMode?loadBenchmarkIndex(benchmarkIndex):app->load(initial))) {
        DestroyWindow(hwnd); VSTGUI::exitPlatform(); return 2; }
    RECT requested{0,0,requestedWidth+(benchmarkMode?280:20),
        std::max(requestedHeight+65,benchmarkMode?850:705)};
    AdjustWindowRectEx(&requested,GetWindowLongW(hwnd,GWL_STYLE),FALSE,GetWindowLongW(hwnd,GWL_EXSTYLE));
    SetWindowPos(hwnd,nullptr,0,0,requested.right-requested.left,requested.bottom-requested.top,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
    app->simulateScale(scale);
    if(benchmarkMode){
        ShowWindow(combo,SW_HIDE);ShowWindow(currentButton,SW_HIDE);ShowWindow(snapshotButton,SW_HIDE);
        SetWindowTextW(statusLabel,L"BENCHMARK · CONTINUATION");
        benchmarkPanel=CreateWindowExW(0,L"STATIC",L"",WS_CHILD|WS_VISIBLE,10,55,250,800,hwnd,nullptr,instance,nullptr);
        const auto label=[&](const wchar_t* text,int y,int height=25){return CreateWindowExW(0,L"STATIC",text,
            WS_CHILD|WS_VISIBLE,20,y,230,height,hwnd,nullptr,instance,nullptr);};
        benchmarkIdLabel=label(L"BENCHMARK",65);
        CreateWindowExW(0,L"BUTTON",L"Previous",WS_CHILD|WS_VISIBLE,20,95,73,30,hwnd,reinterpret_cast<HMENU>(201),instance,nullptr);
        CreateWindowExW(0,L"BUTTON",L"Next",WS_CHILD|WS_VISIBLE,97,95,73,30,hwnd,reinterpret_cast<HMENU>(202),instance,nullptr);
        CreateWindowExW(0,L"BUTTON",L"Random",WS_CHILD|WS_VISIBLE,174,95,73,30,hwnd,reinterpret_cast<HMENU>(203),instance,nullptr);
        label(L"INPUT PROGRESSION",137);
        benchmarkInputLabel=label(L"",165,155);
        label(L"SELECTED CONTINUATION",328);
        benchmarkSelectedLabel=label(L"Select a recommendation",355,50);
        constexpr const wchar_t* ratingNames[]{L"Naturalness",L"Intent fit",L"Rhythm fit",L"Distinctiveness",L"Usability"};
        for(int i=0;i<5;++i){const int y=410+i*35;
            label(ratingNames[i],y);
            benchmarkScores[i]=CreateWindowExW(0,L"COMBOBOX",nullptr,WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST,
                157,y-4,90,170,hwnd,reinterpret_cast<HMENU>(static_cast<INT_PTR>(301+i)),instance,nullptr);
            for(const wchar_t* value:{L"1",L"2",L"3",L"4",L"5"})
                SendMessageW(benchmarkScores[i],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(value));
            SendMessageW(benchmarkScores[i],CB_SETCURSEL,2,0);
        }
        label(L"Verdict",587);
        benchmarkVerdict=CreateWindowExW(0,L"COMBOBOX",nullptr,WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST,
            125,585,122,170,hwnd,reinterpret_cast<HMENU>(306),instance,nullptr);
        for(const wchar_t* value:{L"KEEP",L"QUESTIONABLE",L"BAD"})
            SendMessageW(benchmarkVerdict,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(value));
        SendMessageW(benchmarkVerdict,CB_SETCURSEL,1,0);
        label(L"Issue category",623);
        benchmarkIssue=CreateWindowExW(0,L"COMBOBOX",nullptr,WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST,
            125,620,122,240,hwnd,reinterpret_cast<HMENU>(307),instance,nullptr);
        for(const wchar_t* value:{L"Harmony",L"Matching",L"Ranking",L"Intent",L"Rhythm",L"Diversity",
                                  L"Voicing",L"Sound",L"UI",L"Other"})
            SendMessageW(benchmarkIssue,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(value));
        SendMessageW(benchmarkIssue,CB_SETCURSEL,9,0);
        label(L"Short note",660);
        benchmarkNote=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_LEFT|ES_MULTILINE|WS_VSCROLL,
            20,683,227,51,hwnd,reinterpret_cast<HMENU>(308),instance,nullptr);
        CreateWindowExW(0,L"BUTTON",L"Save Rating",WS_CHILD|WS_VISIBLE,20,742,110,30,hwnd,reinterpret_cast<HMENU>(204),instance,nullptr);
        CreateWindowExW(0,L"BUTTON",L"Snapshot",WS_CHILD|WS_VISIBLE,137,742,110,30,hwnd,reinterpret_cast<HMENU>(205),instance,nullptr);
        loadBenchmarkIndex(benchmarkIndex);
    }else SendMessageW(combo,CB_SETCURSEL,initial-'A',0);
    if(!zoomSmokeReport.empty()) {
        std::ofstream report(zoomSmokeReport,std::ios::trunc);
        if(!report){DestroyWindow(hwnd);VSTGUI::exitPlatform();return 10;}
        int passed{};
        for(const auto [width,height]:std::array<std::pair<int,int>,3>{{{900,640},{1100,900},{1800,1000}}}) {
            app->resize(width,height);
            for(const std::uint32_t zoom:{100u,125u,150u}) {
                if(!app->zoomSmoke(zoom)){report<<"failed "<<width<<'x'<<height<<" zoom "<<zoom<<'\n';
                    DestroyWindow(hwnd);VSTGUI::exitPlatform();return 11;}
                app->tick();++passed;
            }
        }
        report<<"zoom smoke "<<passed<<"/9 PASS (modes, card hitbox, overlay, library)\n";
        DestroyWindow(hwnd);VSTGUI::exitPlatform();return 0;
    }
    if(!midiSmokeReport.empty()) {
        std::ofstream report(midiSmokeReport,std::ios::trunc);const bool okay=report&&app->midiWorkflowSmoke(midiSmokeInput);
        report<<"MIDI workflow: visible/More Continue/Enrich identity, audition, save, drag arm, snapshot, overlay "<<(okay?"PASS":"FAIL")<<'\n';
        DestroyWindow(hwnd);VSTGUI::exitPlatform();return okay?0:10;
    }
    if(!resizeSmokeReport.empty()) {
        std::ofstream report(resizeSmokeReport,std::ios::trunc);
        if(!report){DestroyWindow(hwnd);VSTGUI::exitPlatform();return 3;}
        int passed{};
        for(char letter='A';letter<='H';++letter) {
            if(!app->load(letter)){report<<"load failed "<<letter<<'\n';DestroyWindow(hwnd);VSTGUI::exitPlatform();return 4;}
            for(const auto [width,height]:std::array<std::pair<int,int>,3>{{{900,640},{1100,900},{1800,1000}}}) {
                RECT size{0,0,width+20,height+65};
                AdjustWindowRectEx(&size,GetWindowLongW(hwnd,GWL_STYLE),FALSE,GetWindowLongW(hwnd,GWL_EXSTYLE));
                SetWindowPos(hwnd,nullptr,0,0,size.right-size.left,size.bottom-size.top,
                    SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
                for(const double factor:{1.,1.25,1.5,2.}) {
                    app->simulateScale(factor);app->tick();
                    RECT client{};GetClientRect(demoContent,&client);
                    if(client.right<900||client.bottom<640){report<<"size failed "<<letter<<' '<<width<<'x'<<height<<'\n';
                        DestroyWindow(hwnd);VSTGUI::exitPlatform();return 5;}
                    ++passed;
                }
            }
        }
        report<<"resize smoke "<<passed<<"/96 PASS\n";
        DestroyWindow(hwnd);VSTGUI::exitPlatform();return 0;
    }
    if(!benchmarkSmokeReport.empty()) {
        std::ofstream report(benchmarkSmokeReport,std::ios::trunc);
        if(!report){DestroyWindow(hwnd);VSTGUI::exitPlatform();return 7;}
        std::size_t passed{};
        for(std::size_t i=0;i<benchmarkCases.size();++i){
            if(!loadBenchmarkIndex(i)){report<<"failed "<<i<<'\n';DestroyWindow(hwnd);VSTGUI::exitPlatform();return 8;}
            app->tick();RECT size{};GetClientRect(demoContent,&size);
            if(size.right<900||size.bottom<640){report<<"undersized "<<i<<'\n';DestroyWindow(hwnd);VSTGUI::exitPlatform();return 9;}
            ++passed;
        }
        report<<"benchmark navigation "<<passed<<'/'<<benchmarkCases.size()<<" PASS\n";
        DestroyWindow(hwnd);VSTGUI::exitPlatform();return 0;
    }
    ShowWindow(hwnd,show); UpdateWindow(hwnd); SetTimer(hwnd,1,250,nullptr);
    MSG msg{}; while (GetMessageW(&msg,nullptr,0,0)>0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    VSTGUI::exitPlatform(); return static_cast<int>(msg.wParam);
}
