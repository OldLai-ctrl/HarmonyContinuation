#include "core/ContinuationEngine.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
using namespace harmony;
ConcreteChordEvent chord(int degree, RoleFlags roles = 0, int alteration = 0) {
    ConcreteChordEvent result;
    result.degree = {degree, alteration};
    result.roles = roles;
    result.quality = ChordQuality::Major;
    result.durationQN = 4;
    return result;
}
ConcreteChordEvent qualityChord(ChordQuality quality) {
    auto result=chord(1);
    result.quality=quality;
    return result;
}
void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
} // namespace
int main() {
    try {
        ContinuationCandidate c;
        c.intent=PhraseIntent::Resolve;
        c.cadence=CadenceType::Authentic;
        c.continuation={chord(1)};
        c.actualPhraseReturn=true;
        auto result=evaluateIntentCompletion(c,ScaleDegree{2,0},ScaleDegree{5,0});
        check(result.score>0.9f && result.reason==CompletionReason::DominantTonic,
              "single tonic completes dominant resolution");
        result=evaluateIntentCompletion(c,ScaleDegree{2,0},ScaleDegree{4,0});
        check(result.reason==CompletionReason::StableTonic,
              "Authentic cadence metadata alone does not claim an observed dominant before the final tonic");
        c.continuation={chord(5),chord(1)};
        result=evaluateIntentCompletion(c,ScaleDegree{2,0},ScaleDegree{4,0});
        check(result.reason==CompletionReason::DominantTonic,
              "an actual V-to-I continuation confirms dominant resolution");
        c.continuation={chord(6)};
        result=evaluateIntentCompletion(c);
        check(result.score<0.5f&&result.reason==CompletionReason::NoEvidence,
              "non-tonic does not finish resolution");

        c.intent=PhraseIntent::Loop;
        c.continuation={chord(1)};
        check(evaluateIntentCompletion(c,ScaleDegree{1,0}).score>0.9f,
              "single chord can close loop");
        c.continuation={chord(4)};
        c.actualPhraseReturn=false;
        c.cadence=CadenceType::LoopClosure;
        check(evaluateIntentCompletion(c).score<0.5f,"loop metadata alone is insufficient");
        c.preservedLoop=true;
        check(evaluateIntentCompletion(c).score>0.8f,"adapted loop context certifies closure");

        c.intent=PhraseIntent::Develop;
        c.cadence=CadenceType::None;
        c.continuation={chord(4)};
        check(evaluateIntentCompletion(c).score<0.5f,"plain single chord is a short development");
        c.continuation={chord(4),chord(5)};
        check(evaluateIntentCompletion(c).score>0.7f,"distinct path develops");
        c.continuation={chord(5,flag(Role::SecondaryDominant))};
        check(evaluateIntentCompletion(c).score>0.7f,"single functional pivot is allowed");

        c.intent=PhraseIntent::Color;
        c.continuation={chord(4,flag(Role::Borrowed))};
        result=evaluateIntentCompletion(c);
        check(result.score>0.8f&&result.reason==CompletionReason::BorrowedColor,
              "single borrowed chord has a borrowed-color reason");
        c.continuation={chord(5,flag(Role::SecondaryDominant))};
        result=evaluateIntentCompletion(c);
        check(result.reason==CompletionReason::SecondaryColor,
              "secondary dominant has a secondary-color reason");
        c.continuation={chord(7,flag(Role::SecondaryLeadingTone))};
        result=evaluateIntentCompletion(c);
        check(result.reason==CompletionReason::SecondaryColor,
              "secondary leading tone has a secondary-color reason");
        for (const auto quality:{ChordQuality::Major7,ChordQuality::Minor7,ChordQuality::Dominant7,
                ChordQuality::Sus2,ChordQuality::Sus4}) {
            c.continuation={qualityChord(quality)};
            result=evaluateIntentCompletion(c);
            check(result.reason==CompletionReason::ExtendedColor,
                  "seventh and suspended chord qualities have an extended-color reason");
        }
        c.continuation={qualityChord(ChordQuality::Augmented)};
        result=evaluateIntentCompletion(c);
        check(result.reason==CompletionReason::AudibleColor,
              "augmented chord has an audible-color reason");
        c.continuation={chord(4,0,1)};
        result=evaluateIntentCompletion(c);
        check(result.reason==CompletionReason::AudibleColor,
              "altered scale degree has an audible-color reason");
        c.continuation={qualityChord(ChordQuality::Diminished7)};
        result=evaluateIntentCompletion(c);
        check(result.reason==CompletionReason::AudibleColor,
              "diminished seventh has an audible-color reason");
        c.continuation={chord(4)};
        result=evaluateIntentCompletion(c);
        check(result.score<0.5f&&result.reason==CompletionReason::NoEvidence,
              "ordinary major triad is not color evidence");
        std::cout << "intent completion cases passed\n";
    } catch (const std::exception& e) {
        std::cerr << "IntentCompletionTests: " << e.what() << '\n';
        return 1;
    }
}
