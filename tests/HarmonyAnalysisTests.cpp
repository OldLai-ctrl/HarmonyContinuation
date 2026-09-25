#include "ProgressionJson.h"
#include "core/HarmonyAnalysis.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

using namespace harmony;
namespace {
void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }

struct Case {
    Progression source;
    HarmonicAnalysisResult result;
};
Case load(const char* name, AnalysisContext context = {}) {
    std::ifstream file(std::string(HC_HARMONY_FIXTURE_DIR) + '/' + name + ".json", std::ios::binary);
    check(static_cast<bool>(file), "fixture missing");
    const std::string json(std::istreambuf_iterator<char>{file}, {});
    auto parsed = dev::parseProgressionJson(json);
    if (!parsed) throw std::runtime_error(std::string(name) + ": " + parsed.error);
    auto result = analyzeHarmony(parsed.events, context);
    check(result.full.size() == parsed.events.size(), "analysis dropped an event");
    for (const auto& event : result.full)
        check(event.structuralWeight >= 0 && event.structuralWeight <= 1 &&
              event.analysisConfidence >= 0 && event.analysisConfidence <= 1, "weight/confidence range");
    std::cout << "PASS " << name << '\n';
    return {std::move(parsed.events), std::move(result)};
}
bool selected(const Case& value, PitchClass tonic, Mode mode) {
    return value.result.selectedKey && value.result.selectedKey->key.tonic == tonic &&
           value.result.selectedKey->key.mode == mode;
}
bool candidate(const Case& value, PitchClass tonic, Mode mode) {
    const auto& keys = value.result.keyCandidates;
    return std::any_of(keys.begin(), keys.begin() + std::min<std::size_t>(6, keys.size()), [&](const auto& key) {
        return key.key.tonic == tonic && key.key.mode == mode;
    });
}
bool inSkeleton(const Case& value, std::size_t index) {
    const auto& indices = value.result.skeletonIndices;
    return std::find(indices.begin(), indices.end(), index) != indices.end();
}
}

