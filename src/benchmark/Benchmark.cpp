#include "Benchmark.h"
#include "BenchJson.h"
#include "session/ProductServices.h"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <set>
#include <sstream>
#include <stdexcept>

namespace harmony::benchmark {
namespace {
std::string readFile(const std::filesystem::path& path) {
    std::ifstream in(path,std::ios::binary);
    if(!in)throw std::runtime_error("cannot open "+path.string());
    std::string data(std::istreambuf_iterator<char>{in},{});
    if(data.size()>4*1024*1024)throw std::runtime_error("file exceeds 4 MiB");
    return data;
}
void validId(std::string_view id) {
    if(id.empty()||id.size()>64||!std::all_of(id.begin(),id.end(),[](unsigned char c){
        return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-';}))
        throw std::runtime_error("invalid benchmark ID");
}
void validRating(const Rating& rating) {
    validId(rating.benchmarkId);
    if(rating.candidateFingerprint.empty()||rating.candidateFingerprint.size()>16384||
       rating.candidateFingerprint!=session::continuationFingerprint(rating.recommendation.candidate))
        throw std::runtime_error("rating fingerprint does not match snapshot candidate");
    for(const auto score:{rating.naturalness,rating.intentFit,rating.rhythmFit,rating.distinctiveness,rating.usability})
        if(score<1||score>5)throw std::runtime_error("rating score outside 1-5");
    if(rating.verdict!="KEEP"&&rating.verdict!="QUESTIONABLE"&&rating.verdict!="BAD")
        throw std::runtime_error("invalid verdict");
    static const std::set<std::string> issues{"Harmony","Matching","Ranking","Intent","Rhythm",
        "Diversity","Voicing","Sound","UI","Other"};
    if(!issues.contains(rating.issueCategory)||rating.note.size()>2048)
        throw std::runtime_error("invalid issue category or note");
    if(rating.schemaVersion!=1||rating.libraryVersion<1||rating.sessionSchemaVersion<1||
       rating.matchingVersion<1||rating.recommendationVersion<1)
        throw std::runtime_error("invalid rating version context");
}
} // namespace
Case loadCase(const std::filesystem::path& path) {
    const auto root=json::parse(readFile(path));
    Case result;
    result.id=root.at("id").text();validId(result.id);
    if(path.stem().string()!=result.id)throw std::runtime_error("benchmark ID must match filename");
    result.name=root.at("name").text();result.category=root.at("category").text();
    result.notes=root.at("notes").text();
    if(result.name.empty()||result.name.size()>128||result.category.empty()||result.notes.empty())
        throw std::runtime_error("incomplete benchmark description");
    for(const auto& value:root.at("expectedCharacteristics").items()) {
        const auto& text=value.text();if(text.empty()||text.size()>256)throw std::runtime_error("invalid expected characteristic");
        result.expectedCharacteristics.push_back(text);
    }
    if(result.expectedCharacteristics.empty()||result.expectedCharacteristics.size()>8)
        throw std::runtime_error("benchmark needs 1-8 expected characteristics");
    const auto& chords=root.at("chords").items();
    if(chords.empty()||chords.size()>64)throw std::runtime_error("benchmark chord count out of range");
    const auto loaded=demo::loadScenario(path);
    if(!loaded)throw std::runtime_error(loaded.error);
    result.scenario=loaded.scenario;
    if(result.scenario.name!=result.name||result.scenario.chords.size()!=chords.size())
        throw std::runtime_error("benchmark scenario mismatch");
    return result;
}
std::vector<std::filesystem::path> caseFiles(const std::filesystem::path& directory) {
    std::vector<std::filesystem::path> files;
    if(!std::filesystem::is_directory(directory))throw std::runtime_error("benchmark directory missing");
    for(const auto& entry:std::filesystem::directory_iterator(directory))
        if(entry.is_regular_file()&&entry.path().extension()==".json")files.push_back(entry.path());
    std::sort(files.begin(),files.end());
    return files;
}
std::string ratingFilename(const Rating& rating) {
    validId(rating.benchmarkId);
    std::uint64_t hash=14695981039346656037ull;
    for(const unsigned char c:rating.candidateFingerprint){hash^=c;hash*=1099511628211ull;}
    std::ostringstream out;out<<rating.benchmarkId<<'_'<<std::hex<<std::setw(16)<<std::setfill('0')<<hash<<".rating.json";
    return out.str();
}
bool saveRating(const Rating& rating,const std::filesystem::path& directory,std::string& error) {
    try {
        validRating(rating);
        auto root=json::Value::map();
        root.object["schemaVersion"]=rating.schemaVersion;
        root.object["benchmarkId"]=rating.benchmarkId;
        root.object["candidateFingerprint"]=rating.candidateFingerprint;
        root.object["verdict"]=rating.verdict;
        root.object["issueCategory"]=rating.issueCategory;
        root.object["note"]=rating.note;
        auto scores=json::Value::map();
        scores.object["naturalness"]=rating.naturalness;
        scores.object["intentFit"]=rating.intentFit;
        scores.object["rhythmFit"]=rating.rhythmFit;
        scores.object["distinctiveness"]=rating.distinctiveness;
        scores.object["usability"]=rating.usability;
        root.object["scores"]=std::move(scores);
        auto context=json::Value::map();
        context.object["libraryVersion"]=rating.libraryVersion;
        context.object["sessionSchemaVersion"]=rating.sessionSchemaVersion;
        context.object["matchingConfigVersion"]=rating.matchingVersion;
        context.object["recommendationConfigVersion"]=rating.recommendationVersion;
        root.object["algorithmContext"]=std::move(context);
        root.object["recommendationSnapshot"]=json::parse(snapshot::serialize(rating.recommendation));
        const auto bytes=json::dump(root);
        std::filesystem::create_directories(directory);
        const auto path=directory/ratingFilename(rating);
        if(std::filesystem::exists(path)) {
            const auto prior=loadRating(path);
            if(prior.candidateFingerprint!=rating.candidateFingerprint)
                throw std::runtime_error("rating filename collision");
        }
        std::ofstream out(path,std::ios::binary|std::ios::trunc);
        if(!out)throw std::runtime_error("cannot create rating file");
        out<<bytes<<'\n';out.close();if(!out)throw std::runtime_error("rating write failed");
        const auto verified=loadRating(path);
        if(verified.candidateFingerprint!=rating.candidateFingerprint)throw std::runtime_error("rating readback mismatch");
        error.clear();return true;
    }catch(const std::exception& e){error=e.what();return false;}
}
Rating loadRating(const std::filesystem::path& path) {
    const auto root=json::parse(readFile(path));Rating rating;
    rating.schemaVersion=root.at("schemaVersion").integer(1,1);
    rating.benchmarkId=root.at("benchmarkId").text();
    rating.candidateFingerprint=root.at("candidateFingerprint").text();
    rating.verdict=root.at("verdict").text();rating.issueCategory=root.at("issueCategory").text();
    rating.note=root.at("note").text();
    const auto& scores=root.at("scores");
    rating.naturalness=scores.at("naturalness").integer(1,5);
    rating.intentFit=scores.at("intentFit").integer(1,5);
    rating.rhythmFit=scores.at("rhythmFit").integer(1,5);
    rating.distinctiveness=scores.at("distinctiveness").integer(1,5);
    rating.usability=scores.at("usability").integer(1,5);
    const auto& context=root.at("algorithmContext");
    rating.libraryVersion=context.at("libraryVersion").integer(1,1000000);
    rating.sessionSchemaVersion=context.at("sessionSchemaVersion").integer(1,1000000);
    rating.matchingVersion=context.at("matchingConfigVersion").integer(1,1000000);
    rating.recommendationVersion=context.at("recommendationConfigVersion").integer(1,1000000);
    const auto decoded=snapshot::deserialize(json::dump(root.at("recommendationSnapshot")));
    if(!decoded)throw std::runtime_error("invalid rating snapshot: "+decoded.error);
    rating.recommendation=decoded.value;validRating(rating);
    return rating;
}
} // namespace harmony::benchmark
