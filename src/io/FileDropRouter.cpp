#include "FileDropRouter.h"
#include <algorithm>
#include <cctype>
namespace harmony::io {
FileDropRoute routeFiles(const std::vector<std::string>& names){
    FileDropRoute result;result.hasFiles=!names.empty();
    for(std::size_t i=0;i<std::min<std::size_t>(32,names.size());++i)try{
        auto name=names[i];if(name.empty()||name.size()>32768)continue;
        if(!name.empty()&&name.back()=='\0')name.pop_back();
        if(name.find('\0')!=std::string::npos)continue;
        const auto path=std::filesystem::path(std::u8string(name.begin(),name.end()));
        auto extension=path.extension().string();std::transform(extension.begin(),extension.end(),extension.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
        if(extension==".mid"||extension==".midi")result.midiFiles.push_back(path);
    }catch(...){}
    return result;
}
}
