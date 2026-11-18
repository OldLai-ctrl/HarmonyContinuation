#include "Processor.h"
#include "PluginIds.h"
#include "pluginterfaces/vst/ivstmessage.h"
#include "pluginterfaces/base/ibstream.h"
#include <cstring>
#include <algorithm>
#include <cmath>
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
tresult PLUGIN_API Processor::setupProcessing(ProcessSetup& setup) {
    if(!std::isfinite(setup.sampleRate)||setup.sampleRate<8000||setup.sampleRate>192000||setup.maxSamplesPerBlock<=0||setup.maxSamplesPerBlock>1048576)return kInvalidArgument;
    const auto result=AudioEffect::setupProcessing(setup);if(result!=kResultOk)return result;
    previewRate_=setup.sampleRate;maximumBlock_=setup.maxSamplesPerBlock;preview_.transport.prepare(previewRate_);preview_.clipId=0;preview_.seekId=0;return result;
}
tresult PLUGIN_API Processor::setProcessing(TBool active) {
    processing_.store(active!=0,std::memory_order_release);
    if(!active)previewGate_.store(0,std::memory_order_seq_cst);
    return kResultOk;
}
tresult PLUGIN_API Processor::process(ProcessData& data) {
    previewInFlight_.store(true,std::memory_order_seq_cst);
    struct EndCallback { std::atomic<bool>& active;~EndCallback(){active.store(false,std::memory_order_seq_cst);} } endCallback{previewInFlight_};
    context_.capture(data.processContext,data.inputEvents!=nullptr);
    if(data.numSamples<0||data.numSamples>maximumBlock_||(data.symbolicSampleSize!=kSample32&&data.symbolicSampleSize!=kSample64))return kInvalidArgument;
    if(previewCommands_.take(previewRequest_)){
        previewError_=preview_.apply(previewRequest_)?preview::AudioError::None:preview::AudioError::InvalidClip;
        if(previewError_!=preview::AudioError::None)previewGate_.store(0,std::memory_order_seq_cst);
    }
    ++previewBlocks_;
    const auto publish=[&](bool output){previewFeedback_.publish({preview_.serial,preview_.clipId,previewBlocks_,preview_.transport.positionQN(),previewRate_,preview_.transport.state(),previewError_,output});};
    if(!data.numOutputs||data.numSamples<=0){publish(false);return kResultOk;}
    if(!data.outputs)return kResultFalse;
    for(int32 bus=0;bus<data.numOutputs;++bus){
        auto& out=data.outputs[bus];const auto* in=data.inputs&&bus<data.numInputs?&data.inputs[bus]:nullptr;out.silenceFlags=0;
        for(int32 c=0;c<out.numChannels;++c){
            const bool silent=!in||c>=in->numChannels||(c<64&&(in->silenceFlags&(uint64(1)<<c)));
            const auto bytes=std::size_t(data.numSamples)*(data.symbolicSampleSize==kSample64?sizeof(double):sizeof(float));
            void* dest=data.symbolicSampleSize==kSample64?(out.channelBuffers64?static_cast<void*>(out.channelBuffers64[c]):nullptr):(out.channelBuffers32?out.channelBuffers32[c]:nullptr);
            const void* source=silent?nullptr:data.symbolicSampleSize==kSample64?(in->channelBuffers64?static_cast<const void*>(in->channelBuffers64[c]):nullptr):(in->channelBuffers32?in->channelBuffers32[c]:nullptr);
            if(!source){if(dest)std::memset(dest,0,bytes);if(c<64)out.silenceFlags|=uint64(1)<<c;}
            else if(dest&&dest!=source)std::memcpy(dest,source,bytes);
        }
    }
    const auto& outputBus=data.outputs[0];
    bool available=processing_.load(std::memory_order_acquire)&&outputBus.numChannels>0;
    for(int32 channel=0;available&&channel<outputBus.numChannels;++channel)
        available=data.symbolicSampleSize==kSample64?(outputBus.channelBuffers64&&outputBus.channelBuffers64[channel]):(outputBus.channelBuffers32&&outputBus.channelBuffers32[channel]);
    if(available&&previewGate_.load(std::memory_order_seq_cst)==preview_.serial&&preview_.serial){
        std::array<float,256> left{},right{};
        for(int32 offset=0;offset<data.numSamples;offset+=256){const int32 count=std::min<int32>(256,data.numSamples-offset);
            preview_.transport.process(left.data(),right.data(),count);
            if(previewGate_.load(std::memory_order_seq_cst)!=preview_.serial)break;
            auto& out=data.outputs[0];
            for(int32 c=0;c<out.numChannels;++c){const auto* audio=c==1?right.data():left.data();bool nonzero=false;
                for(int32 i=0;i<count;++i){nonzero|=audio[i]!=0;
                    if(data.symbolicSampleSize==kSample64){if(out.channelBuffers64&&out.channelBuffers64[c])out.channelBuffers64[c][offset+i]+=audio[i];}
                    else if(out.channelBuffers32&&out.channelBuffers32[c])out.channelBuffers32[c][offset+i]+=audio[i];}
                if(nonzero&&c<64)out.silenceFlags&=~(uint64(1)<<c);
            }
        }
    }
    publish(available);return kResultOk;
}

tresult PLUGIN_API Processor::notify(IMessage* message) {
    try {
        if(message&&message->getMessageID()&&std::strcmp(message->getMessageID(),"HC.PreviewCommand")==0){
            const void* bytes{};uint32 size{};
            if(!message->getAttributes()||message->getAttributes()->getBinary("request",bytes,size)!=kResultOk||!bytes||size!=sizeof(preview::TransportRequest))return kInvalidArgument;
            preview::TransportRequest request;std::memcpy(&request,bytes,sizeof(request));
            if(!preview::validTransportRequest(request)||request.serial<=lastPreviewRequest_||!previewCommands_.publish(request))return kInvalidArgument;
            lastPreviewRequest_=request.serial;
            previewGate_.store(request.state==preview::PlaybackState::Playing?request.serial:0,std::memory_order_seq_cst);return kResultOk;
        }
        if(message&&message->getMessageID()&&std::strcmp(message->getMessageID(),"HC.PreviewPoll")==0){
            preview::TransportFeedback status;previewFeedback_.read(status);
            status.outputActive=status.outputActive&&processing_.load(std::memory_order_acquire);
            auto reply=owned(allocateMessage());if(!reply)return kResultFalse;
            reply->setMessageID("HC.PreviewStatus");reply->getAttributes()->setBinary("status",&status,sizeof(status));
            reply->getAttributes()->setInt("muted",previewGate_.load(std::memory_order_seq_cst)==0);
            reply->getAttributes()->setInt("quiescent",!previewInFlight_.load(std::memory_order_seq_cst));return sendMessage(reply);
        }

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
