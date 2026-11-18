#pragma once
#include "vstgui/lib/idatapackage.h"
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
namespace harmony::plugin {
struct DropItemReport {
    std::size_t index{};
    std::string type{"未知/错误"};
    std::size_t returnedBytes{};
    std::size_t declaredBytes{};
    bool fullPayloadReadable{};
    std::string readableAs{"否"};
    bool looksLikeXml{};
    bool containsVstXml{};
    std::string printablePreview;
    std::string hexPreview;
    std::string decodedText;
};
struct DropReport {
    std::string kind{"尚未收到拖放"};
    std::string summary{"拖放状态：等待拖入"};
    std::vector<DropItemReport> items;
    std::size_t itemCount{};
    std::size_t payloadBytes{};
};
DropReport inspectDrop(VSTGUI::IDataPackage*, std::string_view source = "拖放") noexcept;
}
