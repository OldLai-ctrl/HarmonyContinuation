#include "wish/WishWorkflow.h"

#include "color/HarmonyColorAnalyzer.h"
#include "preview/PreviewSequence.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

using namespace harmony;
using namespace harmony::wish;
using namespace std::chrono_literals;

void require(bool condition, const char* message, int& checks) {
    if (!condition) throw std::runtime_error(message);
    ++checks;
}

template<class Predicate>
std::shared_ptr<const WishView> waitFor(WishWorkflow& workflow, Predicate predicate,
                                        const char* timeoutMessage) {
    const auto deadline = std::chrono::steady_clock::now() + 10s;
    for (;;) {
        auto current = workflow.view();
        if (predicate(*current)) return current;
        if (std::chrono::steady_clock::now() >= deadline)
            throw std::runtime_error(timeoutMessage);
        std::this_thread::sleep_for(2ms);
    }
}

const ProgressionTemplate& sourceById(
    const std::vector<ProgressionTemplate>& sources, const std::string& id) {
    const auto found = std::find_if(sources.begin(), sources.end(),
        [&](const auto& source) { return source.id == id; });
    if (found == sources.end()) throw std::runtime_error("required real Factory source is missing");
    return *found;
}

std::vector<ProgressionTemplate> shortSources(
    const std::vector<ProgressionTemplate>& sources) {
    return {sourceById(sources, "COMMON_MINOR_007"),
            sourceById(sources, "COMMON_MINOR_001"),
            sourceById(sources, "V4_A08"),
            sourceById(sources, "COMMON_MAJOR_020")};
}

int pc(int value) { return (value % 12 + 12) % 12; }

bool colorFlag(const color::PathColor& path, ColorDirection direction) {
    switch (direction) {
        case ColorDirection::Warming: return path.warming;
        case ColorDirection::Cooling: return path.cooling;
        case ColorDirection::TensionRising: return path.tensionRising;
        case ColorDirection::TensionReleasing: return path.tensionReleasing;
        case ColorDirection::Any: return true;
    }
    return false;
}

struct ColorFixture {
    ProgressionTemplate source;
    int tonic{};
    ColorDirection direction{ColorDirection::Any};
};

} // namespace

