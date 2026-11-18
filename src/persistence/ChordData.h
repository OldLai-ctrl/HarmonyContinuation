#pragma once
#include "core/ProgressionMatcher.h"
#include "benchmark/BenchJson.h"

namespace harmony::persistence {
benchmark::json::Value chordOut(const ChordEvent&);
ChordEvent chordIn(const benchmark::json::Value&);
std::string encodeChords(const Progression&);
Progression decodeChords(std::string_view);
// Same decode from an already parsed node: avoids dumping the subtree only to re-parse it.
Progression decodeChordsValue(const benchmark::json::Value&);
std::string encodeTemplateData(const ProgressionTemplate&);
void decodeTemplateData(std::string_view, ProgressionTemplate&);
} // namespace harmony::persistence