int main() {
    try {
        const auto major = load("major_basic");
        check(selected(major, PitchClass::C, Mode::Major), "C F G C should select C Major");
        check(major.result.full[0].degree->degree == 1 && major.result.full[1].degree->degree == 4 &&
              major.result.full[2].degree->degree == 5, "I IV V degrees");
        check(major.result.full[0].function == HarmonicFunction::Tonic &&
              major.result.full[1].function == HarmonicFunction::Predominant &&
              major.result.full[2].function == HarmonicFunction::Dominant, "T PD D functions");

        const auto naturalMinor = load("minor_natural");
        check(selected(naturalMinor, PitchClass::A, Mode::Minor) &&
              naturalMinor.result.full[1].function == HarmonicFunction::Predominant,
              "natural minor with iv is recognized");
        const auto harmonicMinor = load("minor_harmonic_dominant");
        check(selected(harmonicMinor, PitchClass::A, Mode::Minor) &&
              harmonicMinor.result.full[2].function == HarmonicFunction::Dominant &&
              !hasRole(harmonicMinor.result.full[2].roles, Role::SecondaryDominant),
              "harmonic minor V7 is a primary dominant");

        const auto ii = load("ii_v_i");
        check(selected(ii, PitchClass::C, Mode::Major), "ii V I should select C Major");
        check(ii.result.full[0].degree->degree == 2 && ii.result.full[1].degree->degree == 5 &&
              ii.result.full[2].degree->degree == 1, "ii V I degrees");

        const auto relative = load("relative_minor");
        check(candidate(relative, PitchClass::A, Mode::Minor) && candidate(relative, PitchClass::C, Mode::Major),
              "relative major and minor must both remain candidates");

        const auto secondary = load("secondary_dominant");
        check(selected(secondary, PitchClass::C, Mode::Major), "secondary dominant should not break C Major");
        check(hasRole(secondary.result.full[2].roles, Role::SecondaryDominant) &&
              secondary.result.full[2].target && secondary.result.full[2].target->degree == 2 &&
              formatDegree(secondary.result.full[2]) == "V7/ii", "A7 is V7/ii");
        check(!inSkeleton(secondary, 2), "brief A7 may be omitted from skeleton");

        const auto vv = load("v_of_v");
        check(selected(vv, PitchClass::C, Mode::Major) &&
              hasRole(vv.result.full[1].roles, Role::SecondaryDominant) &&
              vv.result.full[1].target->degree == 5 &&
              formatDegree(vv.result.full[1]) == "V7/V", "D7 is V/V");

        const auto vviAuto = load("v_of_vi");
        check(candidate(vviAuto, PitchClass::C, Mode::Major), "C Major remains a plausible V/vi reading");
        AnalysisContext cMajor;
        cMajor.forcedKey = KeySignature{PitchClass::C, Mode::Major};
        const auto vvi = load("v_of_vi", cMajor);
        check(selected(vvi, PitchClass::C, Mode::Major) &&
              hasRole(vvi.result.full[1].roles, Role::SecondaryDominant) &&
              vvi.result.full[1].target->degree == 6 &&
              formatDegree(vvi.result.full[1]) == "V7/vi", "E7 is V/vi");

        const auto leading = load("leading_tone");
        check(selected(leading, PitchClass::C, Mode::Major) &&
              hasRole(leading.result.full[1].roles, Role::SecondaryLeadingTone) &&
              leading.result.full[1].target->degree == 2 &&
              formatDegree(leading.result.full[1]) == "vii°7/ii", "C#dim7 is vii dim7/ii");

        const auto iv = load("borrowed_iv");
        check(selected(iv, PitchClass::C, Mode::Major) && hasRole(iv.result.full[2].roles, Role::Borrowed) &&
              iv.result.full[2].degree->degree == 4, "Fm is borrowed iv");
        const auto bvii = load("borrowed_bvii");
        check(selected(bvii, PitchClass::C, Mode::Major) && hasRole(bvii.result.full[1].roles, Role::Borrowed) &&
              bvii.result.full[1].degree->degree == 7 && bvii.result.full[1].degree->alteration == -1,
              "Bb is borrowed bVII");
        const auto bvi = load("borrowed_bvi");
        check(selected(bvi, PitchClass::C, Mode::Major) && hasRole(bvi.result.full[1].roles, Role::Borrowed) &&
              bvi.result.full[1].degree->degree == 6 && bvi.result.full[1].degree->alteration == -1,
              "Ab is borrowed bVI");
        const auto biii = load("borrowed_biii");
        check(selected(biii, PitchClass::C, Mode::Major) && hasRole(biii.result.full[1].roles, Role::Borrowed) &&
              biii.result.full[1].degree->degree == 3 && biii.result.full[1].degree->alteration == -1,
              "Eb is borrowed bIII");

        const auto longSecondary = load("long_secondary_dominant");
        check(hasRole(longSecondary.result.full[1].roles, Role::SecondaryDominant) &&
              longSecondary.result.full[1].structuralWeight >= 0.55f && inSkeleton(longSecondary, 1),
              "a full-bar secondary dominant can be structural");

        const auto approach = load("short_approach");
        check(hasRole(approach.result.full[1].roles, Role::Approach) &&
              approach.result.full[1].structuralWeight < approach.result.full[2].structuralWeight &&
              !inSkeleton(approach, 1), "short chromatic approach should be light and omittable");

        const auto shortDominant = load("short_dominant");
        check(shortDominant.result.full[1].structuralWeight > 0.5f && inSkeleton(shortDominant, 1),
              "short cadential V7 must remain in skeleton");

        const auto base = load("duration_base");
        const auto scaled = load("duration_scaled");
        check(base.result.selectedKey->key.tonic == scaled.result.selectedKey->key.tonic &&
              base.result.selectedKey->key.mode == scaled.result.selectedKey->key.mode,
              "duration scaling should not change the chosen key");
        for (std::size_t i = 0; i < base.result.full.size(); ++i)
            check(std::abs(base.result.full[i].structuralWeight - scaled.result.full[i].structuralWeight) < 0.01f,
                  "duration scaling should preserve relative structural weight");

        const auto open = load("last_open");
        check(open.result.full.back().structuralWeight > 0.2f && inSkeleton(open, 2),
              "OPEN ending remains analyzable and in skeleton");
        const auto weird = load("weird");
        check(weird.result.overallConfidence < major.result.overallConfidence &&
              std::any_of(weird.result.full.begin(), weird.result.full.end(), [](const auto& e) {
                  return e.function == HarmonicFunction::Unknown;
              }), "unresolved progression should express uncertainty");

        const auto cubase = load("structured_cubase");
        check(cubase.result.full[0].chord.root == PitchClass::C &&
              cubase.result.full[0].chord.quality == ChordQuality::Major7 &&
              cubase.result.full[0].chord.intervalMask == 0x891,
              "Cubase structured mask should normalize Cmaj7");
        auto conflict = cubase.source[0]; conflict.name = "Cm7";
        check(!normalizeChord(conflict).diagnostics.empty() && normalizeChord(conflict).quality == ChordQuality::Major7,
              "structured mask wins over conflicting display name with warning");
        const auto cubaseTriads = load("cubase_triads_mask");
        check(selected(cubaseTriads, PitchClass::C, Mode::Major) &&
              cubaseTriads.result.full[0].chord.quality == ChordQuality::Major &&
              cubaseTriads.result.full[1].chord.quality == ChordQuality::Major &&
              cubaseTriads.result.full[2].chord.quality == ChordQuality::Diminished &&
              cubaseTriads.result.full[3].chord.quality == ChordQuality::Minor &&
              cubaseTriads.result.full[4].chord.quality == ChordQuality::Minor &&
              cubaseTriads.result.full[0].chord.intervalMask == 0x91 &&
              cubaseTriads.result.full[2].chord.intervalMask == 0x49 &&
              cubaseTriads.result.full[0].chord.diagnostics.empty() &&
              hasRole(cubaseTriads.result.full[2].roles, Role::SecondaryLeadingTone) &&
              cubaseTriads.result.keyCandidates.front().confidence > 0.15f &&
              std::find(cubaseTriads.result.warnings.begin(), cubaseTriads.result.warnings.end(),
                        "key is ambiguous; review alternate candidates") != cubaseTriads.result.warnings.end(),
              "real Cubase compact triad masks must not erase chord identity");
        ChordEvent extended;
        extended.name = "Cmaj9";
        const auto normalizedExtension = normalizeChord(extended);
        check(normalizedExtension.quality == ChordQuality::Major7 && normalizedExtension.colorMask == (1u << 2),
              "maj9 preserves its seventh identity and ninth color");

        AnalysisContext override;
        override.forcedKey = KeySignature{PitchClass::A, Mode::Minor};
        const auto forced = load("major_basic", override);
        check(selected(forced, PitchClass::A, Mode::Minor), "forced key controls the selected analysis");

        std::cout << "All harmony golden cases passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
