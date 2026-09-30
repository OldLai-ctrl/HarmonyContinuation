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
    result.durationQN = 4;
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
        c.continuation={chord(1)};
        auto result=evaluateIntentCompletion(c,ScaleDegree{2,0},ScaleDegree{5,0});
        check(result.score>0.9f && result.reason==CompletionReason::DominantTonic,
              "single tonic completes dominant resolution");
        c.continuation={chord(6)};
        check(evaluateIntentCompletion(c).score<0.5f,"non-tonic does not finish resolution");

        c.intent=PhraseIntent::Loop;
        c.continuation={chord(1)};
        check(evaluateIntentCompletion(c,ScaleDegree{1,0}).score>0.9f,
              "single chord can close loop");
        c.continuation={chord(4)};
        c.cadence=CadenceType::LoopClosure;
        check(evaluateIntentCompletion(c).score>0.8f,"documented loop closure");

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
        check(evaluateIntentCompletion(c).score>0.8f,"single borrowed chord adds color");
        c.continuation={chord(4)};
        check(evaluateIntentCompletion(c).score<0.5f,"plain chord is weak color evidence");
        std::cout << "intent completion cases passed\n";
    } catch (const std::exception& e) {
        std::cerr << "IntentCompletionTests: " << e.what() << '\n';
        return 1;
    }
}
