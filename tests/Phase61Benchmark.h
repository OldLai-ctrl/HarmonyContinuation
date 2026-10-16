#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {
constexpr const char* phase61FactoryHash="74c529828f8c4b934bbaf4ada719aa95491ab498b58885522124ff6505671534";

std::string phase61ChordPath(const std::vector<ConcreteChordEvent>& events) {
    std::ostringstream out;
    for (std::size_t i=0;i<events.size();++i) {
        if (i) out << " -> ";
        out << events[i].label << '@' << events[i].durationQN << "QN";
    }
    return out.str();
}

std::string phase61InputPath(const Progression& events) {
    std::ostringstream out;
    for (std::size_t i=0;i<events.size();++i) {
        if (i) out << " -> ";
        out << events[i].name << '@' << events[i].durationQN.value_or(-1.) << "QN";
        if (events[i].openEnded) out << "(OPEN)";
    }
    return out.str();
}

std::pair<std::optional<ScaleDegree>,std::optional<ScaleDegree>> phase61PhraseEndpoints(
    const MatchQuery& query,const ContinuationCandidate& candidate) {
    for (const auto& interpretation:query.interpretations) {
        if (interpretation.key.key.tonic!=candidate.key.tonic ||
            interpretation.key.key.mode!=candidate.key.mode || interpretation.full.empty()) continue;
        return {interpretation.full.front().degree,interpretation.full.back().degree};
    }
    return {};
}

bool phase61SameChord(const ChordEvent& source,const ConcreteChordEvent& target) {
    ChordEvent realized;realized.name=target.label;realized.quality=target.quality;
    const auto a=normalizeChord(source),b=normalizeChord(realized);
    return a.root==b.root&&a.quality==b.quality&&a.bass==b.bass;
}

bool phase61CandidateRepeatsInput(const Progression& input,const ContinuationCandidate& candidate) {
    if (input.size()!=candidate.continuation.size()) return false;
    for (std::size_t i=0;i<input.size();++i)
        if (!phase61SameChord(input[i],candidate.continuation[i])) return false;
    return true;
}

bool phase61TargetSuffix(const ContinuationCandidate& candidate) {
    constexpr std::array<int,4> roots{4,5,7,0};
    constexpr std::array<ChordQuality,4> qualities{ChordQuality::Minor,ChordQuality::Major,ChordQuality::Major,ChordQuality::Major};
    if (candidate.continuation.size()!=roots.size()) return false;
    for (std::size_t i=0;i<roots.size();++i) {
        ChordEvent event;event.name=candidate.continuation[i].label;event.quality=candidate.continuation[i].quality;
        const auto chord=normalizeChord(event);
        if (!chord.root||int(*chord.root)!=roots[i]||chord.quality!=qualities[i]) return false;
    }
    return true;
}

void phase61CheckVoicing(const preview::Sequence& whole,const midi::ReadResult& decoded,bool suffixOnly) {
    require(bool(decoded)&&!decoded.file.notes.empty(),"Phase61 actual MIDI parses and contains notes");
    require(!whole.events.empty()&&(!suffixOnly||whole.recommendationBoundary<whole.events.size()),
        "Phase61 actual MIDI has a continuation boundary");
    preview::ChordVoicer voicer;
    const double suffixStart=whole.events[whole.recommendationBoundary].startQN;
    for (std::size_t i=0;i<whole.events.size();++i) {
        const auto& event=whole.events[i];const auto voiced=voicer.voice(event.chord,event.topVoice);
        if (suffixOnly&&i<whole.recommendationBoundary) continue;
        std::vector<int> expected{voiced.bass};
        for (int j=0;j<voiced.upperCount;++j) expected.push_back(voiced.upper[j]);
        std::vector<int> actual;
        const double start=event.startQN-(suffixOnly?suffixStart:0.);
        for (const auto& note:decoded.file.notes)
            if (closeQN(note.startQN,start)&&closeQN(note.durationQN,event.durationQN)) actual.push_back(note.pitch);
        std::sort(expected.begin(),expected.end());std::sort(actual.begin(),actual.end());
        require(actual==expected,"Phase61 actual MIDI pitch, onset and duration equal retained preview voicing");
    }
}

