#include "DropCapture.h"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <sstream>

namespace harmony::plugin {
namespace {
constexpr std::size_t maxItems = 32;
constexpr std::size_t maxItemPreview = 1024 * 1024;
constexpr std::size_t maxTotalPreview = 1024 * 1024;

bool validUtf8(std::string_view value) {
    std::size_t at{};
    while (at < value.size()) {
        const auto first = static_cast<unsigned char>(value[at]);
        std::size_t count{};
        std::uint32_t cp{};
        if (first < 0x80) { count = 1; cp = first; }
        else if (first >= 0xC2 && first <= 0xDF) { count = 2; cp = first & 0x1F; }
        else if (first >= 0xE0 && first <= 0xEF) { count = 3; cp = first & 0x0F; }
        else if (first >= 0xF0 && first <= 0xF4) { count = 4; cp = first & 0x07; }
        else return false;
        if (at + count > value.size()) return false;
        for (std::size_t i = 1; i < count; ++i) {
            const auto next = static_cast<unsigned char>(value[at + i]);
            if ((next & 0xC0) != 0x80) return false;
            cp = (cp << 6) | (next & 0x3F);
        }
        if ((count == 3 && cp < 0x800) || (count == 4 && cp < 0x10000) ||
            (cp >= 0xD800 && cp <= 0xDFFF) || cp > 0x10FFFF) return false;
        at += count;
    }
    return true;
}

void appendUtf8(std::string& output, std::uint32_t cp) {
    if (cp <= 0x7F) output.push_back(static_cast<char>(cp));
    else if (cp <= 0x7FF) {
        output.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        output.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
        output.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        output.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        output.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        output.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

bool decodeUtf16(std::string_view bytes, bool littleEndian, std::string& output) {
    if (bytes.size() % 2 != 0) return false;
    output.clear();
    for (std::size_t i = 0; i < bytes.size(); i += 2) {
        const auto a = static_cast<unsigned char>(bytes[i]);
        const auto b = static_cast<unsigned char>(bytes[i + 1]);
        std::uint32_t cp = littleEndian ? (a | (static_cast<std::uint32_t>(b) << 8))
                                        : (b | (static_cast<std::uint32_t>(a) << 8));
        if (cp == 0) break;
        if (cp >= 0xD800 && cp <= 0xDBFF) {
            if (i + 3 >= bytes.size()) return false;
            const auto c = static_cast<unsigned char>(bytes[i + 2]);
            const auto d = static_cast<unsigned char>(bytes[i + 3]);
            const auto low = littleEndian ? (c | (static_cast<std::uint32_t>(d) << 8))
                                          : (d | (static_cast<std::uint32_t>(c) << 8));
            if (low < 0xDC00 || low > 0xDFFF) return false;
            cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
            i += 2;
        } else if (cp >= 0xDC00 && cp <= 0xDFFF) return false;
        appendUtf8(output, cp);
    }
    return true;
}

std::string typeName(VSTGUI::IDataPackage::Type type) {
    if (type == VSTGUI::IDataPackage::kText) return "文本（VSTGUI 抽象类型）";
    if (type == VSTGUI::IDataPackage::kFilePath) return "文件路径（VSTGUI 抽象类型）";
    if (type == VSTGUI::IDataPackage::kBinary) return "二进制（VSTGUI 抽象类型）";
    if (type == VSTGUI::IDataPackage::kError) return "错误/未知";
    return "未知 VSTGUI 类型";
}

std::string makePreview(std::string_view value, std::size_t cap) {
    std::ostringstream out;
    const auto length = std::min(value.size(), cap);
    for (std::size_t i = 0; i < length; ++i) {
        const auto c = static_cast<unsigned char>(value[i]);
        if (c == '\n') out << "\\n";
        else if (c == '\r') out << "\\r";
        else if (c == '\t') out << "\\t";
        else if (c >= 0x20 && c != 0x7F) out << static_cast<char>(c);
        else out << "\\x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(c) << std::dec;
    }
    if (value.size() > length) out << "…";
    return out.str();
}

std::string hexPreview(std::string_view value, std::size_t count) {
    std::ostringstream out;
    const auto length = std::min(value.size(), count);
    for (std::size_t i = 0; i < length; ++i)
        out << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<unsigned>(static_cast<unsigned char>(value[i])) << (i + 1 == length ? "" : " ");
    if (value.size() > length) out << " …";
    return out.str();
}

void classifyText(DropItemReport& item, std::string_view raw, bool preferredUtf8) {
    if (preferredUtf8) while (!raw.empty() && raw.back() == '\0') raw.remove_suffix(1);
    std::string decoded;
    bool likelyUtf16{};
    if (raw.size() >= 2 && static_cast<unsigned char>(raw[0]) == 0xFF && static_cast<unsigned char>(raw[1]) == 0xFE) {
        likelyUtf16 = true;
        if (decodeUtf16(raw.substr(2), true, decoded)) item.readableAs = "UTF-16LE";
    } else if (raw.size() >= 2 && static_cast<unsigned char>(raw[0]) == 0xFE && static_cast<unsigned char>(raw[1]) == 0xFF) {
        likelyUtf16 = true;
        if (decodeUtf16(raw.substr(2), false, decoded)) item.readableAs = "UTF-16BE";
    } else if (raw.size() >= 4 && raw.size() % 2 == 0) {
        std::size_t oddZeros{};
        std::size_t evenZeros{};
        for (std::size_t i = 1; i < std::min<std::size_t>(raw.size(), 64); i += 2)
            if (raw[i] == '\0') ++oddZeros;
        for (std::size_t i = 0; i < std::min<std::size_t>(raw.size(), 64); i += 2)
            if (raw[i] == '\0') ++evenZeros;
        if (oddZeros >= 4) {
            likelyUtf16 = true;
            if (decodeUtf16(raw, true, decoded)) item.readableAs = "UTF-16LE（无 BOM，按字节特征判断）";
        } else if (evenZeros >= 4) {
            likelyUtf16 = true;
            if (decodeUtf16(raw, false, decoded)) item.readableAs = "UTF-16BE（无 BOM，按字节特征判断）";
        }
    }
    if (item.readableAs == "否" && !likelyUtf16) {
        while (!raw.empty() && raw.back() == '\0') raw.remove_suffix(1);
    }
    if (item.readableAs == "否" && !likelyUtf16 && validUtf8(raw)) {
        decoded.assign(raw);
        if (decoded.starts_with("\xEF\xBB\xBF")) decoded.erase(0, 3);
        item.readableAs = preferredUtf8 ? "UTF-8（VSTGUI 文本）" : "UTF-8";
    }
    if (item.readableAs == "否") return;
    item.decodedText = std::move(decoded);
    auto content = std::string_view(item.decodedText);
    while (!content.empty() && (content.front() == ' ' || content.front() == '\t' || content.front() == '\r' || content.front() == '\n')) content.remove_prefix(1);
    item.looksLikeXml = !content.empty() && content.front() == '<';
    item.containsVstXml = item.decodedText.find("<vst-xml") != std::string::npos;
    item.printablePreview = makePreview(item.decodedText, 128);
}
} // namespace

DropReport inspectDrop(VSTGUI::IDataPackage* data, std::string_view source) noexcept {
    DropReport out;
    try {
        if (!data) {
            out.kind = "没有 IDataPackage";
            out.summary = "失败阶段：回调收到空 IDataPackage";
            return out;
        }
        const auto count = data->getCount();
        out.itemCount = std::min<std::size_t>(count, maxItems);
        std::size_t previewedBytes{};
        bool anyText{}, anyPath{}, anyBinary{}, anyError{};
        std::ostringstream info;
        info << "来源：" << source << "；IDataPackage 数据项=" << count << "（已检查 " << out.itemCount << " 项）\n";
        out.items.reserve(out.itemCount);
        for (std::size_t i = 0; i < out.itemCount; ++i) {
            VSTGUI::IDataPackage::Type type{};
            const void* buffer{};
            const auto returned = data->getData(static_cast<uint32_t>(i), buffer, type);
            const auto declared = data->getDataSize(static_cast<uint32_t>(i));
            if (declared > std::numeric_limits<std::size_t>::max() - out.payloadBytes)
                out.payloadBytes = std::numeric_limits<std::size_t>::max();
            else
                out.payloadBytes += declared;
            DropItemReport item;
            item.index = i;
            item.type = typeName(type);
            item.returnedBytes = returned;
            item.declaredBytes = declared;
            anyText |= type == VSTGUI::IDataPackage::kText;
            anyPath |= type == VSTGUI::IDataPackage::kFilePath;
            anyBinary |= type == VSTGUI::IDataPackage::kBinary;
            anyError |= type == VSTGUI::IDataPackage::kError;

            const auto room = previewedBytes < maxTotalPreview ? maxTotalPreview - previewedBytes : 0;
            const auto length = std::min<std::size_t>({returned, declared, maxItemPreview, room});
            item.fullPayloadReadable = buffer && returned >= declared && declared <= maxItemPreview && length == declared;
            if (buffer && length) {
                const std::string_view bytes(static_cast<const char*>(buffer), length);
                previewedBytes += length;
                item.hexPreview = hexPreview(bytes, 64);
                if (type == VSTGUI::IDataPackage::kText || type == VSTGUI::IDataPackage::kFilePath || type == VSTGUI::IDataPackage::kBinary)
                    classifyText(item, bytes, type == VSTGUI::IDataPackage::kText);
            }
            out.items.push_back(std::move(item));
            const auto& saved = out.items.back();
            info << "数据项 " << saved.index << "：类型=" << saved.type
                 << "；声明大小=" << saved.declaredBytes << " 字节；返回大小=" << saved.returnedBytes << " 字节"
                 << "；UTF-8/UTF-16 可读性=" << saved.readableAs
                 << "；包含 <vst-xml=" << (saved.containsVstXml ? "是" : "否")
                 << "；XML 开头=" << (saved.looksLikeXml ? "是" : "否");
            if (!buffer && declared) info << "；读取失败：数据指针为空";
            if (declared > length) info << "；预览已截断或不可用";
            if (!saved.printablePreview.empty()) info << "；可打印预览=\"" << saved.printablePreview << '"';
            if (!saved.hexPreview.empty()) info << "；前 64 字节 HEX=" << saved.hexPreview;
            info << '\n';
        }
        if (count > out.itemCount) info << "未检查：还有 " << count - out.itemCount << " 项（最多检查 32 项）\n";
        info << "预览内存上限：总计 " << maxTotalPreview << " 字节；每项 " << maxItemPreview << " 字节。\n"
             << "VSTGUI IDataPackage 只提供抽象类型；Windows 原生剪贴板格式 ID 由独立检查器读取。";
        out.summary = info.str();
        if (anyError) out.kind = "错误/未知（可能包含其他数据项）";
        else if (anyText && !anyPath && !anyBinary) out.kind = "文本";
        else if (anyBinary && !anyText && !anyPath) out.kind = "二进制";
        else if (anyPath && !anyText && !anyBinary) out.kind = "文件路径";
        else out.kind = "混合/未知";
    } catch (...) {
        out.kind = "检查失败";
        out.summary = "失败阶段：有界 IDataPackage 检查发生异常";
        out.items.clear();
    }
    return out;
}
} // namespace harmony::plugin
