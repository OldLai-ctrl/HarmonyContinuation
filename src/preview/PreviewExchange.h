#pragma once
#include "PreviewTransport.h"
#include <atomic>
#include <bit>
#include <cmath>

namespace harmony::preview {
enum class AudioBackend : std::uint32_t { Host, Windows };
enum class AudioError : std::uint32_t { None, DeviceUnavailable, InvalidClip, CallbackAbsent };
// A complete desired state, rather than lossy independent load/control commands.
// Only the UI thread publishes; one audio/Windows worker consumes.
struct TransportRequest {
    std::uint64_t serial{},clipId{},seekId{};
    PlaybackState state{PlaybackState::Stopped};
    double positionQN{};
    PreparedPreview clip{};
    bool repeat{};
};
inline bool validTransportRequest(const TransportRequest& r) noexcept {
    return r.serial>0&&std::isfinite(r.positionQN)&&
        (r.state==PlaybackState::Stopped||r.state==PlaybackState::Playing||r.state==PlaybackState::Paused)&&
        ((r.clipId&&validPreparedPreview(r.clip))||(!r.clipId&&r.state==PlaybackState::Stopped));
}
class PreviewMailbox {
public:
    bool publish(const TransportRequest& request) noexcept {
        slots_[back_]=request;
        back_=middle_.exchange(back_|dirty,std::memory_order_acq_rel)&indexMask;
        return true;
    }
    bool take(TransportRequest& request) noexcept {
        for(int attempt=0;attempt<3;++attempt){
            auto middle=middle_.load(std::memory_order_acquire);
            if(!(middle&dirty))return false;
            if(middle_.compare_exchange_strong(middle,front_,std::memory_order_acq_rel)){
                front_=middle&indexMask;request=slots_[front_];return true;
            }
        }
        return false;
    }
private:
    static_assert(std::atomic<int>::is_always_lock_free);
    // Producer owns back, consumer owns front, and an atomic exchange transfers
    // ownership through middle. Neither side writes the other side's buffer.
    static constexpr int dirty=4,indexMask=3;
    std::array<TransportRequest,3> slots_{};
    int front_{0},back_{2};
    std::atomic<int> middle_{1};
};

struct TransportFeedback {
    std::uint64_t serial{},clipId{},blocks{};
    double positionQN{},sampleRate{};
    PlaybackState state{PlaybackState::Stopped};
    AudioError error{AudioError::None};
    bool outputActive{};
};
class PreviewFeedback {
public:
    void publish(const TransportFeedback& s) noexcept {
        revision_.fetch_add(1,std::memory_order_seq_cst);
        const std::array<std::uint64_t,8> words{s.serial,s.clipId,s.blocks,std::bit_cast<std::uint64_t>(s.positionQN),
            std::bit_cast<std::uint64_t>(s.sampleRate),std::uint64_t(s.state),std::uint64_t(s.error),std::uint64_t(s.outputActive)};
        for(std::size_t i=0;i<words.size();++i)words_[i].store(words[i],std::memory_order_seq_cst);
        revision_.fetch_add(1,std::memory_order_seq_cst);
    }
    bool read(TransportFeedback& s) const noexcept {
        for(int attempt=0;attempt<4;++attempt){
            const auto before=revision_.load(std::memory_order_seq_cst);if(before&1)continue;
            std::array<std::uint64_t,8> w{};for(std::size_t i=0;i<w.size();++i)w[i]=words_[i].load(std::memory_order_seq_cst);
            if(revision_.load(std::memory_order_seq_cst)!=before)continue;
            s={w[0],w[1],w[2],std::bit_cast<double>(w[3]),std::bit_cast<double>(w[4]),PlaybackState(w[5]),AudioError(w[6]),w[7]!=0};return true;
        }
        return false;
    }
private:
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
    std::atomic<std::uint64_t> revision_{};
    std::array<std::atomic<std::uint64_t>,8> words_{};
};
// Owned exclusively by a processing thread. A stopped request also works before
// any clip has been loaded, which is needed for safe backend handover.
class PreviewReceiver {
public:
    PreviewTransport transport;
    bool apply(const TransportRequest& r) noexcept {
        if(!validTransportRequest(r)||r.serial<=serial)return false;
        if(r.clipId&&r.clipId!=clipId){
            if(!transport.load(r.clip))return false;
            clipId=r.clipId;seekId=0;
        }
        transport.setRepeat(r.repeat);
        if(!r.clipId)clipId=0;
        if(r.state==PlaybackState::Stopped)transport.stop();
        else {
            if(r.state==PlaybackState::Playing&&transport.state()!=PlaybackState::Playing)transport.play();
            else if(transport.state()==PlaybackState::Stopped)transport.play();
            if(r.seekId!=seekId)transport.seekQN(r.positionQN);
            if(r.state==PlaybackState::Paused)transport.pause();
        }
        serial=r.serial;seekId=r.seekId;return true;
    }
    std::uint64_t serial{},clipId{},seekId{};
};
}