bool phase61WriteAudition(const std::filesystem::path& directory,const std::string& stem,
    const Progression& input,const ContinuationCandidate& candidate,const std::vector<MatchResult>& matches,
    const MatchQuery& query,std::ofstream& manifest,const char* kind,bool loopTwice,bool writeSuffixFiles=true) {
    std::filesystem::create_directories(directory);
    const auto importedSession=imported(input);
    const auto built=preview::buildSequence(importedSession,&candidate,120.);
    require(bool(built)&&built.sequence.recommendationBoundary==input.size(),
        "Phase61 retained candidate builds full input-plus-continuation preview");
    require(built.sequence.totalQN>0.,"Phase61 audible recommendation has a positive full-path QN duration");
    if (loopTwice) {
        require(phase61CandidateRepeatsInput(input,candidate),"Phase61 Loop audition is an exact second harmonic pass after the input cycle");
        require(built.sequence.events.size()==input.size()*2,"Phase61 Loop audition contains exactly two complete chord cycles");
    }
    midi::CandidateExportContext exportContext;exportContext.imported=importedSession;exportContext.tempo=120.;
    exportContext.meter={4,4};exportContext.key=candidate.key;exportContext.mode=midi::ArrangementMode::VoiceLed;
    exportContext.scope=midi::ExportScope::FullPhrase;
    const auto fullPayload=midi::candidatePayload(exportContext,candidate);
    exportContext.scope=midi::ExportScope::ContinuationOnly;
    const auto suffixPayload=midi::candidatePayload(exportContext,candidate);
    require(bool(fullPayload)&&bool(suffixPayload),"Phase61 named audition full and suffix MIDI payloads build");
    const auto fullRead=midi::readFromMemory(fullPayload.payload.smfBytes);
    const auto suffixRead=midi::readFromMemory(suffixPayload.payload.smfBytes);
    compareMidi(built.sequence,fullRead);
    phase61CheckVoicing(built.sequence,fullRead,false);
    preview::Sequence suffixSequence;suffixSequence.tempoBPM=built.sequence.tempoBPM;
    suffixSequence.sourceRecommendation=built.sequence.sourceRecommendation;suffixSequence.recommendationBoundary=0;
    const auto suffixStart=built.sequence.events[built.sequence.recommendationBoundary].startQN;
    for (std::size_t i=built.sequence.recommendationBoundary;i<built.sequence.events.size();++i) {
        auto event=built.sequence.events[i];event.startQN-=suffixStart;event.segment=preview::Segment::Recommended;
        suffixSequence.events.push_back(std::move(event));
    }
    suffixSequence.totalQN=built.sequence.totalQN-suffixStart;
    require(!suffixSequence.events.empty(),"Phase61 suffix audition contains all returned chords");
    compareMidi(suffixSequence,suffixRead);
    phase61CheckVoicing(built.sequence,suffixRead,true);

    const auto captured=snapshot::capture(importedSession,candidate,matches,120.,4,4,candidate.key,{},candidate.intent);
    const auto restored=snapshot::deserialize(snapshot::serialize(captured));
    require(bool(restored)&&restored.value.candidate.continuation.size()==candidate.continuation.size(),
        "Phase61 named candidate survives snapshot roundtrip");
    for (std::size_t i=0;i<candidate.continuation.size();++i) {
        const auto& before=candidate.continuation[i];const auto& after=restored.value.candidate.continuation[i];
        require(before.label==after.label&&before.quality==after.quality&&closeQN(before.durationQN,after.durationQN),
            "Phase61 snapshot preserves actual candidate chord names, qualities and QN values");
    }
    const auto restoredPreview=preview::buildSequence(importedSession,&restored.value.candidate,120.);
    require(bool(restoredPreview)&&closeQN(restoredPreview.sequence.totalQN,built.sequence.totalQN),
        "Phase61 restored snapshot rebuilds the same actual full-path duration");

    const auto fullWav=preview::renderOffline(built.sequence,48000.);
    require(!fullWav.left.empty()&&fullWav.left.size()==fullWav.right.size()&&fullWav.rms>0.,
        "Phase61 named full audition renders non-silent stereo WAV");
    const auto suffixFrame=preview::qnToSamples(suffixStart,built.sequence.tempoBPM,fullWav.sampleRate);
    require(suffixFrame>=0&&static_cast<std::size_t>(suffixFrame)<fullWav.left.size(),
        "Phase61 suffix boundary maps into full audition WAV");
    preview::AudioBuffer suffixWav;suffixWav.sampleRate=fullWav.sampleRate;
    suffixWav.left.assign(fullWav.left.begin()+suffixFrame,fullWav.left.end());
    suffixWav.right.assign(fullWav.right.begin()+suffixFrame,fullWav.right.end());
    long double squares{};
    for (const auto sample:suffixWav.left) {
        require(std::isfinite(sample),"Phase61 suffix WAV samples are finite");
        suffixWav.peak=std::max(suffixWav.peak,static_cast<double>(std::abs(sample)));
        squares+=static_cast<long double>(sample)*sample;
    }
    require(!suffixWav.left.empty(),"Phase61 suffix WAV excerpt is non-empty");
    suffixWav.rms=std::sqrt(static_cast<double>(squares/suffixWav.left.size()));
    require(suffixWav.rms>0.,"Phase61 suffix WAV excerpt is audible");

    const auto fullMidiPath=directory/(stem+"-full.mid"),suffixMidiPath=directory/(stem+"-suffix.mid");
    const auto fullWavPath=directory/(stem+"-full.wav"),suffixWavPath=directory/(stem+"-suffix.wav");
    std::string error;
    require(midi::writeToFile(fullPayload.payload.smfBytes,fullMidiPath,error),"Phase61 named full MIDI writes");error.clear();
    if (writeSuffixFiles) require(midi::writeToFile(suffixPayload.payload.smfBytes,suffixMidiPath,error),"Phase61 named suffix MIDI writes");
    error.clear();require(preview::writeWav16(fullWav,fullWavPath,error),"Phase61 named full WAV writes");error.clear();
    if (writeSuffixFiles) require(preview::writeWav16(suffixWav,suffixWavPath,error),"Phase61 named suffix WAV writes");
    const double cycleQN=std::accumulate(input.begin(),input.end(),0.0,[](double sum,const auto& chord){
        return sum+chord.durationQN.value_or(4.);
    });
    const auto [phraseStart,current]=phase61PhraseEndpoints(query,candidate);
    const auto completion=evaluateIntentCompletion(candidate,phraseStart,current);
    manifest<<'"'<<stem<<'"'<<'\t'<<kind<<'\t'<<candidate.primaryTemplate<<'\t'<<candidate.id<<'\t'
        <<intentName(candidate.intent)<<'\t'<<candidate.rankingScore<<'\t'<<completion.score<<'\t'<<completionReasonKey(
            completion.reason)<<'\t'<<'"'<<phase61InputPath(input)<<" -> "
        <<phase61ChordPath(candidate.continuation)<<'"'<<'\t'<<candidate.continuation.size()<<'\t'
        <<built.sequence.totalQN<<'\t'<<built.sequence.totalQN*60.0/built.sequence.tempoBPM<<'\t'
        <<cycleQN<<'\t'<<(loopTwice?2:0)<<'\t'<<fullRead.file.totalQN<<'\t'<<suffixRead.file.totalQN<<'\t'
        <<fullWavPath.string()<<'\t'<<fullMidiPath.string()<<'\t'<<(writeSuffixFiles?suffixWavPath.string():"NA")<<'\t'
        <<(writeSuffixFiles?suffixMidiPath.string():"NA")<<"\tMIDI_PITCH_QN=PASS;SNAPSHOT=PASS;PREVIEW=PASS\n";
    return true;
}

bool phase61HasTemplate(const ContinuationCandidate& candidate,const TemplateID& id) {
    return candidate.primaryTemplate==id ||
        std::find(candidate.supportingTemplates.begin(),candidate.supportingTemplates.end(),id)!=candidate.supportingTemplates.end();
}

