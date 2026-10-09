#include "library/ProgressionLibrary.h"
#include "library/LegacyFactoryIdResolver.h"
#include "core/ContinuationEngine.h"
#include "midi/MidiClip.h"
#include "midi/StandardMidiFileWriter.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace harmony;
void require(bool ok,const std::string& message) {
    if(!ok)throw std::runtime_error(message);
}
void smoke(const library::LoadResult& catalogue) {
    const CandidateIndex index(catalogue.templates);
    for(const auto prefix:{"V3_DUSK_","V3_OPEN_"}) {
        const auto it=std::find_if(catalogue.templates.begin(),catalogue.templates.end(),
                                 [&](const auto& t){return t.id.starts_with(prefix);});
        require(it!=catalogue.templates.end(),"representative phrase missing");
        Progression chords;
        for(std::size_t i=0;i<2;++i) {
            const auto concrete=realizeContinuation(it->full[i],{PitchClass::C,it->mode},4);
            ChordEvent chord;chord.name=concrete.label;chord.quality=concrete.quality;chord.startQN=4.*i;
            chord.openEnded=i==1;if(i==0)chord.durationQN=4;chords.push_back(std::move(chord));
        }
        ImportedProgressionSession imported;
        require(imported.replace(chords,TimelineCoordinateMode::RelativeToSelection),"smoke input");
        AnalysisContext context;context.forcedKey=KeySignature{PitchClass::C,it->mode};
        const auto result=recommendContinuations(makeMatchQuery(imported.events,context),index,
                                                {{},it->intent,{}});
        const ContinuationCandidate* candidate{};
        for(const auto& group:result.groups)if(!group.empty()){candidate=&group.front();break;}
        require(candidate!=nullptr,"no smoke result: "+it->id);
        const auto audible=preview::buildSequence(imported,candidate,120);
        require(static_cast<bool>(audible),"smoke preview: "+it->id);
        const auto clip=midi::buildClip(audible.sequence,midi::ArrangementMode::VoiceLed,
                                       midi::ExportScope::FullPhrase,{});
        require(static_cast<bool>(clip),"smoke MIDI: "+it->id);
        const auto smf=midi::writeToMemory(clip.sequence);
        require(smf&&smf.bytes.size()>14,"smoke serialization: "+it->id);
        std::cout<<it->id<<" load -> recommendation -> MIDI PASS\n";
    }
}
int main(int argc,char** argv) {
    try {
        const auto v3=library::loadFactory(HC_FACTORY_DB_PATH);
        require(static_cast<bool>(v3),"production catalogue load");
        if(argc==2&&std::string_view(argv[1])=="--smoke"){smoke(v3);return 0;}
        const auto v2=library::loadFactory(HC_FACTORY_V2_PATH);
        require(v3&&v3.libraryVersion==3&&v3.storageSchemaVersion==2,"expanded V3 database loads");
        require(v3.templates.size()>=500&&v3.templates.size()<=800,"V3 content target");
        require(v2&&v2.libraryVersion==2&&v2.storageSchemaVersion==1&&v2.templates.size()==161,
                "historical V2 remains 161 rows / Schema 1");
        for(const auto& old:v2.templates) {
            const auto ref=library::resolveFactoryId(old.id,v3.templates);
            require(ref&&ref.entry->full.size()==old.full.size(),"historical reference lost: "+old.id);
            for(std::size_t i=0;i<old.full.size();++i)
                require(formatMatchEvent(ref.entry->full[i])==formatMatchEvent(old.full[i])&&
                        ref.entry->full[i].durationQN==old.full[i].durationQN,
                        "historical music changed: "+old.id);
        }
        std::size_t additions{};
        for(const auto& item:v3.templates) {
            require(!item.nameZh.empty()&&!item.nameEn.empty(),"missing display name: "+item.id);
            if(!item.id.starts_with("V3_"))continue;
            ++additions;
            // Exercise the actual library consumer, not just source JSON syntax.
            const auto audible=midi::previewFromTemplate(item,{PitchClass::C,item.mode},120);
            require(audible&&audible.sequence.events.size()==item.full.size(),"preview: "+item.id);
            const auto clip=midi::buildClip(audible.sequence,midi::ArrangementMode::VoiceLed,
                                          midi::ExportScope::CurrentOnly,{});
            require(clip&&!clip.sequence.notes.empty(),"MIDI realization: "+item.id);
            for(const auto& note:clip.sequence.notes)
                require(note.midiNote>=0&&note.midiNote<=127&&std::isfinite(note.startQN)&&
                        std::isfinite(note.durationQN)&&note.durationQN>0,
                        "invalid MIDI note: "+item.id);
            const auto smf=midi::writeToMemory(clip.sequence);
            require(smf&&smf.bytes.size()>14&&std::equal(smf.bytes.begin(),smf.bytes.begin()+4,"MThd"),
                    "MIDI serialization: "+item.id);
        }
        require(additions+155==v3.templates.size(),"canonical legacy row count changed");
        std::cout<<"V3 "<<v3.templates.size()<<" rows; "<<additions
                 <<" additions preview/MIDI PASS; V2 161 references preserved\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
