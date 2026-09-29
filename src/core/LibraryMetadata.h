#pragma once
#include <array>
#include <optional>
#include <string_view>

namespace harmony {
// Stable built-in identifiers. User free tags are separate strings on the template.
enum class TagID {
    CityPopJpop, CommonMajor, CommonMinor, Functional, Jazz, Pop, RnbSoul, Rock,
    Resolve, Develop, Loop, Color, SecondaryDominant, SecondaryLeadingTone,
    PassingDiminished, BorrowedChord, SeventhChord, CadentialExpansion
};
struct TagDefinition { TagID id; std::string_view value; };
inline constexpr std::array<TagDefinition,18> builtInTagDefinitions{{
    {TagID::CityPopJpop,"citypop-jpop"},{TagID::CommonMajor,"common-major"},
    {TagID::CommonMinor,"common-minor"},{TagID::Functional,"functional"},
    {TagID::Jazz,"jazz"},{TagID::Pop,"pop"},{TagID::RnbSoul,"rnb-soul"},
    {TagID::Rock,"rock"},{TagID::Resolve,"resolve"},{TagID::Develop,"develop"},
    {TagID::Loop,"loop"},{TagID::Color,"color"},
    {TagID::SecondaryDominant,"secondary_dominant"},
    {TagID::SecondaryLeadingTone,"secondary_leading_tone"},
    {TagID::PassingDiminished,"passing_diminished"},
    {TagID::BorrowedChord,"borrowed_chord"},
    {TagID::SeventhChord,"seventh_chord"},
    {TagID::CadentialExpansion,"cadential_expansion"}
}};
inline std::optional<TagID> parseTagID(std::string_view value) noexcept {
    for(const auto& definition:builtInTagDefinitions)
        if(definition.value==value)return definition.id;
    return {};
}
} // namespace harmony