int runDev9Consistency(const std::filesystem::path& out,const std::filesystem::path& factoryPath,bool strict) {
    std::filesystem::create_directories(out);
    std::ofstream log(out/"consistency.txt");
    plugin::RecommendationWorker worker(factoryPath,out/"isolated-user.db",out/"isolated-factory-store");
    int duplicates{};
    for(int test=0;test<8;++test) {
        const auto p=test==0?phrase({"Am","G#dim","G","D/F#"},{4,4,4,4},true):
            test<5?phrase({"F","G","Am"},{4,4,4},true):
            test<7?phrase({"C","Fm","G"},{2,2,2},true):phrase({"C","G","Am"},{4,4,4},true);
        AnalysisContext ctx;
        if(test==4)ctx.forcedKey=KeySignature{PitchClass::A,Mode::Minor};
        if(test==6)ctx.forcedKey=KeySignature{PitchClass::C,Mode::Major};
        const auto preset=test==2?plugin::DevelopmentAlgorithmPreset::HarmonyFirst:
            test==3?plugin::DevelopmentAlgorithmPreset::Abstract:plugin::DevelopmentAlgorithmPreset::Legacy;
        worker.submitPreset(p,ctx,preset,{},false,test+1);
        const auto r=wait(worker);const auto& rec=r.recommendations;
        require(r.factoryVersion==4&&r.factoryCount==657,"dev9 consistency frozen Factory");
        require(r.algorithmPreset==preset&&rec.retrievalTimingMode==plugin::developmentContext(ctx,preset).retrievalTimingMode&&
            rec.abstractChordCompatibility==(preset==plugin::DevelopmentAlgorithmPreset::Abstract),"actual preset dispatch");
        const auto defaults=session::presentationIndices(rec,{});
        log<<"\nCASE "<<test<<" input="<<phase61InputPath(p)<<" preset="<<plugin::algorithmPresetName(preset)
            <<" forced="<<(ctx.forcedKey?formatKey(*ctx.forcedKey):"AUTO")<<" selected="<<formatKey(r.analysis.selectedKey->key)<<'\n';
        for(const auto& k:r.analysis.keyCandidates)if(k.confidence>.05f)
            log<<"KEY "<<formatKey(k.key)<<" score="<<k.score<<" confidence="<<k.confidence<<'\n';
        std::vector<std::string> midiOutputs;
        bool classic{},completeLoop{},foreignColor{},dmLoop{};
        for(std::size_t g=0;g<4;++g)for(std::size_t n=0;n<rec.groups[g].size();++n) {
            const auto& c=rec.groups[g][n];
            const auto visible=[&](const auto& indices){return std::find(indices[g].begin(),indices[g].end(),n)!=indices[g].end();};
            log<<"CARD "<<intentName(c.intent)<<" rank="<<n+1<<" default="<<visible(defaults)<<" more=1"
                <<" primary="<<c.primaryTemplate<<" key="<<formatKey(c.key)<<" score="<<c.rankingScore
                <<" hold="<<c.suggestedCurrentChordDurationQN.value_or(-1.)<<" cadence="<<cadenceName(c.cadence)
                <<" support="<<c.supportCount<<" suffix="<<phase61ChordPath(c.continuation)<<" sources=";
            for(const auto& id:c.supportingTemplates)log<<id<<',';log<<'\n';
            if(ctx.forcedKey)require(c.key.tonic==ctx.forcedKey->tonic&&c.key.mode==ctx.forcedKey->mode,"fixed key reaches realization");
            if(test==0&&c.continuation.size()==1&&c.continuation.front().label=="Em") {
                if(strict)require(c.supportCount==6&&c.rankingScore<66.f,"merged provenance does not add another support bonus");
                midi::CandidateExportContext ec;ec.imported=imported(p);ec.tempo=120.;ec.meter={4,4};
                ec.mode=midi::ArrangementMode::VoiceLed;ec.scope=midi::ExportScope::FullPhrase;ec.key=c.key;
                const auto midi=midi::candidatePayload(ec,c);require(bool(midi),"actual Em MIDI export");
                const auto decoded=midi::readFromMemory(midi.payload.smfBytes);require(bool(decoded),"Em MIDI readback");
                std::ostringstream notes;
                for(const auto& note:decoded.file.notes)notes<<note.pitch<<'@'<<note.startQN<<'/'<<note.durationQN
                    <<':'<<note.velocity<<':'<<note.channel<<',';
                log<<"MIDI "<<notes.str()<<'\n';
                // Key metadata may differ; compare the rendered note events and times.
                if(std::find(midiOutputs.begin(),midiOutputs.end(),notes.str())!=midiOutputs.end())++duplicates;
                midiOutputs.push_back(notes.str());
            }
            if(g==3&&(c.key.tonic!=PitchClass::A||c.key.mode!=Mode::Minor))foreignColor=true;
            if(phase61TargetSuffix(c)&&visible(defaults))classic=true;
            if(c.intent==PhraseIntent::Loop&&c.continuation.size()==1&&c.continuation[0].label=="F"&&visible(defaults))completeLoop=true;
            if(c.intent==PhraseIntent::Loop&&c.continuation.size()==1&&c.continuation[0].label=="Dm"&&visible(defaults))dmLoop=true;
        }
        if(test>0&&test<4)require(foreignColor,"Auto Color includes another matching key");
        if(test==4)require(rec.groups[3].empty(),"fixed A minor Color difference is accounted for");
        if(test==7)require(classic&&completeLoop,"classic continuation and F loop completion remain visible");
        if(strict&&(test==5||test==6))require(dmLoop,"borrowed iv keeps valid Dm-to-C loop boundary");
        if(test==5||test==6)for(const auto& m:rec.matches)if(m.templateId=="V3_CYCLE_016") {
            log<<"DM_TRACE key="<<formatKey(m.key)<<" disposition="<<m.disposition<<" match="<<m.similarity
                <<" ranking="<<m.candidateRankingScore<<" eligibility="<<m.candidateEligibilityScore<<'\n';
            for(const auto& s:m.alignmentTrace)log<<alignmentOpName(s.operation)<<" q="<<(s.queryIndex?std::to_string(*s.queryIndex):"-")
                <<" t="<<(s.templateIndex?std::to_string(*s.templateIndex):"-")<<" reasons="<<reasonNames(s.reasons)<<'\n';
        }
    }
    log<<"duplicateMusic="<<duplicates<<'\n';log.flush();require(bool(log),"consistency evidence written");
    if(strict)require(duplicates==0,"same actual Em MIDI must occupy only one recommendation");
    std::cout<<"dev9 consistency cases=8 duplicateMusic="<<duplicates<<'\n';return 0;
}

int runPhase62(const std::filesystem::path& out,const std::filesystem::path& factoryPath,bool strict) {
    std::filesystem::create_directories(out);
    std::ofstream log(out/"focused.txt");
    require(bool(log),"Phase62 focused output opens");
    plugin::RecommendationWorker worker(factoryPath,out/"isolated-user.db",out/"isolated-factory-store");
    const auto labels=[](const ContinuationCandidate& c) {
        std::string s;for(const auto& e:c.continuation){if(!s.empty())s+=' ';s+=e.label;}return s;
    };
    int classificationFailures{};
    for(int test=0;test<4;++test) {
        const auto p=test==0?phrase({"Am","G#dim","G","D/F#"},{4,4,4,4},true):
            test==1?phrase({"C","G","Am"},{4,4,4},true):
            test==2?phrase({"C","G","Am","F"},{4,4,4,4},false):
                    phrase({"C","Fm","G"},{4,4,4},true);
        for(const auto preset:{plugin::DevelopmentAlgorithmPreset::Legacy,
                plugin::DevelopmentAlgorithmPreset::HarmonyFirst,plugin::DevelopmentAlgorithmPreset::Abstract}) {
            if(test!=0&&preset!=plugin::DevelopmentAlgorithmPreset::Legacy)continue;
            AnalysisContext ctx;
            if(test!=0){ctx.timeSigNumerator=4;ctx.timeSigDenominator=4;}
            worker.submitPreset(p,ctx,preset,{},false,static_cast<std::uint64_t>(test+1));
            const auto r=wait(worker);const auto& rec=r.recommendations;
            const auto shown=session::presentationIndices(rec,{});
            log<<"INPUT "<<phase61InputPath(p)<<" preset="<<plugin::algorithmPresetName(preset)
                <<" selected="<<formatKey(r.analysis.selectedKey->key)<<" FULL="<<r.analysis.full.size()
                <<" SKELETON="<<r.analysis.skeletonIndices.size()<<'\n';
            require(r.factoryVersion==4&&r.factoryCount==657,"Phase62 frozen Factory V4/657");
            for(std::size_t i=0;i<std::min<std::size_t>(5,r.analysis.keyCandidates.size());++i) {
                const auto& k=r.analysis.keyCandidates[i];log<<"KEY "<<formatKey(k.key)<<" score="<<k.score<<" confidence="<<k.confidence<<'\n';
            }
            std::array<bool,3> foundTargets{};bool classic{},cycle{},dm{};
            for(std::size_t g=0;g<4;++g)for(std::size_t n=0;n<rec.groups[g].size();++n) {
                const auto& c=rec.groups[g][n];const auto path=labels(c);
                const bool visible=std::find(shown[g].begin(),shown[g].end(),n)!=shown[g].end();
                log<<"CARD "<<intentName(c.intent)<<" rank="<<n+1<<" default="<<visible<<" primary="<<c.primaryTemplate
                    <<" key="<<formatKey(c.key)<<" match="<<c.matchSimilarity<<" score="<<c.rankingScore
                    <<" cadence="<<cadenceName(c.cadence)<<" suffix="<<path<<'\n';
                if(test==0&&(path=="G C Em"||path=="C G Em"||path=="Em")) {
                    foundTargets[path=="G C Em"?0:path=="C G Em"?1:2]=true;
                    if(c.intent==PhraseIntent::Loop)++classificationFailures;
                    for(const auto& m:rec.matches)if(m.templateId==c.primaryTemplate) {
                        log<<"FOCUS rawCost="<<m.rawCost<<" full="<<m.subScores.fullHarmony<<" skeleton="<<m.subScores.skeletonHarmony
                            <<" cadenceSubscore="<<c.subscores.cadence<<" support="<<c.supportCount<<'\n';
                        for(const auto& s:m.alignmentTrace)log<<alignmentOpName(s.operation)<<" q="<<(s.queryIndex?std::to_string(*s.queryIndex):"-")
                            <<" t="<<(s.templateIndex?std::to_string(*s.templateIndex):"-")<<" reasons="<<reasonNames(s.reasons)<<'\n';
                    }
                }
                if(path=="Em F G C"&&visible)classic=true;
                if(path=="C G Am F"&&c.intent==PhraseIntent::Loop&&visible)cycle=true;
                if(test==3&&path=="Dm") {dm=c.intent==PhraseIntent::Loop;}
            }
            if(test==0)require(std::all_of(foundTargets.begin(),foundTargets.end(),[](bool found){return found;}),
                "Phase62 retains each of the three actual R01 routes");
            if(test==1)require(classic,"Phase62 C-G-Am to Em-F-G-C remains default-visible");
            if(test==2)require(cycle,"Phase62 complete 1564 loop still returns one full next round");
            if(test==3)require(dm,"Phase62 internal borrowed iv preserves the Dm loop route");
        }
    }
    log<<"loopClassificationFailures="<<classificationFailures<<'\n';log.flush();
    if(strict)require(classificationFailures==0,"Phase62 approximate structural replacements must not inherit Loop qualification");
    std::cout<<"Phase62 focused cases=6 loopClassificationFailures="<<classificationFailures<<'\n';return 0;
}

