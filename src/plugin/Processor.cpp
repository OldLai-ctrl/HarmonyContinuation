#include "Processor.h"
#include "PluginIds.h"
#include "pluginterfaces/vst/ivstmessage.h"
#include "pluginterfaces/base/ibstream.h"
#include <cstring>
namespace harmony::plugin {
using namespace Steinberg;
using namespace Steinberg::Vst;
Processor::Processor() {
    setControllerClass(controllerId);
    // AudioEffect implements IProcessContextRequirements in VST3 SDK 3.8.1.
    // Keep this list aligned with the fields captured in the bounded snapshot.
    processContextRequirements.needTempo().needTimeSignature().needProjectTimeMusic().needChord().needTransportState();
}
tresult PLUGIN_API Processor::initialize(FUnknown* host) {
    const auto result = AudioEffect::initialize(host);
    if (result != kResultOk) return result;
    addAudioInput(STR16("Input"), SpeakerArr::kStereo);
    addAudioOutput(STR16("Output"), SpeakerArr::kStereo);
    return kResultOk;
}
tresult PLUGIN_API Processor::setBusArrangements(SpeakerArrangement* in, int32 ni, SpeakerArrangement* out, int32 no) {
    if (ni != 1 || no != 1 || !in || !out || in[0] != out[0] || (in[0] != SpeakerArr::kMono && in[0] != SpeakerArr::kStereo)) return kResultFalse;
    return AudioEffect::setBusArrangements(in, ni, out, no);
}
tresult PLUGIN_API Processor::canProcessSampleSize(int32 size) { return size == kSample32 || size == kSample64 ? kResultTrue : kResultFalse; }
tresult PLUGIN_API Processor::process(ProcessData& data) {
    context_.capture(data.processContext);
    if (!data.numOutputs || data.numSamples <= 0) return kResultOk;
    if (!data.outputs) return kResultFalse;
    for (int32 bus = 0; bus < data.numOutputs; ++bus) {
        auto& out = data.outputs[bus];
        const auto* in = data.inputs && bus < data.numInputs ? &data.inputs[bus] : nullptr;
        out.silenceFlags = 0;
        for (int32 c = 0; c < out.numChannels; ++c) {
            const bool silent = !in || c >= in->numChannels || (c < 64 && (in->silenceFlags & (uint64(1) << c)));
            const auto bytes = static_cast<std::size_t>(data.numSamples) * (data.symbolicSampleSize == kSample64 ? sizeof(double) : sizeof(float));
            void* dest = data.symbolicSampleSize == kSample64 ? static_cast<void*>(out.channelBuffers64[c]) : out.channelBuffers32[c];
            const void* source = silent ? nullptr : (data.symbolicSampleSize == kSample64 ? static_cast<const void*>(in->channelBuffers64[c]) : in->channelBuffers32[c]);
            if (!source) { if (dest) std::memset(dest, 0, bytes); if (c < 64) out.silenceFlags |= uint64(1) << c; }
            else if (dest && dest != source) std::memcpy(dest, source, bytes);
        }
    }
    return kResultOk;
}
tresult PLUGIN_API Processor::notify(IMessage* message) {
    try {
        if (message && message->getMessageID() &&
            (std::strcmp(message->getMessageID(), "HC.RequestSnapshot") == 0 ||
             std::strcmp(message->getMessageID(), "HC.PollTransport") == 0)) {
            const bool transportPoll = std::strcmp(message->getMessageID(), "HC.PollTransport") == 0;
            HostSnapshot snapshot;
            if (!context_.read(snapshot)) return kResultFalse;
            auto reply = owned(allocateMessage());
            if (!reply) return kResultFalse;
            reply->setMessageID(transportPoll ? "HC.TransportSnapshot" : "HC.Snapshot");
            reply->getAttributes()->setBinary("snapshot", &snapshot, sizeof(snapshot));
            return sendMessage(reply);
        }
        return AudioEffect::notify(message);
    } catch (...) { return kResultFalse; }
}
tresult PLUGIN_API Processor::getState(IBStream* stream) {
    if (!stream) return kInvalidArgument;
    const char magic[] = "HC00"; int32 written{};
    return stream->write(const_cast<char*>(magic), 4, &written) == kResultOk && written == 4 ? kResultOk : kResultFalse;
}
tresult PLUGIN_API Processor::setState(IBStream* stream) {
    if (!stream) return kInvalidArgument;
    char magic[4]{}; int32 read{};
    return stream->read(magic, 4, &read) == kResultOk && read == 4 && std::memcmp(magic, "HC00", 4) == 0 ? kResultOk : kResultFalse;
}
}
