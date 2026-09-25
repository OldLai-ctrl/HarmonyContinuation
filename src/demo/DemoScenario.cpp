#include "DemoScenario.h"
#include "ProgressionJson.h"
#include <fstream>
#include <iterator>
#include <regex>
#include <stdexcept>

namespace harmony::demo {
ScenarioResult loadScenario(const std::filesystem::path& path) {
    ScenarioResult result;
    try {
        std::ifstream file(path,std::ios::binary);
        if (!file) throw std::runtime_error("cannot open demo scenario");
        const std::string data(std::istreambuf_iterator<char>{file},{});
        if (data.size()>65536) throw std::runtime_error("demo scenario too large");
        auto field=[&](const char* key)->std::optional<std::string> {
            std::smatch m; const std::regex re(std::string("\"")+key+"\"\\s*:\\s*\"([^\"]+)\"");
            if (std::regex_search(data,m,re)) return m[1].str(); return std::nullopt;
        };
        result.scenario.name=field("name").value_or(path.stem().string());
        std::smatch m;
        if (std::regex_search(data,m,std::regex("\"tempo\"\\s*:\\s*([0-9]+(?:\\.[0-9]+)?)")))
            result.scenario.tempo=std::stod(m[1].str());
        if (result.scenario.tempo<30 || result.scenario.tempo>300) throw std::runtime_error("invalid demo tempo");
        if (std::regex_search(data,m,std::regex("\"meter\"\\s*:\\s*\\[\\s*([0-9]+)\\s*,\\s*([0-9]+)\\s*\\]"))) {
            result.scenario.meterNumerator=std::stoi(m[1].str()); result.scenario.meterDenominator=std::stoi(m[2].str());
        }
        if (result.scenario.meterNumerator<1 || result.scenario.meterNumerator>32 ||
            result.scenario.meterDenominator<1 || result.scenario.meterDenominator>32) throw std::runtime_error("invalid demo meter");
        const auto chordKey=data.find("\"chords\"");
        if (chordKey==std::string::npos) throw std::runtime_error("missing chords");
        const auto begin=data.find('[',chordKey);
        if (begin==std::string::npos) throw std::runtime_error("missing chord array");
        const auto end=data.find(']',begin);
        if (end==std::string::npos) throw std::runtime_error("unclosed chord array");
        auto parsed=dev::parseProgressionJson(std::string_view(data).substr(begin,end-begin+1));
        if (!parsed) throw std::runtime_error(parsed.error);
        result.scenario.chords=std::move(parsed.events);
        if (result.scenario.chords.size()>64 || !sortAndInferDurations(result.scenario.chords)) throw std::runtime_error("invalid demo chords");
        if (auto value=field("forcedKey")) {
            constexpr const char* names[]{"C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B"};
            bool found{};
            for (int i=0;i<12;++i) for (int mode=0;mode<2;++mode) {
                const auto label=std::string(names[i])+(mode?":minor":":major");
                if (*value==label) { result.scenario.forcedKey=KeySignature{static_cast<PitchClass>(i),mode?Mode::Minor:Mode::Major}; found=true; }
            }
            if (!found) throw std::runtime_error("invalid forcedKey");
        }
        if (auto value=field("style")) {
            for (const auto& [name,s]:std::initializer_list<std::pair<const char*,Style>>{
                {"pop",Style::Pop},{"rock",Style::Rock},{"rnb",Style::Rnb},{"jazz",Style::Jazz},
                {"citypop",Style::CityPop},{"functional",Style::Functional}}) if (*value==name) result.scenario.style=s;
            if (!result.scenario.style) throw std::runtime_error("invalid style");
        }
        if (auto value=field("intent")) {
            for (const auto& [name,intent]:std::initializer_list<std::pair<const char*,PhraseIntent>>{
                {"develop",PhraseIntent::Develop},{"resolve",PhraseIntent::Resolve},{"loop",PhraseIntent::Loop},{"color",PhraseIntent::Color}})
                if (*value==name) result.scenario.intent=intent;
            if (!result.scenario.intent) throw std::runtime_error("invalid intent");
        }
    } catch (const std::exception& e) { result.scenario={}; result.error=e.what(); }
    return result;
}
} // namespace harmony::demo