int runWishWorkflowChecks(const std::vector<harmony::ProgressionTemplate>& sources,
                          const harmony::wish::AnnotationCatalog& annotations) {
    using namespace harmony;
    using namespace harmony::wish;

    int checks{};
    const auto batchSources = shortSources(sources);
    WishSettings tonicC;
    tonicC.filter.fixedTonic = PitchClass::C;

    // Free drawing must ignore even a malformed input value passed by its caller.
    WishInput badInput;
    badInput.imported.coordinateMode = TimelineCoordinateMode::Unknown;
    badInput.imported.events.resize(65);
    WishWorkflow freeWorkflow(batchSources, annotations);
    freeWorkflow.reset(tonicC, badInput, 101);
    require(!freeWorkflow.view()->usesInput && freeWorkflow.view()->error.empty(),
            "free workflow ignores an invalid supplied input", checks);
    require(freeWorkflow.draw(1), "free workflow accepts a first draw", checks);
    const auto freeDone = waitFor(freeWorkflow, [](const auto& view) { return !view.busy; },
                                  "free workflow draw timed out");
    require(freeDone->error.empty() && freeDone->items.size() == 1,
            "free workflow draws from its own Factory pool", checks);

    // One round is retained across 1 + 10 + 10 and old published snapshots never mutate.
    WishWorkflow roundWorkflow(batchSources, annotations);
    roundWorkflow.reset(tonicC, {}, 20261023);
    require(roundWorkflow.draw(1), "round accepts draw(1)", checks);
    const auto first = waitFor(roundWorkflow, [](const auto& view) { return !view.busy; },
                               "first round draw timed out");
    require(first->items.size() == 1 && first->poolSize >= 2,
            "real short fixture has multiple fixed-tonic candidates", checks);
    const auto firstIdentity = first->items.front()->musicIdentity;
    require(roundWorkflow.draw(10), "round accepts draw(10)", checks);
    const auto second = waitFor(roundWorkflow, [](const auto& view) { return !view.busy; },
                                "second round draw timed out");
    require(second->lastDraw > 0 && second->lastDraw < 10,
            "draw(10) reports the actual short remainder", checks);
    require(first->items.size() == 1 && first->items.front()->musicIdentity == firstIdentity,
            "an earlier immutable view remains unchanged after append", checks);
    require(roundWorkflow.draw(10), "exhausted round accepts a final draw request", checks);
    const auto third = waitFor(roundWorkflow, [](const auto& view) { return !view.busy; },
                               "third round draw timed out");
    require(third->lastDraw == 0 && third->remaining == 0 &&
            third->diagnostic.find("exhausted=1") != std::string::npos,
            "exhausted pool is reported without refill", checks);
    require(third->items.size() == third->poolSize, "all available candidates were retained", checks);
    std::set<std::string> identities;
    for (const auto& item : third->items)
        require(identities.insert(item->musicIdentity).second,
                "one no-replacement round never repeats a music identity", checks);
    require(first->items.size() == 1 && first->items.front()->musicIdentity == firstIdentity,
            "the original view stays immutable after later draws", checks);

    // Equal seeds and parameters produce the same order from the same real source subset.
    const auto drawSeeded = [&](std::uint64_t seed) {
        WishWorkflow workflow(batchSources, annotations);
        workflow.reset(tonicC, {}, seed);
        require(workflow.draw(10), "seeded workflow accepts draw(10)", checks);
        const auto done = waitFor(workflow, [](const auto& view) { return !view.busy; },
                                  "seeded workflow draw timed out");
        require(done->error.empty(), "seeded workflow has no error", checks);
        std::vector<std::string> result;
        for (const auto& item : done->items) result.push_back(item->musicIdentity);
        return result;
    };
    require(drawSeeded(9090) == drawSeeded(9090),
            "fixed seed reproduces the same draw order", checks);

    // Reviewed memberships map the selected absolute tonic through each real offset.
    const auto& multiModeSource = sourceById(sources, "COMMON_MINOR_007");
    WishWorkflow modeWorkflow({multiModeSource}, annotations);
    WishSettings modes;
    modes.filter.fixedTonic = PitchClass::C;
    modes.filter.modes = {MusicalMode::Aeolian, MusicalMode::Ionian};
    modeWorkflow.reset(modes, {}, 303);
    require(modeWorkflow.draw(10), "multi-mode workflow accepts a draw", checks);
    const auto modeDone = waitFor(modeWorkflow, [](const auto& view) { return !view.busy; },
                                  "multi-mode workflow draw timed out");
    require(modeDone->error.empty() && modeDone->poolSize >= 2,
            "reviewed multi-mode source produces tonic-mapped routes", checks);
    std::set<MusicalMode> realizedModes;
    for (const auto& item : modeDone->items) {
        require(!item->match, "free routes do not fabricate a match object", checks);
        require(!item->memberships.empty(), "mode-filtered route carries reviewed membership", checks);
        for (const auto& membership : item->memberships) {
            require(membership.tonicPitchClass == int(PitchClass::C) &&
                    (membership.membership.mode == MusicalMode::Aeolian ||
                     membership.membership.mode == MusicalMode::Ionian),
                    "membership tonic and selected modes agree", checks);
            require(pc(int(item->route.candidate.key.tonic) + membership.membership.tonicOffset) ==
                    int(PitchClass::C), "route key plus annotation offset maps to fixed tonic", checks);
            realizedModes.insert(membership.membership.mode);
        }
    }
    require(realizedModes.contains(MusicalMode::Aeolian) &&
            realizedModes.contains(MusicalMode::Ionian),
            "both reviewed modes remain selectable together", checks);

    // A style choice is resolved from source metadata, including exclusion of nonmatching sources.
    constexpr std::array styleOptions{
        Style::Pop, Style::Rock, Style::Rnb, Style::Jazz, Style::CityPop, Style::Functional};
    const ProgressionTemplate* styledSource{};
    const ProgressionTemplate* nonstyledSource{};
    Style selectedStyle{Style::Pop};
    for (const auto style : styleOptions) {
        const auto matching = std::find_if(sources.begin(), sources.end(), [&](const auto& source) {
            return (source.styles & static_cast<StyleFlags>(style)) != 0;
        });
        const auto nonmatching = std::find_if(sources.begin(), sources.end(), [&](const auto& source) {
            return (source.styles & static_cast<StyleFlags>(style)) == 0;
        });
        if (matching != sources.end() && nonmatching != sources.end()) {
            styledSource = &*matching;
            nonstyledSource = &*nonmatching;
            selectedStyle = style;
            break;
        }
    }
    require(styledSource != nullptr && nonstyledSource != nullptr,
            "real Factory metadata supports a matching and nonmatching style pair", checks);
    std::vector<ProgressionTemplate> styleSources{*styledSource, *nonstyledSource};
    WishWorkflow styleWorkflow(styleSources, annotations);
    WishSettings styled;
    styled.filter.fixedTonic = PitchClass::C;
    styled.style = selectedStyle;
    styleWorkflow.reset(styled, {}, 404);
    require(styleWorkflow.draw(10), "style-filtered workflow accepts a draw", checks);
    const auto styleDone = waitFor(styleWorkflow, [](const auto& view) { return !view.busy; },
                                   "style-filtered workflow draw timed out");
    require(styleDone->error.empty() && styleDone->poolSize > 0,
            "style metadata leaves its matching source available", checks);
    for (const auto& item : styleDone->items)
        for (const auto& sourceRef : item->sources)
            require(sourceRef.id == styledSource->id &&
                    (styledSource->styles & static_cast<StyleFlags>(selectedStyle)) != 0,
                    "every style-filtered route retains matching source metadata", checks);

    // Use the existing analyzer to locate reviewed-data-independent, Known color flags.
    std::array<std::optional<ColorFixture>, 4> colorFixtures;
    constexpr std::array colorDirections{
        ColorDirection::Warming, ColorDirection::Cooling,
        ColorDirection::TensionRising, ColorDirection::TensionReleasing};
    const auto scanColorSources = [&](std::size_t begin, std::size_t end) {
        for (std::size_t i = begin; i < end; ++i) {
            for (int tonic = 0; tonic < 12; ++tonic) {
                const auto route = preview::templateRoute(sources[i],
                    {static_cast<PitchClass>(tonic), sources[i].mode});
                if (!route) continue;
                const auto path = color::HarmonyColorAnalyzer::analyzeContinuation(
                    color::HarmonyColorAnalyzer::prepare(route.imported.events), route.candidate);
                if (path.status != color::Status::Known) continue;
                for (std::size_t d = 0; d < colorDirections.size(); ++d)
                    if (!colorFixtures[d] && colorFlag(path, colorDirections[d]))
                        colorFixtures[d] = ColorFixture{sources[i], tonic, colorDirections[d]};
            }
            if (std::all_of(colorFixtures.begin(), colorFixtures.end(),
                            [](const auto& fixture) { return fixture.has_value(); })) break;
        }
    };
    const auto initialColorScan = std::min<std::size_t>(64, sources.size());
    scanColorSources(0, initialColorScan);
    const bool foundKnownColor = std::any_of(colorFixtures.begin(), colorFixtures.end(),
        [](const auto& fixture) { return fixture.has_value(); });
    if (!foundKnownColor) scanColorSources(initialColorScan, sources.size());
    std::size_t checkedColorDirections{};
    for (const auto& fixture : colorFixtures) {
        if (!fixture) continue;
        WishWorkflow colorWorkflow({fixture->source}, annotations);
        WishSettings colorSettings;
        colorSettings.filter.fixedTonic = static_cast<PitchClass>(fixture->tonic);
        colorSettings.color = fixture->direction;
        colorWorkflow.reset(colorSettings, {}, 505);
        require(colorWorkflow.draw(1), "Known-color workflow accepts a draw", checks);
        const auto colorDone = waitFor(colorWorkflow, [](const auto& view) { return !view.busy; },
                                       "Known-color workflow draw timed out");
        require(colorDone->error.empty() && colorDone->poolSize == 1 &&
                colorDone->items.size() == 1,
                "Known color flag admits its real source route", checks);
        const auto& item = *colorDone->items.front();
        const auto path = color::HarmonyColorAnalyzer::analyzeContinuation(
            color::HarmonyColorAnalyzer::prepare(item.route.imported.events), item.route.candidate);
        require(path.status == color::Status::Known && colorFlag(path, fixture->direction),
                "color filtering uses the analyzer's Known flag", checks);
        ++checkedColorDirections;
    }
    require(checkedColorDirections > 0,
            "a Known color direction exists in the inspected real Factory subset", checks);

    // Empty and unreviewed mode pools stay empty; they are not assigned guessed memberships.
    WishWorkflow noInputWorkflow(batchSources, annotations);
    WishSettings inputMode;
    inputMode.useInput = true;
    noInputWorkflow.reset(inputMode, {}, 606);
    const auto noInput = noInputWorkflow.view();
    require(noInput->usesInput && !noInput->error.empty() && !noInputWorkflow.draw(1),
            "input mode with no current progression is an explicit error", checks);

    WishWorkflow locrianWorkflow(batchSources, annotations);
    WishSettings locrian;
    locrian.filter.modes = {MusicalMode::Locrian};
    locrian.filter.strict = true;
    locrianWorkflow.reset(locrian, {}, 707);
    require(locrianWorkflow.draw(1), "Locrian query is accepted as a valid empty-pool query", checks);
    const auto locrianDone = waitFor(locrianWorkflow, [](const auto& view) { return !view.busy; },
                                     "Locrian workflow draw timed out");
    require(locrianDone->error.empty() && locrianDone->poolSize == 0 &&
            locrianDone->items.empty() && locrianDone->lastDraw == 0,
            "unreviewed Locrian data yields zero candidates without invented attribution", checks);

    // Reset during a pending worker revision clears its result and the retained object can draw again.
    const auto asyncCount = std::min<std::size_t>(64, sources.size());
    std::vector<ProgressionTemplate> asyncSources(sources.begin(), sources.begin() + asyncCount);
    WishWorkflow asyncWorkflow(std::move(asyncSources), annotations);
    const auto oldRevision = asyncWorkflow.reset(WishSettings{}, {}, 808);
    require(asyncWorkflow.draw(10), "async invalidation starts a draw", checks);
    const auto pending = asyncWorkflow.view();
    require(pending->revision == oldRevision && pending->busy,
            "draw publishes a busy snapshot before worker completion", checks);
    const auto newRevision = asyncWorkflow.reset(tonicC, {}, 809);
    require(newRevision > oldRevision, "reset increments the result revision", checks);
    const auto resetDone = waitFor(asyncWorkflow, [](const auto& view) { return !view.busy; },
                                   "stale worker did not settle after reset");
    require(resetDone->revision == newRevision && resetDone->items.empty() &&
            resetDone->poolSize == 0 && resetDone->error.empty(),
            "old-revision draw cannot publish over reset state", checks);
    require(asyncWorkflow.draw(1), "new revision accepts a draw after stale work settles", checks);
    const auto newDone = waitFor(asyncWorkflow, [](const auto& view) { return !view.busy; },
                                 "new-revision draw timed out");
    require(newDone->revision == newRevision && newDone->items.size() == 1,
            "post-reset draw publishes only current-revision results", checks);

    // Simulate closing and reopening an editor while EditorUiState retains one workspace workflow.
    auto workspace = std::make_shared<WishWorkflow>(batchSources, annotations);
    workspace->reset(tonicC, {}, 910);
    auto editor = workspace;
    require(editor->draw(1), "first editor session draws from shared workspace", checks);
    const auto editorFirst = waitFor(*editor, [](const auto& view) { return !view.busy; },
                                     "first editor-session draw timed out");
    require(editorFirst->items.size() == 1, "first editor session receives one result", checks);
    const auto editorFirstIdentity = editorFirst->items.front()->musicIdentity;
    editor.reset();
    editor = workspace;
    require(editor->draw(1), "reopened editor continues the retained round", checks);
    const auto editorSecond = waitFor(*editor, [](const auto& view) { return !view.busy; },
                                      "reopened editor draw timed out");
    require(editorSecond->items.size() >= 2 &&
            editorSecond->items.front()->musicIdentity == editorFirstIdentity,
            "reopened editor sees the same accumulated immutable result list", checks);
    require(editorSecond->items[1]->musicIdentity != editorFirstIdentity,
            "reopening the editor does not repeat the workspace candidate", checks);

    return checks;
}
