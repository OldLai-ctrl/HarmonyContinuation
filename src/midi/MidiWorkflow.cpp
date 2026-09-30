#include "MidiWorkflow.h"
#include <atomic>
#include <chrono>
#include <random>
namespace harmony::midi {
namespace {
PayloadResult payload(const CandidateExportContext& context,const preview::BuildResult& built,
    std::optional<KeySignature> key,std::optional<PhraseIntent> intent) {
    if(!built)return {{},built.error};
    const auto clip=buildClip(built.sequence,context.mode,context.scope,context.meter,key,intent);
    if(!clip)return {{},clip.error};
    auto value=makePayload(clip.sequence,suggestedFilename(intent,key));
    if(value.smfBytes.empty())return {{},"invalid MIDI payload"};
    return {std::move(value),{}};
}
}
PayloadResult candidatePayload(const CandidateExportContext& context,const ContinuationCandidate& candidate) {
    return payload(context,preview::buildSequence(context.imported,&candidate,context.tempo),candidate.key,candidate.intent);
}
PayloadResult candidatePayload(const CandidateExportContext& context,const enrichment::EnrichmentCandidate& candidate) {
    ImportedProgressionSession transformed;
    if(!transformed.replace(candidate.progression,TimelineCoordinateMode::RelativeToSelection))return {{},"invalid enrichment timeline"};
    return payload(context,preview::buildSequence(transformed,nullptr,context.tempo,candidate.constraints),context.key,{});
}
DragFileResult createDragFile(const MidiClipPayload& payload,const std::filesystem::path& directory) {
    try {
        std::filesystem::create_directories(directory);
        if(std::filesystem::is_symlink(directory))return {{},"unsafe MIDI drag directory"};
        const auto cutoff=std::filesystem::file_time_type::clock::now()-std::chrono::hours(48);
        std::size_t inspected{};
        for(const auto& entry:std::filesystem::directory_iterator(directory)) {
            if(++inspected>4096)break;
            const auto name=entry.path().filename().string();
            std::error_code ec;
            if(entry.is_regular_file(ec)&&!entry.is_symlink(ec)&&name.starts_with("HC_")&&entry.path().extension()==".mid") {
                const auto modified=entry.last_write_time(ec);
                if(!ec&&modified<cutoff)std::filesystem::remove(entry.path(),ec);
            }
        }
        static std::atomic<std::uint64_t> count{};
        std::string safe;for(unsigned char c:payload.suggestedFilename)if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_')safe+=static_cast<char>(c);
        if(safe.size()>64)safe.resize(64);
        const auto stamp=std::chrono::system_clock::now().time_since_epoch().count();
        const auto path=directory/("HC_"+safe+"_"+std::to_string(stamp)+"_"+std::to_string(std::random_device{}())+"_"+std::to_string(++count)+".mid");
        std::string error;if(!writeToFile(payload.smfBytes,path,error))return {{},error};
        return {path,{}};
    } catch(const std::exception& e){return {{},e.what()};}
}
}
