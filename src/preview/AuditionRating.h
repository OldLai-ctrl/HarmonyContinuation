#pragma once
#include <string>
namespace harmony::preview {
enum class Verdict { Unrated, Keep, Revise, Reject };
struct AuditionRating {
    int naturalness{}, intentFit{}, rhythmFit{}, distinctiveness{};
    Verdict verdict{Verdict::Unrated};
    std::string note;
};
} // namespace harmony::preview
