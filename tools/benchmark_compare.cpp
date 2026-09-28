#include "benchmark/BenchJson.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <stdexcept>

namespace {
using harmony::benchmark::json::Value;
std::string read(const std::filesystem::path& path){std::ifstream in(path,std::ios::binary);
    if(!in)throw std::runtime_error("cannot open "+path.string());
    return std::string(std::istreambuf_iterator<char>{in},{});}
std::map<std::string,const Value*> byCase(const Value& report){
    std::map<std::string,const Value*> result;
    for(const auto& item:report.at("cases").items())
        if(!result.emplace(item.at("id").text(),&item).second)throw std::runtime_error("duplicate report case ID");
    return result;
}
std::vector<std::string> keys(const Value& item){
    std::vector<std::string> out;
    for(const auto& key:item.at("keyCandidates").items())out.push_back(key.at("key").text());
    return out;
}
void record(Value& changes,std::string id,std::string group,std::string type,std::string detail){
    auto row=Value::map();row.object["caseId"]=std::move(id);row.object["group"]=std::move(group);
    row.object["type"]=std::move(type);row.object["detail"]=std::move(detail);
    changes.array.push_back(std::move(row));
}
void compareCase(const Value& a,const Value& b,Value& changes){
    const auto id=a.at("id").text();
    if(keys(a)!=keys(b))record(changes,id,"","KeyInterpretationChanged","key candidate set changed");
    for(const auto group:{"resolve","develop","loop","color"}){
        const auto& left=a.at("groups").at(group).items();const auto& right=b.at("groups").at(group).items();
        if(left.empty()!=right.empty())record(changes,id,group,left.empty()?"GroupGainedCandidate":"GroupBecameEmpty",
            "empty status changed");
        for(std::size_t rank=0;rank<std::min(left.size(),right.size());++rank){
            const auto& before=left[rank];const auto& after=right[rank];
            if(before.at("pathSignature").text()!=after.at("pathSignature").text())
                record(changes,id,group,"CandidateChanged","rank "+std::to_string(rank+1)+" path or hold changed");
            const auto& oldHold=before.at("openHoldQN");const auto& newHold=after.at("openHoldQN");
            if(oldHold.type==Value::Type::Number&&newHold.type==Value::Type::Number&&
                oldHold.finite()!=newHold.finite())
                record(changes,id,group,"OpenDurationChanged","rank "+std::to_string(rank+1));
        }
        std::map<std::string,const Value*> old,newer;
        for(const auto& c:left)old.emplace(c.at("fingerprint").text(),&c);
        for(const auto& c:right)newer.emplace(c.at("fingerprint").text(),&c);
        for(const auto& [fingerprint,candidate]:old){
            const auto found=newer.find(fingerprint);
            if(found==newer.end()){record(changes,id,group,"CandidateRemoved",candidate->at("pathSignature").text());continue;}
            const auto& other=*found->second;
            if(candidate->at("rank").integer(1,100000)!=other.at("rank").integer(1,100000))
                record(changes,id,group,"RankChanged",candidate->at("pathSignature").text());
        }
        for(const auto& [fingerprint,candidate]:newer)if(!old.contains(fingerprint))
            record(changes,id,group,"CandidateAdded",candidate->at("pathSignature").text());
    }
}
} // namespace
int main(int argc,char** argv){
    try{
        if(argc!=5||std::string_view(argv[3])!="--output")
            throw std::runtime_error("usage: benchmark_compare OLD.json NEW.json --output changes.json");
        const auto old=harmony::benchmark::json::parse(read(argv[1]));
        const auto newer=harmony::benchmark::json::parse(read(argv[2]));
        if(old.at("schemaVersion").integer(1,1)!=newer.at("schemaVersion").integer(1,1))
            throw std::runtime_error("incompatible report versions");
        const auto oldCases=byCase(old),newCases=byCase(newer);
        auto changes=Value::list();
        for(const auto& [id,item]:oldCases){const auto found=newCases.find(id);
            if(found==newCases.end())record(changes,id,"","CaseRemoved","case missing in new report");
            else compareCase(*item,*found->second,changes);}
        for(const auto& [id,item]:newCases)if(!oldCases.contains(id))
            record(changes,id,"","CaseAdded","new case in report");
        auto result=Value::map();result.object["schemaVersion"]=1;
        result.object["oldReport"]=std::string(argv[1]);result.object["newReport"]=std::string(argv[2]);
        result.object["changeCount"]=static_cast<int>(changes.array.size());result.object["changes"]=std::move(changes);
        const std::filesystem::path output=argv[4];
        if(!output.parent_path().empty())std::filesystem::create_directories(output.parent_path());
        std::ofstream out(output,std::ios::binary|std::ios::trunc);
        if(!out)throw std::runtime_error("cannot create comparison report");
        out<<harmony::benchmark::json::dump(result)<<'\n';out.close();
        if(!out)throw std::runtime_error("comparison write failed");
        std::cout<<"compared "<<oldCases.size()<<" old cases and "<<newCases.size()<<" new cases; changes="
                 <<result.at("changeCount").integer(0,1000000)<<'\n';
        return 0;
    }catch(const std::exception& e){std::cerr<<"compare FAIL: "<<e.what()<<'\n';return 1;}
}
