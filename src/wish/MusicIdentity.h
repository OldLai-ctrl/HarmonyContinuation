#pragma once
#include "preview/PreviewSequence.h"

namespace harmony::wish {
// Exact realized route identity, independent of labels, explanations and draw order.
std::string performanceFingerprint(const preview::Sequence&, int meterNumerator, int meterDenominator);
// Versioned binding includes the existing source projection and its complete C-reference realization.
std::string sourceFingerprint(const ProgressionTemplate&);
}
