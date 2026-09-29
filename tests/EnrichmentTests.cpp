#include "demo/DemoScenario.h"
#include "enrichment/ProgressionEnrichmentEngine.h"
#include "midi/StandardMidiFileWriter.h"
#include "preview/PreviewSequence.h"
#include "snapshot/EnrichmentSnapshot.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <set>
#include <stdexcept>

namespace {
using namespace harmony;
void require(bool value, const std::string& context) {
    if (!value) throw std::runtime_error(context);
}
bool technique(const enrichment::EnrichmentResult& result, enrichment::TechniqueID id) {
    for (const auto& group : result.groups) for (const auto& candidate : group)
        for (const auto item : candidate.techniques) if (item == id) return true;
    return false;
}
}
int main() {
    try {
        int cases = 0, candidates = 0, previews = 0, midiFiles = 0, snapshots = 0;
        for (const auto& file : std::filesystem::directory_iterator(HC_ENRICHMENT_CASE_DIR)) {
            if (file.path().extension() != ".json") continue;
            ++cases;
            const auto loaded = demo::loadScenario(file.path());
            require(static_cast<bool>(loaded), file.path().string() + ": " + loaded.error);
            AnalysisContext context; context.forcedKey = loaded.scenario.forcedKey;
            const auto analysis = analyzeHarmony(loaded.scenario.chords, context);
            const auto result = enrichment::enrichProgression(loaded.scenario.chords, analysis,
                                                               loaded.scenario.style);
            require(result.error.empty(), file.path().string() + ": " + result.error);
            std::set<std::string> fingerprints;
            float priorComplexity = -1;
            for (std::size_t groupIndex = 0; groupIndex < result.groups.size(); ++groupIndex) {
                for (const auto& candidate : result.groups[groupIndex]) {
                    ++candidates;
                    require(!candidate.operations.empty() && candidate.operations.size() <=
                            enrichment::EnrichmentConfig{}.maxOperations[groupIndex], "edit budget");
                    require(candidate.skeletonPreservation >= 0.75f, "skeleton preservation");
                    require(candidate.complexityScore + 0.0001f >= priorComplexity, "complexity monotonicity");
                    priorComplexity = candidate.complexityScore;
                    require(fingerprints.insert(candidate.fingerprint).second, "duplicate candidate");
                    for (std::size_t i = 0; i < candidate.progression.size(); ++i) {
                        const auto& event = candidate.progression[i];
                        require(std::isfinite(event.startQN) && (!i || event.startQN >
                            candidate.progression[i - 1].startQN), "event order");
                        require(normalizeChord(event).quality != ChordQuality::Unknown, "unknown chord");
                    }
                    ImportedProgressionSession imported;
                    require(imported.replace(candidate.progression, TimelineCoordinateMode::AbsoluteProjectQN),
                            "import enriched progression");
                    const auto preview = preview::buildSequence(imported, nullptr, loaded.scenario.tempo);
                    require(static_cast<bool>(preview), "enriched preview: " + preview.error);
                    for (const auto& operation : candidate.operations) {
                        if (operation.technique != enrichment::TechniqueID::NinthColor) continue;
                        const double start = loaded.scenario.chords[operation.sourceIndex].startQN;
                        const auto note = std::find_if(preview.sequence.events.begin(), preview.sequence.events.end(),
                            [&](const auto& event) { return std::abs(event.startQN - start) < 1e-7; });
                        require(note != preview.sequence.events.end() && (note->chord.intervals & (1u << 2)),
                                "ninth must survive preview/MIDI conversion");
                    }
                    ++previews;
                    const auto clip = midi::buildClip(preview.sequence, midi::ArrangementMode::VoiceLed,
                                                      midi::ExportScope::FullPhrase,
                                                      {loaded.scenario.meterNumerator, loaded.scenario.meterDenominator},
                                                      loaded.scenario.forcedKey);
                    require(static_cast<bool>(clip), "enriched MIDI: " + clip.error);
                    const auto bytes = midi::writeToMemory(clip.sequence);
                    require(static_cast<bool>(bytes) && bytes.bytes.size() > 50, "enriched MIDI bytes");
                    ++midiFiles;
                    ImportedProgressionSession original;
                    require(original.replace(loaded.scenario.chords,TimelineCoordinateMode::AbsoluteProjectQN),
                            "original snapshot input");
                    const auto captured=snapshot::captureEnrichment(original,candidate,loaded.scenario.tempo,
                        loaded.scenario.meterNumerator,loaded.scenario.meterDenominator,
                        loaded.scenario.forcedKey,loaded.scenario.style);
                    const auto restored=snapshot::deserializeEnrichment(snapshot::serialize(captured));
                    require(restored && restored.value.candidate.fingerprint==candidate.fingerprint &&
                            restored.value.candidate.operations.size()==candidate.operations.size(),
                            "enrichment snapshot roundtrip");
                    ++snapshots;
                }
            }
            if (file.path().stem().string().find("passing_dim") != std::string::npos)
                require(technique(result, enrichment::TechniqueID::PassingDiminished), "passing diminished");
            if (file.path().stem().string().find("secondary_target") != std::string::npos)
                require(technique(result, enrichment::TechniqueID::SecondaryDominant), "secondary dominant");
        }
        require(cases >= 24, "benchmark case count");
        require(candidates >= 20, "candidate coverage");
        std::cout << "enrichment cases=" << cases << " candidates=" << candidates
                  << " preview=" << previews << " MIDI=" << midiFiles << " snapshot=" << snapshots << '\n';
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "EnrichmentTests FAIL: " << e.what() << '\n'; return 1;
    }
}
