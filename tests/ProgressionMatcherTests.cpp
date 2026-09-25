#include "TemplateJson.h"
#include "ProgressionJson.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>

using namespace harmony;
namespace {
int checks{}, failures{};
void expect(bool condition, const std::string& label) {
    ++checks;
    if (!condition) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
std::vector<MatchEvent> notation(std::string_view sequence, Mode mode = Mode::Major,
                                  std::string_view rhythm = {}) {
    std::istringstream input{std::string(sequence)};
    std::vector<std::string> tokens;
    for (std::string token; input >> token;) tokens.push_back(token);
    std::istringstream durations{std::string(rhythm)};
    std::vector<MatchEvent> events;
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        double value = 4.0;
        if (!rhythm.empty() && !(durations >> value)) throw std::runtime_error("bad test rhythm");
        auto event = dev::parseNotationEvent(tokens[i], mode, value);
        event.sourceIndex = i;
        events.push_back(event);
    }
    return events;
}
MatchQuery query(std::string_view sequence, Mode mode = Mode::Major, std::string_view rhythm = {}) {
    MatchQuery result;
    KeyInterpretation interpretation;
    interpretation.key.key.mode = mode;
    interpretation.key.key.tonic = mode == Mode::Major ? PitchClass::C : PitchClass::A;
    interpretation.key.confidence = 0.4f;
    interpretation.full = notation(sequence, mode, rhythm);
    for (std::size_t i = 0; i < interpretation.full.size(); ++i)
        if (i == 0 || i + 1 == interpretation.full.size() || interpretation.full[i].structuralWeight >= 0.55f)
            interpretation.skeletonIndices.push_back(i);
    interpretation.fingerprint = makeFingerprint(mode, interpretation.full, interpretation.skeletonIndices, CadenceType::None);
    result.interpretations.push_back(std::move(interpretation));
    return result;
}
ProgressionTemplate templateById(const std::vector<ProgressionTemplate>& all, std::string_view id) {
    const auto found = std::find_if(all.begin(), all.end(), [id](const auto& t) { return t.id == id; });
    if (found == all.end()) throw std::runtime_error("missing fixture template");
    return *found;
}
MatchResult single(const MatchQuery& q, ProgressionTemplate t) {
    const CandidateIndex index({std::move(t)});
    const auto results = matchProgression(q, index, {}, 1);
    if (results.empty()) throw std::runtime_error("no match result");
    return results.front();
}
bool hasOp(const MatchResult& result, AlignmentOp op) {
    return std::any_of(result.alignmentTrace.begin(), result.alignmentTrace.end(),
                       [op](const auto& step) { return step.operation == op; });
}
bool hasFlag(const MatchResult& result, MatchReason reason) {
    return std::any_of(result.alignmentTrace.begin(), result.alignmentTrace.end(),
                       [reason](const auto& step) { return hasReason(step.reasons, reason); });
}
void show(std::string_view label, const MatchResult& r) {
    std::cout << label << " " << r.templateId << " score=" << r.similarity << " skeleton="
              << r.subScores.skeletonHarmony << " full=" << r.subScores.fullHarmony
              << " rhythm=" << r.subScores.rhythmSimilarity << " start=" << r.templateMatchStart
              << " end=" << r.templateMatchEnd << " next=" << r.continuationStartIndex << '\n';
}
}
int main() {
    try {
        const auto parsed = dev::loadDevelopmentTemplates();
        expect(bool(parsed), "fixture parses: " + parsed.error);
        if (!parsed) return 1;
        const auto& all = parsed.templates;
        expect(all.size() >= 40 && all.size() <= 80, "fixture library has 40..80 templates");
        const auto m05 = templateById(all, "M05"), m01 = templateById(all, "M01");
        const auto a = single(query("I vi ii V"), m05);
        show("A", a);
        expect(a.similarity > 0.82f, "A exact prefix high");
        expect(a.templateMatchEnd == 3 && a.continuationStartIndex == 4 && a.hasContinuation, "A suffix free, next I");
        const auto b = single(query("I vi V7/ii ii V", Mode::Major, "4 4 1 3 4"), m05);
        show("B", b);
        expect(b.similarity > 0.72f && hasOp(b, AlignmentOp::QueryInsertion), "B secondary insertion remains high");
        expect(hasFlag(b, MatchReason::ResolvesToMatchedTarget), "B trace explains secondary target");
        const auto secondary = dev::parseNotationEvent("V7/ii", Mode::Major, 1.0);
        expect(secondary.degree && secondary.degree->degree == 6 && secondary.target && secondary.target->degree == 2,
               "B V/ii stores actual VI root and ii target");
        std::ifstream secondaryFile(HC_HARMONY_FIXTURE_DIR "/../progressions/queries/B_secondary_dominant.json", std::ios::binary);
        const std::string secondaryText(std::istreambuf_iterator<char>{secondaryFile}, {});
        const auto secondaryProgression = dev::parseProgressionJson(secondaryText);
        expect(bool(secondaryProgression), "B Cubase-style query parses");
        if (secondaryProgression) {
            AnalysisContext forcedC; forcedC.forcedKey = KeySignature{PitchClass::C, Mode::Major};
            const auto bActual = single(makeMatchQuery(secondaryProgression.events, forcedC), templateById(all, "M17"));
            expect(bActual.alignmentTrace.size() == 5 &&
                   bActual.alignmentTrace[2].operation == AlignmentOp::Match &&
                   hasReason(bActual.alignmentTrace[2].reasons, MatchReason::SecondaryTargetMatch),
                   "B actual A7 aligns directly with V7/ii template event");
        }
        const auto c = single(query("I ii V"), m05);
        show("C", c);
        expect(c.similarity > 0.65f && hasOp(c, AlignmentOp::TemplateDeletion), "C missing vi tolerated");
        const auto d = single(query("I vi ii I"), m05);
        show("D", d);
        expect(c.similarity > d.similarity + 0.04f, "D missing dominant costlier than vi");
        const auto e = single(query("I vi ii7 V7"), templateById(all, "M20"));
        show("E", e);
        expect(e.similarity > 0.75f && hasFlag(e, MatchReason::QualityVariant), "E quality variant");
        const auto f = single(query("I iv V I"), m01);
        show("F", f);
        expect(f.similarity > 0.68f && hasFlag(f, MatchReason::BorrowedVariant), "F borrowed iv");
        const auto exact = single(query("I IV V I"), m01);
        const auto g = single(query("I ii V I"), m01);
        show("G", g);
        expect(g.similarity > 0.60f && exact.similarity > g.similarity + 0.04f, "G predominant substitution");
        const auto h = single(query("bII #iv bVI"), m05);
        show("H", h);
        expect(h.similarity < 0.65f, "H unrelated low");
        const auto i = single(query("I #Idim7 ii V I", Mode::Major, "4 0.5 3.5 4 4"), templateById(all,"M08"));
        show("I", i);
        expect(i.similarity > 0.75f && hasFlag(i, MatchReason::EmbellishingInsertion), "I diminished passing insertion");
        std::ifstream diminishedFile(HC_HARMONY_FIXTURE_DIR "/../progressions/queries/D_diminished_passing.json", std::ios::binary);
        const std::string diminishedText(std::istreambuf_iterator<char>{diminishedFile}, {});
        const auto diminishedProgression = dev::parseProgressionJson(diminishedText);
        expect(bool(diminishedProgression), "I diminished real query parses");
        if (diminishedProgression) {
            AnalysisContext forcedC; forcedC.forcedKey = KeySignature{PitchClass::C, Mode::Major};
            const auto realQuery = makeMatchQuery(diminishedProgression.events, forcedC);
            const auto fixture = templateById(all, "M19");
            const auto exactDiminished = single(realQuery, fixture);
            expect(exactDiminished.alignmentTrace.size() == 4 &&
                   exactDiminished.alignmentTrace[1].operation == AlignmentOp::Substitute &&
                   hasReason(exactDiminished.alignmentTrace[1].reasons, MatchReason::EnharmonicDegree),
                   "I real C#dim7 aligns directly with enharmonic diminished template");
        }
        auto jTemplate = m01;
        for (std::size_t k = 0; k < jTemplate.full.size(); ++k) jTemplate.full[k].durationQN = (k == 0 || k == 3) ? 4.0 : 2.0;
        const auto j = single(query("I IV V I", Mode::Major, "8 4 4 8"), jTemplate);
        show("J", j);
        expect(j.subScores.rhythmSimilarity > 0.98f, "J global rhythm scaling");
        for (std::size_t k = 0; k < jTemplate.full.size(); ++k) jTemplate.full[k].durationQN = (k == 0 || k == 3) ? 8.0 : 1.0;
        const auto k = single(query("I IV V I", Mode::Major, "2 2 2 2"), jTemplate);
        show("K", k);
        expect(k.subScores.rhythmSimilarity < 0.70f && k.subScores.fullHarmony > 0.55f, "K rhythm mismatch distinct from harmony");
        auto lQuery = query("I IV V I");
        lQuery.interpretations[0].full.back().durationQN.reset();
        const auto l = single(lQuery, m01);
        show("L", l);
        expect(l.subScores.rhythmSimilarity > 0.98f, "L OPEN last chord skips rhythm mismatch");
        auto lUnevenTemplate = m01;
        lUnevenTemplate.full[0].durationQN = 8.0;
        lUnevenTemplate.full[1].durationQN = 4.0;
        lUnevenTemplate.full[2].durationQN = 2.0;
        lUnevenTemplate.full[3].durationQN = 2.0;
        auto lUnevenQuery = query("I IV V I", Mode::Major, "4 2 1 1");
        lUnevenQuery.interpretations[0].full.back().durationQN.reset();
        const auto lUneven = single(lUnevenQuery, lUnevenTemplate);
        show("L-uneven", lUneven);
        expect(lUneven.subScores.rhythmSimilarity > 0.95f, "L uneven OPEN retains scaled rhythm");
        const auto m = single(query("vi ii V"), templateById(all,"M06"));
        show("M", m);
        expect(m.templateMatchStart == 2 && m.templateMatchEnd == 4 && m.continuationStartIndex == 5, "M mid-template boundaries");
        const auto nA = single(query("I vi ii V"), m05);
        const auto nB = single(query("I vi ii V"), templateById(all,"M23"));
        show("N-A", nA); show("N-B", nB);
        expect(nA.similarity > nB.similarity + 0.10f, "N query tail distinguishes templates");
        const auto oStructural = single(query("I bVI ii V I", Mode::Major, "4 4 4 4 4"), templateById(all,"M08"));
        const auto oPassing = single(query("I #Idim7 ii V I", Mode::Major, "4 0.5 3.5 4 4"), templateById(all,"M08"));
        show("O-structural", oStructural); show("O-passing", oPassing);
        expect(oPassing.similarity > oStructural.similarity + 0.05f, "O structural insertion more costly");
        std::ifstream file(HC_HARMONY_FIXTURE_DIR "/relative_minor.json", std::ios::binary);
        const std::string source(std::istreambuf_iterator<char>{file}, {});
        const auto progression = dev::parseProgressionJson(source);
        expect(bool(progression), "P actual ambiguous progression parses");
        if (progression) {
            const auto p = makeMatchQuery(progression.events);
            expect(p.interpretations.size() == 3, "P top 3 key interpretations evaluated");
            expect(std::any_of(p.interpretations.begin(), p.interpretations.end(), [](const auto& e) { return e.key.key.mode == Mode::Major; }) &&
                   std::any_of(p.interpretations.begin(), p.interpretations.end(), [](const auto& e) { return e.key.key.mode == Mode::Minor; }),
                   "P major and relative minor present");
            AnalysisContext forced; forced.forcedKey = KeySignature{PitchClass::C, Mode::Major};
            const auto q = makeMatchQuery(progression.events, forced);
            expect(q.interpretations.size() == 1 && q.interpretations[0].key.key.mode == Mode::Major && q.forcedKey,
                   "Q forced key only");
            const CandidateIndex index(all);
            const auto results = matchProgression(p, index);
            expect(!results.empty(), "P multi-key matcher has results");
            expect(!results.empty() && results.front().key.mode == Mode::Major &&
                   p.interpretations.front().key.key.mode == Mode::Minor,
                   "P stronger alignment can beat higher minor-key prior");
            const auto forcedResults = matchProgression(q, index);
            expect(std::all_of(forcedResults.begin(), forcedResults.end(), [](const auto& r) { return r.key.mode == Mode::Major; }),
                   "Q forced results only major");
        }
        const auto r = single(query("I vi bIII V"), m05);
        show("R", r);
        expect(r.similarity > 0.55f && r.similarity < a.similarity, "R one wrong chord remains a match with reduced score");
        expect(hasOp(r, AlignmentOp::Substitute), "R wrong chord represented as substitution");
        expect(a.similarity >= 0.f && a.similarity <= 1.f && h.similarity >= 0.f && h.similarity <= 1.f,
               "normalized similarity bounds");
        expect(a.alignmentTrace.size() == 4, "full trace retained");
        expect(a.continuationLength == 1 && a.templateLabels.size() == 5, "continuation position and labels retained");
        std::vector<ProgressionTemplate> many;
        many.reserve(10000);
        for (std::size_t index = 0; index < 9999; ++index) {
            auto unrelated = templateById(all, "M31");
            unrelated.id = "synthetic-" + std::to_string(index);
            many.push_back(std::move(unrelated));
        }
        auto buried = m05;
        buried.id = "buried-exact";
        many.push_back(std::move(buried));
        const CandidateIndex largeIndex(std::move(many));
        const auto shortlist = largeIndex.shortlist(query("I vi ii V"));
        expect(shortlist.size() == 800 && std::find(shortlist.begin(), shortlist.end(), 9999) != shortlist.end(),
               "10k prefilter retains buried exact template");
        expect(!dev::parseTemplateJson("[{\"id\":\"x\",\"mode\":\"major\",\"sequence\":\"bad\"}]").error.empty(),
               "malformed template rejected");
        expect(!dev::parseTemplateJson("[{\"id\":\"x\",\"mode\":\"major\",\"sequence\":\"I\"},{\"id\":\"x\",\"mode\":\"major\",\"sequence\":\"V\"}]").error.empty(),
               "duplicate template id rejected");
        std::cout << "ProgressionMatcherTests " << checks - failures << "/" << checks << " PASS\n";
        return failures ? 1 : 0;
    } catch (const std::exception& error) { std::cerr << "EXCEPTION " << error.what() << '\n'; return 1; }
}
