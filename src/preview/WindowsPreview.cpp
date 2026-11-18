#include "WindowsPreview.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <thread>
#if defined(_WIN32)
#include <windows.h>
#include <mmsystem.h>
#endif
namespace harmony::preview {
struct WindowsPreview::Impl {
    PreviewMailbox commands;
    PreviewFeedback published;
    std::atomic<bool> quit{};
    std::thread worker;
    Impl():worker([this]{run();}){}
    ~Impl(){quit.store(true,std::memory_order_release);if(worker.joinable())worker.join();}
    void run() noexcept {
        PreviewReceiver receiver;receiver.transport.prepare(48000);
        TransportRequest request;TransportFeedback status;status.sampleRate=48000;
#if defined(_WIN32)
        constexpr int frames=256,buffers=3;
        HWAVEOUT device{};std::array<WAVEHDR,buffers> headers{};
        std::array<std::array<short,frames*2>,buffers> pcm{};
        std::array<float,frames> left{},right{};
        double originQN{};std::uint64_t previousClip{},previousSeek{};
        PlaybackState previousState=PlaybackState::Stopped;
        const auto close=[&]{
            if(!device)return true;
            bool okay=waveOutReset(device)==MMSYSERR_NOERROR;
            for(auto& h:headers)if(h.dwFlags&WHDR_PREPARED)
                okay=(waveOutUnprepareHeader(device,&h,sizeof(h))==MMSYSERR_NOERROR)&&okay;
            if(waveOutClose(device)!=MMSYSERR_NOERROR)return false;
            device=nullptr;headers={};return okay;
        };
        const auto open=[&]{
            if(device)return true;
            WAVEFORMATEX format{};format.wFormatTag=WAVE_FORMAT_PCM;format.nChannels=2;format.nSamplesPerSec=48000;
            format.wBitsPerSample=16;format.nBlockAlign=4;format.nAvgBytesPerSec=192000;
            if(waveOutOpen(&device,WAVE_MAPPER,&format,0,0,CALLBACK_NULL)!=MMSYSERR_NOERROR){device=nullptr;return false;}
            for(int i=0;i<buffers;++i){headers[i].lpData=reinterpret_cast<LPSTR>(pcm[i].data());headers[i].dwBufferLength=frames*4;
                if(waveOutPrepareHeader(device,&headers[i],sizeof(WAVEHDR))!=MMSYSERR_NOERROR){close();return false;}}
            return true;
        };
        const auto heardQN=[&]{MMTIME time{};time.wType=TIME_SAMPLES;
            if(!device)return status.positionQN;
            if(waveOutGetPosition(device,&time,sizeof(time))!=MMSYSERR_NOERROR){status.error=AudioError::DeviceUnavailable;return status.positionQN;}
            double samples{};
            if(time.wType==TIME_SAMPLES)samples=time.u.sample;
            else if(time.wType==TIME_BYTES)samples=double(time.u.cb)/4.;
            else {status.error=AudioError::DeviceUnavailable;return status.positionQN;}
            const auto start=receiver.transport.startQN(),end=receiver.transport.endQN();
            const auto position=originQN+samples/48000*receiver.transport.tempoBPM()/60.;
            if(receiver.transport.repeat()&&end>start)return start+std::fmod((std::max)(0.,position-start),end-start);
            return std::clamp(position,start,end);
        };
        while(!quit.load(std::memory_order_acquire)){
            if(commands.take(request)&&request.serial>receiver.serial){
                const bool repeatChanged=request.repeat!=receiver.transport.repeat();
                const bool moved=request.clipId!=previousClip||request.seekId!=previousSeek;
                const bool changed=moved||repeatChanged;
                if(request.state==PlaybackState::Stopped){
                    if(close()){receiver.apply(request);status.error=AudioError::None;}
                    else status.error=AudioError::DeviceUnavailable;
                }
                else if(changed){
                    // Flush queued audio on a repeat toggle and rebase the device
                    // clock at the heard position, using the old loop policy.
                    const auto position=moved?request.positionQN:heardQN();
                    if(!close()){status.error=AudioError::DeviceUnavailable;published.publish(status);continue;}
                    if(!receiver.apply(request)){status.error=AudioError::InvalidClip;continue;}
                    if(!moved)receiver.transport.seekQN(position);
                    originQN=receiver.transport.positionQN();status.positionQN=originQN;
                    if(!open()){receiver.transport.stop();status.error=AudioError::DeviceUnavailable;}
                    else {status.error=AudioError::None;if(request.state==PlaybackState::Paused&&waveOutPause(device)!=MMSYSERR_NOERROR)status.error=AudioError::DeviceUnavailable;}
                } else if(request.state==PlaybackState::Paused){
                    // Freeze the device first, then reconstruct from its heard
                    // sample. Queued samples can be ahead of the transport's
                    // natural end; retaining that queue would restart at zero.
                    if(device&&waveOutPause(device)!=MMSYSERR_NOERROR)status.error=AudioError::DeviceUnavailable;
                    const auto position=heardQN();
                    if(status.error==AudioError::None&&close()){
                        if(!receiver.apply(request))status.error=AudioError::InvalidClip;
                        else {receiver.transport.seekQN(position);receiver.transport.pause();originQN=position;status.positionQN=position;
                            if(receiver.transport.state()==PlaybackState::Paused&&(!open()||waveOutPause(device)!=MMSYSERR_NOERROR))status.error=AudioError::DeviceUnavailable;}
                    }else status.error=AudioError::DeviceUnavailable;
                } else {
                    receiver.apply(request);if(!open()){receiver.transport.stop();status.error=AudioError::DeviceUnavailable;}
                    else {status.error=waveOutRestart(device)==MMSYSERR_NOERROR?AudioError::None:AudioError::DeviceUnavailable;}
                }
                previousClip=request.clipId;previousSeek=request.seekId;previousState=request.state;
                if(request.state!=PlaybackState::Stopped&&receiver.transport.state()==PlaybackState::Stopped&&status.error==AudioError::None){
                    status.positionQN=receiver.transport.endQN();close();previousState=PlaybackState::Stopped;
                }
            }
            if(device&&previousState==PlaybackState::Playing&&status.error==AudioError::None){
                bool queued=false;
                for(auto& h:headers){
                    if((h.dwFlags&WHDR_INQUEUE)&&!(h.dwFlags&WHDR_DONE)){queued=true;continue;}
                    if(receiver.transport.state()!=PlaybackState::Playing)continue;
                    receiver.transport.process(left.data(),right.data(),frames);
                    auto* samples=reinterpret_cast<short*>(h.lpData);
                    for(int i=0;i<frames;++i){samples[2*i]=short(std::clamp(left[i],-1.f,1.f)*32767);samples[2*i+1]=short(std::clamp(right[i],-1.f,1.f)*32767);}
                    if(waveOutWrite(device,&h,sizeof(h))!=MMSYSERR_NOERROR){status.error=AudioError::DeviceUnavailable;receiver.transport.stop();close();break;}
                    queued=true;
                }
                status.positionQN=heardQN();
                if(!queued&&receiver.transport.state()==PlaybackState::Stopped){status.positionQN=receiver.transport.endQN();close();previousState=PlaybackState::Stopped;}
            }
            if(status.error!=AudioError::None){receiver.transport.pause();close();}
            status.serial=receiver.serial;status.clipId=receiver.clipId;status.outputActive=device!=nullptr;
            status.state=status.error!=AudioError::None?PlaybackState::Stopped:previousState;
            if(status.state==PlaybackState::Stopped&&status.error==AudioError::None&&previousState==PlaybackState::Stopped)status.positionQN=receiver.transport.positionQN();
            ++status.blocks;published.publish(status);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        close();
#else
        while(!quit.load(std::memory_order_acquire)){
            if(commands.take(request)){status.serial=request.serial;status.clipId=request.clipId;status.error=AudioError::DeviceUnavailable;published.publish(status);}
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
#endif
    }
};
WindowsPreview::WindowsPreview():impl_(std::make_unique<Impl>()){}
WindowsPreview::~WindowsPreview()=default;
bool WindowsPreview::submit(const TransportRequest& r) noexcept {return validTransportRequest(r)&&impl_->commands.publish(r);}
TransportFeedback WindowsPreview::feedback() const noexcept {TransportFeedback s;impl_->published.read(s);return s;}
}