int runPhase61(const std::filesystem::path& out,const std::filesystem::path& factoryPath,bool strictRequested) {
    std::filesystem::create_directories(out);
    const auto db=library::loadFactory(factoryPath);
    require(bool(db),"Phase61 Factory V4 database loads");
    require(db.templates.size()==657,"Phase61 baseline uses the unmodified 657-entry Factory V4");
    constexpr const char* targetId="FUNCTIONAL_016";
    const auto target=std::find_if(db.templates.begin(),db.templates.end(),[](const auto& item){return item.id=="FUNCTIONAL_016";});

    std::ofstream factory(out/"phase61-factory-target.tsv");
    require(bool(factory),"Phase61 Factory target report opens");
    factory<<"factoryPath\ttemplateCount\tfactorySha256\ttargetPresent\ttargetId\tname\taliases\tmode\tintent\tloopable\tfullPathInCMajor\n";
    factory<<factoryPath.string()<<'\t'<<db.templates.size()<<'\t'<<phase61FactoryHash<<'\t'<<(target!=db.templates.end())<<'\t'<<targetId<<'\t';
    if (target!=db.templates.end()) {
        for (char c:target->name) factory<<(c=='\t'?' ':c);
        factory<<'\t';
        for (std::size_t i=0;i<target->aliases.size();++i) {
            if (i) factory<<';';
            for (char c:target->aliases[i]) factory<<(c=='\t'?' ':c);
        }
        factory<<'\t'<<int(target->mode)<<'\t'<<int(target->intent)<<'\t'<<target->loopable<<'\t';
        const KeySignature cMajor{PitchClass::C,Mode::Major};
        for (std::size_t i=0;i<target->full.size();++i) {
            if (i) factory<<" -> ";
            const auto& event=target->full[i];
            factory<<realizeContinuation(event,cMajor,event.durationQN.value_or(4.)).label;
        }
    } else factory<<"\t\t\t\t\t";
    factory<<'\n';

    std::ofstream matrix(out/"phase61-c-g-am-baseline.tsv");
    std::ofstream traces(out/"phase61-c-g-am-traces.txt");
    const auto mediaDirectory=out/"phase61-media";
    std::filesystem::create_directories(mediaDirectory);
    std::ofstream mediaIndex(out/"phase61-media-index.tsv");
    require(bool(matrix)&&bool(traces)&&bool(mediaIndex),"Phase61 baseline outputs open");
    matrix<<"case\tpreset\tkeySetting\tstyleSetting\tdurationQN\topen\tinputRecognized\tselectedKey\tfullEvents\tskeletonEvents"
        "\tshortlist\taligned\tsimilarityPassed\ttailPassed\trealizationPassed\tpathPassed\tdistinct\tscorePassed\tworkerOutput"
        "\ttargetInMatchResults\ttargetMatchRows\ttargetBestKey\ttargetSimilarity\ttargetRawCost\ttargetFullScore\ttargetSkeletonScore"
        "\ttargetRhythmScore\ttargetDisposition\ttargetCandidateRetained\ttargetCandidateGroup\ttargetCandidateRank"
        "\ttargetDefaultTop3\ttargetMoreButtonAvailable\ttargetCandidateScore\tactualCandidatePath\n";
    traces<<"Phase61 C-G-Am baseline; supplied Factory="<<factoryPath.string()<<"; entries="<<db.templates.size()
        <<"; expectedFactorySha256="<<phase61FactoryHash
        <<"; FUNCTIONAL_016="<<(target!=db.templates.end()?"present":"absent")
        <<"; optionalStrict="<<(strictRequested?"requested":"off")
        <<"; strict requires the FUNCTIONAL_016 target in default Top3 with Em-F-G-C on every C-G-Am matrix cell.\n";
    mediaIndex<<"sample\tkind\tprimaryTemplate\tcandidateId\tintent\tscore\tcompletionScore\tcompletionReason\tactualChordPath\tsuffixChordCount\tfullTotalQN\tfullSecondsAt120BPM\tinputCycleQN\tcycles\tfullMidiQN\tsuffixMidiQN\tfullWav\tfullMidi\tsuffixWav\tsuffixMidi\tvalidation\n";

    constexpr std::array presets{
        plugin::DevelopmentAlgorithmPreset::Legacy,
        plugin::DevelopmentAlgorithmPreset::HarmonyFirst,
        plugin::DevelopmentAlgorithmPreset::Abstract};
    constexpr const char* keyNames[]{"Auto","Cmaj"};
    constexpr const char* styleNames[]{"Auto","Pop"};
    std::size_t caseNumber{},targetMatchRows{},targetRetainedRows{},targetDefaultRows{};
    plugin::RecommendationWorker worker(factoryPath,out/"phase61-isolated-user.db",out/"phase61-isolated-factory-store");
    for (int keySetting=0;keySetting<2;++keySetting)
    for (int styleSetting=0;styleSetting<2;++styleSetting)
    for (double duration:{2.,4.})
    for (bool open:{false,true})
    for (const auto preset:presets) {
        ++caseNumber;
        auto source=phrase({"C","G","Am"},{duration,duration,duration},open);
        auto input=symbolic(source);
        AnalysisContext context;
        context.timeSigNumerator=4;
        context.timeSigDenominator=4;
        if (keySetting==1) context.forcedKey=KeySignature{PitchClass::C,Mode::Major};
        RecommendationRequest request;
        if (styleSetting==1) request.style=Style::Pop;
        const auto effectiveContext=plugin::developmentContext(context,preset);
        worker.submitPreset(input,effectiveContext,preset,request,false,caseNumber);
        auto result=wait(worker);
        require(result.factoryVersion==4&&result.factoryCount==657&&result.factorySha256==phase61FactoryHash,
            "Phase61 worker uses the supplied frozen V4/657 Factory with the expected SHA-256");
        const auto& recommendations=result.recommendations;
        const auto visibility=session::presentationIndices(recommendations,{});
        const MatchResult* bestMatch{};
        std::size_t targetRowsForCase{};
        for (const auto& match:recommendations.matches) {
            if (match.templateId!=targetId) continue;
            ++targetRowsForCase;
            if (!bestMatch || match.candidateRankingScore>bestMatch->candidateRankingScore ||
                (match.candidateRankingScore==bestMatch->candidateRankingScore && match.similarity>bestMatch->similarity))
                bestMatch=&match;
        }
        targetMatchRows+=targetRowsForCase;

        const ContinuationCandidate* targetCandidate{};
        std::size_t targetGroup{},targetIndex{};
        bool targetDefault{};
        bool targetMore{};
        for (std::size_t group=0;group<recommendations.groups.size();++group) {
            for (std::size_t index=0;index<recommendations.groups[group].size();++index) {
                const auto& candidate=recommendations.groups[group][index];
                if (!phase61HasTemplate(candidate,targetId)) continue;
                targetCandidate=&candidate;targetGroup=group;targetIndex=index;
                targetDefault=std::find(visibility[group].begin(),visibility[group].end(),index)!=visibility[group].end();
                targetMore=recommendations.groups[group].size()>2;
                break;
            }
            if (targetCandidate) break;
        }
        if (targetCandidate) ++targetRetainedRows;
        if (targetDefault) ++targetDefaultRows;

        bool inputRecognized=input.size()==3;
        constexpr std::array<int,3> expectedRoots{0,7,9};
        constexpr std::array<ChordQuality,3> expectedQualities{ChordQuality::Major,ChordQuality::Major,ChordQuality::Minor};
        for (std::size_t chordIndex=0;chordIndex<input.size();++chordIndex) {
            const auto& chord=input[chordIndex];
            const auto normalized=normalizeChord(chord);
            inputRecognized=inputRecognized&&normalized.root&&int(*normalized.root)==expectedRoots[chordIndex]&&
                normalized.quality==expectedQualities[chordIndex];
        }
        require(inputRecognized,"Phase61 C-G-Am synthetic input parses as exact C major, G major, A minor");
        const std::string selectedKey=result.analysis.selectedKey?formatKey(result.analysis.selectedKey->key):"UNKNOWN";
        std::string candidatePath;
        if (targetCandidate) {
            for (const auto& chord:input) {
                if (!candidatePath.empty()) candidatePath+=" -> ";
                candidatePath+=chord.name;
            }
            if (!targetCandidate->continuation.empty()) candidatePath+=" -> "+phase61ChordPath(targetCandidate->continuation);
        }
        if (strictRequested) {
            require(targetCandidate&&targetDefault,
                "Phase61 strict requires FUNCTIONAL_016 retained and visible in default Top3 for every matrix cell");
            require(phase61TargetSuffix(*targetCandidate),
                "Phase61 strict requires the actual Em-F-G-C continuation for every matrix cell");
        }
        if (keySetting==1&&styleSetting==0&&duration==4.&&!open&&preset==plugin::DevelopmentAlgorithmPreset::Legacy) {
            if (targetCandidate) {
                const auto targetQuery=makeMatchQuery(input,effectiveContext);
                phase61WriteAudition(mediaDirectory,"target-C-G-Am-FUNCTIONAL_016",input,*targetCandidate,
                    recommendations.matches,targetQuery,mediaIndex,"target",false);
            } else {
                mediaIndex<<"\"target-C-G-Am-FUNCTIONAL_016\"\tmissing-target\tFUNCTIONAL_016\tNA\tNA\t-1\t-1\tNA\t\""
                    <<phase61InputPath(input)<<"\"\t0\t0\t0\t12\t0\t-1\t-1\tNA\tNA\tNA\tNA\tNOT_RETAINED\n";
            }
        }
        const auto missing=std::string("NA");
        matrix<<caseNumber<<'\t'<<plugin::algorithmPresetName(preset)<<'\t'<<keyNames[keySetting]<<'\t'<<styleNames[styleSetting]
            <<'\t'<<duration<<'\t'<<open<<'\t'<<inputRecognized<<'\t'<<selectedKey<<'\t'<<result.analysis.full.size()
            <<'\t'<<result.analysis.skeletonIndices.size()<<'\t'<<recommendations.stats.shortlistSize<<'\t'<<recommendations.stats.aligned
            <<'\t'<<recommendations.stats.similarityPassed<<'\t'<<recommendations.stats.tailPassed<<'\t'<<recommendations.stats.realizationPassed
            <<'\t'<<recommendations.stats.pathPassed<<'\t'<<recommendations.stats.distinct<<'\t'<<recommendations.stats.scorePassed
            <<'\t'<<recommendations.stats.output<<'\t'<<(targetRowsForCase!=0)<<'\t'<<targetRowsForCase<<'\t'
            <<(bestMatch?formatKey(bestMatch->key):missing)<<'\t'<<(bestMatch?bestMatch->similarity:-1.f)<<'\t'
            <<(bestMatch?bestMatch->rawCost:-1.f)<<'\t'<<(bestMatch?bestMatch->subScores.fullHarmony:-1.f)<<'\t'
            <<(bestMatch?bestMatch->subScores.skeletonHarmony:-1.f)<<'\t'<<(bestMatch?bestMatch->subScores.rhythmSimilarity:-1.f)<<'\t'
            <<(bestMatch?bestMatch->disposition:missing)<<'\t'<<(targetCandidate!=nullptr)<<'\t'
            <<(targetCandidate?std::to_string(targetGroup):missing)<<'\t'<<(targetCandidate?std::to_string(targetIndex+1):missing)
            <<'\t'<<targetDefault<<'\t'<<targetMore<<'\t'<<(targetCandidate?targetCandidate->rankingScore:-1.f)<<'\t'
            <<'"'<<candidatePath<<'"'<<'\n';

        traces<<"\nCASE="<<caseNumber<<" preset="<<plugin::algorithmPresetName(preset)<<" keySetting="<<keyNames[keySetting]
            <<" styleSetting="<<styleNames[styleSetting]<<" durationQN="<<duration<<" OPEN="<<open
            <<" analysisReused="<<result.analysisReused<<" generation="<<result.generation<<'\n'
            <<session::recommendationDiagnostics(input,effectiveContext,request,recommendations);
    }
    matrix.flush();traces.flush();
    require(bool(matrix)&&bool(traces)&&bool(mediaIndex),"Phase61 baseline outputs flush");

    // Medium corpus: worker-backed retrieval on the same isolated V4 store.
    // The traces preserve every MatchResult/alignment and every retained card;
    // this is measurement only and does not assign any musical preference score.
    struct Scenario { const char* id; std::vector<std::string> chords; std::vector<double> durations; bool open{}; };
    const std::array<Scenario,12> scenarios{{
        {"C-Am-F-G",{"C","Am","F","G"},{4.,4.,4.,4.}},
        {"Am-F-C-G",{"Am","F","C","G"},{4.,4.,4.,4.}},
        {"C-G-Am-F",{"C","G","Am","F"},{4.,4.,4.,4.}},
        {"C-F-Am-G",{"C","F","Am","G"},{4.,4.,4.,4.}},
        {"F-G-Am",{"F","G","Am"},{4.,4.,4.}},
        {"Am-Gsharpdim-G-DslashFsharp",{"Am","G#dim","G","D/F#"},{4.,4.,4.,4.}},
        {"CslashE-GslashD-AmslashC",{"C/E","G/D","Am/C"},{4.,4.,4.}},
        {"Cmaj7-G7-Am7",{"Cmaj7","G7","Am7"},{4.,4.,4.}},
        {"C-G-Am-syncopated",{"C","G","Am"},{1.5,.5,2.}},
        {"Am-Gsharpdim-G-DslashFsharp-OPEN",{"Am","G#dim","G","D/F#"},{4.,4.,4.,4.},true},
        {"F-G-Am-OPEN",{"F","G","Am"},{4.,4.,4.},true},
        {"F3-G5-AmOPEN",{"F","G","Am"},{3.,5.,4.},true}
    }};
    struct KeySetting { const char* name; std::optional<KeySignature> key; };
    const std::array<KeySetting,3> benchKeys{{
        {"Auto",std::nullopt},
        {"Cmaj",KeySignature{PitchClass::C,Mode::Major}},
        {"Amin",KeySignature{PitchClass::A,Mode::Minor}}
    }};
    std::ofstream benchCases(out/"phase61-medium-cases.tsv"),benchMatches(out/"phase61-medium-matches.tsv"),
        benchCandidates(out/"phase61-medium-candidates.tsv"),benchCoverage(out/"phase61-medium-factory-coverage.tsv"),
        benchTrace(out/"phase61-medium-traces.txt");
    require(bool(benchCases)&&bool(benchMatches)&&bool(benchCandidates)&&bool(benchCoverage)&&bool(benchTrace),
        "Phase61 medium benchmark outputs open");
    benchCases<<"case\tscenario\tpreset\tkeySetting\tstyleSetting\tinputPath\tselectedKey\tfactoryVersion\tfactorySha256\tfactoryCount\tfullEvents\tskeletonEvents\tshortlist\taligned\tsimilarityPassed\ttailPassed\trealizationPassed\tpathPassed\tdistinct\tscorePassed\tworkerOutput\tmatchedRows\tretainedCandidates\tdefaultCards\n";
    benchMatches<<"case\tscenario\tpreset\tkeySetting\tstyleSetting\ttemplateId\tkey\tsimilarity\trawCost\tfullHarmony\tskeletonHarmony\trhythmSimilarity\tcandidateRankingScore\teligibilityScore\tdisposition\tmatchStart\tmatchEnd\tcontinuationStart\tcontinuationLength\talignmentTrace\n";
    benchCandidates<<"case\tscenario\tpreset\tkeySetting\tstyleSetting\tgroup\trank\tdefaultVisible\tmoreAccessible\tcandidateId\tprimaryId\tsupportingIds\tintent\tcadence\trankingScore\teligibilityScore\tcompletionScore\tcompletionReason\tfullPath\n";
    benchCoverage<<"case\tscenario\tpreset\tkeySetting\tstyleSetting\tfactoryTemplateId\tmatchRowCount\tbestMatchKey\tbestSimilarity\tbestDisposition\tretainedCandidateCount\tcandidateDetails\n";
    benchTrace<<"Phase61 medium corpus: worker-backed, exact Factory V4 identity required per row; the coverage TSV has one row for every Factory ID in every case, every returned MatchResult and retained candidate is listed in the TSVs, and full alignment diagnostics follow. Classification and completion fields are diagnostic metadata only; no musical quality score is assigned.\n";
    benchCases<<std::setprecision(9);benchMatches<<std::setprecision(9);
    benchCandidates<<std::setprecision(9);benchCoverage<<std::setprecision(9);
    std::size_t benchCaseNumber{},benchCoverageRows{};
    plugin::RecommendationWorker benchWorker(factoryPath,out/"phase61-medium-isolated-user.db",
        out/"phase61-medium-isolated-factory-store");
    for (const auto& scenario:scenarios) for (const auto& keySetting:benchKeys)
    for (int styleSetting=0;styleSetting<2;++styleSetting) for (const auto preset:presets) {
        ++benchCaseNumber;
        const auto source=phrase(scenario.chords,scenario.durations,scenario.open);
        const auto input=symbolic(source);
        AnalysisContext context;context.timeSigNumerator=4;context.timeSigDenominator=4;context.forcedKey=keySetting.key;
        RecommendationRequest request;if(styleSetting==1)request.style=Style::Pop;
        const auto effectiveContext=plugin::developmentContext(context,preset);
        benchWorker.submitPreset(input,effectiveContext,preset,request,false,benchCaseNumber);
        const auto result=wait(benchWorker);
        require(result.factoryVersion==4&&result.factoryCount==657&&result.factorySha256==phase61FactoryHash,
            "Phase61 medium worker uses the supplied frozen V4/657 Factory with the expected SHA-256");
        const auto& recommendations=result.recommendations;
        const auto visibility=session::presentationIndices(recommendations,{});
        const auto caseId="M"+std::to_string(benchCaseNumber);
        const auto selectedKey=result.analysis.selectedKey?formatKey(result.analysis.selectedKey->key):"UNKNOWN";
        const auto query=makeMatchQuery(input,effectiveContext);
        std::unordered_map<TemplateID,std::vector<const MatchResult*>> matchesByTemplate;
        for (const auto& match:recommendations.matches) matchesByTemplate[match.templateId].push_back(&match);
        std::unordered_map<TemplateID,std::vector<std::string>> candidatesByTemplate;
        for (std::size_t group=0;group<recommendations.groups.size();++group) {
            for (std::size_t rank=0;rank<recommendations.groups[group].size();++rank) {
                const auto& candidate=recommendations.groups[group][rank];
                const bool visible=std::find(visibility[group].begin(),visibility[group].end(),rank)!=visibility[group].end();
                std::ostringstream detail;detail<<group<<':'<<rank+1<<':'<<visible<<':'<<candidate.id<<':'<<candidate.rankingScore<<':'
                    <<phase61InputPath(input)<<" -> "<<phase61ChordPath(candidate.continuation);
                auto add=[&](const TemplateID& id) {
                    auto& rows=candidatesByTemplate[id];
                    if (std::find(rows.begin(),rows.end(),detail.str())==rows.end()) rows.push_back(detail.str());
                };
                add(candidate.primaryTemplate);
                for (const auto& id:candidate.supportingTemplates) add(id);
            }
        }
        std::size_t factoryCoverageRows{};
        for (const auto& item:db.templates) {
            ++factoryCoverageRows;
            const auto matchesIt=matchesByTemplate.find(item.id);
            const auto candidatesIt=candidatesByTemplate.find(item.id);
            const MatchResult* best{};
            if (matchesIt!=matchesByTemplate.end()) for (const auto* match:matchesIt->second)
                if (!best||match->candidateRankingScore>best->candidateRankingScore||
                    (match->candidateRankingScore==best->candidateRankingScore&&match->similarity>best->similarity)) best=match;
            benchCoverage<<caseId<<'\t'<<scenario.id<<'\t'<<plugin::algorithmPresetName(preset)<<'\t'<<keySetting.name
                <<'\t'<<(styleSetting?"Pop":"Auto")<<'\t'<<item.id<<'\t'
                <<(matchesIt==matchesByTemplate.end()?0:matchesIt->second.size())<<'\t'
                <<(best?formatKey(best->key):"NA")<<'\t'<<(best?best->similarity:-1.f)<<'\t'
                <<(best?best->disposition:"no_match_result")<<'\t'
                <<(candidatesIt==candidatesByTemplate.end()?0:candidatesIt->second.size())<<'\t'<<'"';
            if (candidatesIt!=candidatesByTemplate.end())
                for (std::size_t i=0;i<candidatesIt->second.size();++i) {
                    if (i) benchCoverage<<';';
                    benchCoverage<<candidatesIt->second[i];
                }
            benchCoverage<<'"'<<'\n';
        }
        require(factoryCoverageRows==657,"Phase61 medium case writes a coverage row for every Factory V4 template");
        benchCoverageRows+=factoryCoverageRows;
        std::size_t candidateCount{},defaultCount{};
        for (std::size_t group=0;group<recommendations.groups.size();++group) {
            for (std::size_t rank=0;rank<recommendations.groups[group].size();++rank) {
                const auto& candidate=recommendations.groups[group][rank];++candidateCount;
                const bool visible=std::find(visibility[group].begin(),visibility[group].end(),rank)!=visibility[group].end();
                if (visible) ++defaultCount;
                const auto [phraseStart,current]=phase61PhraseEndpoints(query,candidate);
                const auto completion=evaluateIntentCompletion(candidate,phraseStart,current);
                benchCandidates<<caseId<<'\t'<<scenario.id<<'\t'<<plugin::algorithmPresetName(preset)<<'\t'<<keySetting.name
                    <<'\t'<<(styleSetting?"Pop":"Auto")<<'\t'<<group<<'\t'<<rank+1<<'\t'<<visible<<"\t1\t"
                    <<candidate.id<<'\t'<<candidate.primaryTemplate<<'\t';
                for (std::size_t i=0;i<candidate.supportingTemplates.size();++i) {
                    if (i) benchCandidates<<';';
                    benchCandidates<<candidate.supportingTemplates[i];
                }
                benchCandidates<<'\t'<<intentName(candidate.intent)<<'\t'<<cadenceName(candidate.cadence)<<'\t'
                    <<candidate.rankingScore<<'\t'<<candidate.eligibilityScore<<'\t'<<completion.score<<'\t'
                    <<completionReasonKey(completion.reason)<<'\t'<<'"'<<phase61InputPath(input)<<" -> "
                    <<phase61ChordPath(candidate.continuation)<<'"'<<'\n';
            }
        }
        for (const auto& match:recommendations.matches) {
            std::ostringstream alignment;
            for (std::size_t i=0;i<match.alignmentTrace.size();++i) {
                if (i) alignment<<';';
                const auto& step=match.alignmentTrace[i];
                alignment<<"q"<<(step.queryIndex?std::to_string(*step.queryIndex):"-")
                    <<"/t"<<(step.templateIndex?std::to_string(*step.templateIndex):"-")
                    <<'/'<<alignmentOpName(step.operation)<<'/'<<step.cost<<'/'<<reasonNames(step.reasons);
            }
            benchMatches<<caseId<<'\t'<<scenario.id<<'\t'<<plugin::algorithmPresetName(preset)<<'\t'<<keySetting.name
                <<'\t'<<(styleSetting?"Pop":"Auto")<<'\t'<<match.templateId<<'\t'<<formatKey(match.key)<<'\t'
                <<match.similarity<<'\t'<<match.rawCost<<'\t'<<match.subScores.fullHarmony<<'\t'
                <<match.subScores.skeletonHarmony<<'\t'<<match.subScores.rhythmSimilarity<<'\t'
                <<match.candidateRankingScore<<'\t'<<match.candidateEligibilityScore<<'\t'<<match.disposition<<'\t'
                <<match.templateMatchStart<<'\t'<<match.templateMatchEnd<<'\t'<<match.continuationStartIndex<<'\t'
                <<match.continuationLength<<'\t'<<'"'<<alignment.str()<<'"'<<'\n';
        }
        benchCases<<caseId<<'\t'<<scenario.id<<'\t'<<plugin::algorithmPresetName(preset)<<'\t'<<keySetting.name<<'\t'
            <<(styleSetting?"Pop":"Auto")<<'\t'<<'"'<<phase61InputPath(input)<<'"'<<'\t'<<selectedKey<<'\t'
            <<result.factoryVersion<<'\t'<<result.factorySha256<<'\t'<<result.factoryCount<<'\t'
            <<result.analysis.full.size()<<'\t'<<result.analysis.skeletonIndices.size()<<'\t'
            <<recommendations.stats.shortlistSize<<'\t'<<recommendations.stats.aligned<<'\t'
            <<recommendations.stats.similarityPassed<<'\t'<<recommendations.stats.tailPassed<<'\t'
            <<recommendations.stats.realizationPassed<<'\t'<<recommendations.stats.pathPassed<<'\t'
            <<recommendations.stats.distinct<<'\t'<<recommendations.stats.scorePassed<<'\t'
            <<recommendations.stats.output<<'\t'<<recommendations.matches.size()<<'\t'<<candidateCount<<'\t'<<defaultCount<<'\n';
        benchTrace<<"\nCASE="<<caseId<<" scenario="<<scenario.id<<" preset="<<plugin::algorithmPresetName(preset)
            <<" keySetting="<<keySetting.name<<" styleSetting="<<(styleSetting?"Pop":"Auto")
            <<" selectedKey="<<selectedKey<<" factoryHash="<<result.factorySha256<<'\n'
            <<session::recommendationDiagnostics(input,effectiveContext,request,recommendations);

        const auto isAuditionPreset=preset==plugin::DevelopmentAlgorithmPreset::Legacy&&styleSetting==0;
        if (isAuditionPreset&&std::string_view(scenario.id)=="C-Am-F-G"&&std::string_view(keySetting.name)=="Cmaj") {
            const ContinuationCandidate* loop{};
            for (const auto& candidate:recommendations.groups[2])
                if (candidate.intent==PhraseIntent::Loop&&phase61CandidateRepeatsInput(input,candidate)) {loop=&candidate;break;}
            if (loop) phase61WriteAudition(mediaDirectory,"loop-C-Am-F-G-two-cycles",input,*loop,
                recommendations.matches,query,mediaIndex,"loop-two-complete-cycles",true,false);
            else mediaIndex<<"\"loop-C-Am-F-G-two-cycles\"\tloop-unavailable\tNA\tNA\tLoop\t-1\t-1\tNA\t\""
                <<phase61InputPath(input)<<"\"\t0\t0\t0\t16\t0\t-1\t-1\tNA\tNA\tNA\tNA\tNO_EXACT_LOOP_CANDIDATE\n";
        }
        if (isAuditionPreset&&std::string_view(scenario.id)=="Am-F-C-G"&&std::string_view(keySetting.name)=="Amin") {
            const ContinuationCandidate* loop{};
            for (const auto& candidate:recommendations.groups[2])
                if (candidate.intent==PhraseIntent::Loop&&phase61CandidateRepeatsInput(input,candidate)) {loop=&candidate;break;}
            if (loop) phase61WriteAudition(mediaDirectory,"loop-Am-F-C-G-two-cycles",input,*loop,
                recommendations.matches,query,mediaIndex,"loop-two-complete-cycles",true,false);
            else mediaIndex<<"\"loop-Am-F-C-G-two-cycles\"\tloop-unavailable\tNA\tNA\tLoop\t-1\t-1\tNA\t\""
                <<phase61InputPath(input)<<"\"\t0\t0\t0\t16\t0\t-1\t-1\tNA\tNA\tNA\tNA\tNO_EXACT_LOOP_CANDIDATE\n";
        }
        if (preset==plugin::DevelopmentAlgorithmPreset::HarmonyFirst&&styleSetting==0&&
            std::string_view(scenario.id)=="F-G-Am-OPEN"&&std::string_view(keySetting.name)=="Auto") {
            const ContinuationCandidate* color{};
            for (const auto& candidate:recommendations.groups[3])
                if (candidate.intent==PhraseIntent::Color) {color=&candidate;break;}
            if (color) phase61WriteAudition(mediaDirectory,"color-F-G-Am-open-Auto-HarmonyFirst",input,*color,
                recommendations.matches,query,mediaIndex,"F-G-Am-open-Auto-HarmonyFirst-Color",false,true);
            else mediaIndex<<"\"color-F-G-Am-open-Auto-HarmonyFirst\"\tcolor-unavailable\tNA\tNA\tColor\t-1\t-1\tNA\t\""
                <<phase61InputPath(input)<<"\"\t0\t0\t0\t12\t0\t-1\t-1\tNA\tNA\tNA\tNA\tNO_COLOR_CANDIDATE\n";
        }
        if (isAuditionPreset&&std::string_view(scenario.id)=="Am-Gsharpdim-G-DslashFsharp-OPEN"&&
            std::string_view(keySetting.name)=="Auto") {
            const ContinuationCandidate* firstDefault{};float firstScore=-1.f;
            for (std::size_t group=0;group<recommendations.groups.size();++group)
                for (const auto rank:visibility[group]) if (rank<recommendations.groups[group].size()) {
                    const auto& candidate=recommendations.groups[group][rank];
                    if (!firstDefault||candidate.rankingScore>firstScore) {firstDefault=&candidate;firstScore=candidate.rankingScore;}
                }
            if (firstDefault) phase61WriteAudition(mediaDirectory,"r01-open-Am-Gsharpdim-G-DslashFsharp-default",input,*firstDefault,
                recommendations.matches,query,mediaIndex,"R01-OPEN-first-default-candidate",false,false);
            else mediaIndex<<"\"r01-open-Am-Gsharpdim-G-DslashFsharp-default\"\tr01-open-unavailable\tNA\tNA\tNA\t-1\t-1\tNA\t\""
                <<phase61InputPath(input)<<"\"\t0\t0\t0\t16\t0\t-1\t-1\tNA\tNA\tNA\tNA\tNO_DEFAULT_CANDIDATE\n";
        }
    }
    require(benchCaseNumber==216,"Phase61 medium corpus has twelve scenarios × three key settings × two styles × three presets");
    require(benchCoverageRows==benchCaseNumber*657,"Phase61 medium coverage tracks every one of 657 V4 templates in every case");
    require(bool(benchCases)&&bool(benchMatches)&&bool(benchCandidates)&&bool(benchCoverage)&&bool(benchTrace)&&bool(mediaIndex),
        "Phase61 medium benchmark outputs remain writable");

    // One-parameter diagnostic only: remove shared-template diversity penalty
    // for the fixed C-major, Pop, C-G-Am baseline cells. All other weights and
    // gates are identical; no production setting is changed.
    std::ofstream ablation(out/"phase61-diversity0-ablation.tsv"),ablationTrace(out/"phase61-diversity0-traces.txt");
    require(bool(ablation)&&bool(ablationTrace),"Phase61 diversity ablation outputs open");
    ablation<<"case\tpreset\tdurationQN\topen\tvariant\tgroup\trank\tdefaultVisible\tmoreAccessible\tcandidateId\tprimaryId\tsupportingIds\tintent\tcadence\tdiversityPenalty\trankingScore\teligibilityScore\tcompletionScore\tcompletionReason\tfullPath\n";
    ablation<<std::setprecision(9);
    std::size_t ablationCases{};
    auto appendAblationCandidates=[&](std::size_t caseId,const char* presetName,double duration,bool open,const char* variant,
        const RecommendationSet& rec,float diversityPenalty,const Progression& input,const MatchQuery& query) {
        const auto visibility=session::presentationIndices(rec,{});
        for (std::size_t group=0;group<rec.groups.size();++group) for (std::size_t rank=0;rank<rec.groups[group].size();++rank) {
            const auto& candidate=rec.groups[group][rank];
            const bool visible=std::find(visibility[group].begin(),visibility[group].end(),rank)!=visibility[group].end();
            const auto [phraseStart,current]=phase61PhraseEndpoints(query,candidate);
            const auto completion=evaluateIntentCompletion(candidate,phraseStart,current);
            ablation<<caseId<<'\t'<<presetName<<'\t'<<duration<<'\t'<<open<<'\t'<<variant<<'\t'<<group<<'\t'<<rank+1
                <<'\t'<<visible<<"\t1\t"<<candidate.id<<'\t'<<candidate.primaryTemplate<<'\t';
            for (std::size_t i=0;i<candidate.supportingTemplates.size();++i) {
                if (i) ablation<<';';
                ablation<<candidate.supportingTemplates[i];
            }
            ablation<<'\t'<<intentName(candidate.intent)<<'\t'<<cadenceName(candidate.cadence)<<'\t'<<diversityPenalty<<'\t'
                <<candidate.rankingScore<<'\t'<<candidate.eligibilityScore<<'\t'<<completion.score<<'\t'
                <<completionReasonKey(completion.reason)<<'\t'<<'"'<<phase61InputPath(input)<<" -> "
                <<phase61ChordPath(candidate.continuation)<<'"'<<'\n';
        }
    };
    plugin::RecommendationWorker ablationWorker(factoryPath,out/"phase61-ablation-isolated-user.db",
        out/"phase61-ablation-isolated-factory-store");
    for (double duration:{2.,4.}) for (bool open:{false,true}) for (const auto preset:presets) {
        ++ablationCases;
        const auto input=symbolic(phrase({"C","G","Am"},{duration,duration,duration},open));
        AnalysisContext context;context.timeSigNumerator=4;context.timeSigDenominator=4;
        context.forcedKey=KeySignature{PitchClass::C,Mode::Major};
        RecommendationRequest request;request.style=Style::Pop;
        const auto effectiveContext=plugin::developmentContext(context,preset);
        ablationWorker.submitPreset(input,effectiveContext,preset,request,false,ablationCases);
        const auto baseline=wait(ablationWorker);
        require(baseline.factoryVersion==4&&baseline.factoryCount==657&&baseline.factorySha256==phase61FactoryHash,
            "Phase61 ablation baseline uses the supplied frozen V4/657 Factory");
        auto weights=plugin::developmentRecommendationWeights(preset);
        const float originalPenalty=weights.diversityPenalty;
        const auto query=makeMatchQuery(input,effectiveContext);
        auto zeroWeights=weights;zeroWeights.diversityPenalty=0.f;
        const auto ablated=recommendContinuations(query,CandidateIndex(db.templates),request,zeroWeights);
        require(originalPenalty==13.f&&zeroWeights.minimumMatch==weights.minimumMatch&&
            zeroWeights.minimumScore==weights.minimumScore&&zeroWeights.perGroup==weights.perGroup&&
            zeroWeights.unexplainedInsertionPenalty==weights.unexplainedInsertionPenalty&&
            zeroWeights.templateDeletionPenalty==weights.templateDeletionPenalty&&
            zeroWeights.intentCompletionTieBreak==weights.intentCompletionTieBreak,
            "Phase61 diversity ablation changes only diversityPenalty from its baseline value");
        const auto caseId="A"+std::to_string(ablationCases);
        ablationTrace<<"\nCASE="<<caseId<<" preset="<<plugin::algorithmPresetName(preset)<<" fixedKey=Cmaj style=Pop durationQN="
            <<duration<<" OPEN="<<open<<" diversityPenalty="<<originalPenalty<<" vs zero; all other weights unchanged.\n"
            <<"BASELINE"<<session::recommendationDiagnostics(input,effectiveContext,request,baseline.recommendations)
            <<"\nDIVERSITY_ZERO"<<session::recommendationDiagnostics(input,effectiveContext,request,ablated);
        appendAblationCandidates(ablationCases,plugin::algorithmPresetName(preset),duration,open,"baseline",
            baseline.recommendations,originalPenalty,input,query);
        appendAblationCandidates(ablationCases,plugin::algorithmPresetName(preset),duration,open,"diversity0",
            ablated,0.f,input,query);
    }
    require(ablationCases==12,"Phase61 diversity ablation covers fixed C-G-Am Pop across 2/4QN, OPEN/closed and three presets");
    require(bool(ablation)&&bool(ablationTrace),"Phase61 diversity ablation outputs remain writable");
    std::cout<<"Phase61 baseline cases="<<caseNumber<<" targetMatchRows="<<targetMatchRows
        <<" targetRetained="<<targetRetainedRows<<" targetDefaultTop3="<<targetDefaultRows
        <<" targetPresentInFactory="<<(target!=db.templates.end())<<" mediumCases="<<benchCaseNumber
        <<" diversityAblationCases="<<ablationCases<<" output="<<out.string()<<"\n";
    return 0;
}
}
