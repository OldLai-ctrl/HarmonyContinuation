#include "AsyncMidiImport.h"
namespace harmony::io {
AsyncMidiImport::AsyncMidiImport(){thread_=std::thread([this]{run();});}
AsyncMidiImport::~AsyncMidiImport(){
    {std::lock_guard lock(mutex_);stop_=true;pending_.reset();}wake_.notify_one();if(thread_.joinable())thread_.join();
}
std::uint64_t AsyncMidiImport::submit(std::vector<std::filesystem::path> paths,bool open){
    if(paths.size()>32)paths.resize(32);
    std::lock_guard lock(mutex_);pending_=Task{++generation_,std::move(paths),open};ready_.reset();wake_.notify_one();return generation_;
}
void AsyncMidiImport::cancel(){std::lock_guard lock(mutex_);++generation_;pending_.reset();ready_.reset();}
std::optional<CompletedMidiImport> AsyncMidiImport::takeLatest(){
    std::lock_guard lock(mutex_);auto result=std::move(ready_);ready_.reset();return result;
}
void AsyncMidiImport::run(){
    for(;;){Task task;
        {std::unique_lock lock(mutex_);wake_.wait(lock,[this]{return stop_||pending_.has_value();});if(stop_)return;task=std::move(*pending_);pending_.reset();}
        CompletedMidiImport completed;completed.generation=task.generation;
        completed.result.midi.status=midi::ReadStatus::FileError;completed.result.midi.error="No readable MIDI file";
        try {for(const auto& path:task.paths){
            {std::lock_guard lock(mutex_);if(stop_)return;if(task.generation!=generation_)break;}
            completed.result=midi::importFile(path,task.open);if(completed.result)break;
        }}catch(...){completed.result={};completed.result.midi.status=midi::ReadStatus::FileError;completed.result.midi.error="MIDI import failed";}
        {std::lock_guard lock(mutex_);if(stop_)return;if(task.generation==generation_)ready_=std::move(completed);}
    }
}
}
