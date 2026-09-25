#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "ClipboardInspector.h"

#include <algorithm>
#include <climits>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <string_view>

namespace harmony::plugin::windows {
namespace {
constexpr std::size_t maxFormats = 128;
constexpr std::size_t maxItemBytes = 1024 * 1024;
constexpr std::size_t maxTotalBytes = 1024 * 1024;

class ClipboardGuard {
public:
    explicit ClipboardGuard(bool opened) : opened_(opened) {}
    ~ClipboardGuard() { if (opened_) CloseClipboard(); }
private:
    bool opened_{};
};

class GlobalUnlockGuard {
public:
    explicit GlobalUnlockGuard(HGLOBAL memory) : memory_(memory) {}
    ~GlobalUnlockGuard() { if (memory_) GlobalUnlock(memory_); }
private:
    HGLOBAL memory_{};
};

std::string wideToUtf8(const wchar_t* value, int length) {
    if (!value || length <= 0) return {};
    const int required = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, length, nullptr, 0, nullptr, nullptr);
    if (required <= 0) return {};
    std::string result(static_cast<std::size_t>(required), '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, length, result.data(), required, nullptr, nullptr) != required) return {};
    return result;
}

std::string formatName(UINT format) {
    switch (format) {
        case CF_TEXT: return "CF_TEXT";
        case CF_BITMAP: return "CF_BITMAP";
        case CF_METAFILEPICT: return "CF_METAFILEPICT";
        case CF_SYLK: return "CF_SYLK";
        case CF_DIF: return "CF_DIF";
        case CF_TIFF: return "CF_TIFF";
        case CF_OEMTEXT: return "CF_OEMTEXT";
        case CF_DIB: return "CF_DIB";
        case CF_PALETTE: return "CF_PALETTE";
        case CF_PENDATA: return "CF_PENDATA";
        case CF_RIFF: return "CF_RIFF";
        case CF_WAVE: return "CF_WAVE";
        case CF_UNICODETEXT: return "CF_UNICODETEXT";
        case CF_ENHMETAFILE: return "CF_ENHMETAFILE";
        case CF_HDROP: return "CF_HDROP";
        case CF_LOCALE: return "CF_LOCALE";
        case CF_DIBV5: return "CF_DIBV5";
        default: break;
    }
    wchar_t name[256]{};
    const int length = GetClipboardFormatNameW(format, name, static_cast<int>(std::size(name)));
    if (length > 0) return wideToUtf8(name, length);
    return "（未命名的标准或注册格式）";
}

std::string payloadType(UINT format) {
    if (format == CF_UNICODETEXT) return "Unicode 文本（UTF-16LE）";
    if (format == CF_TEXT) return "ANSI 文本（系统代码页）";
    if (format == CF_OEMTEXT) return "OEM 文本（系统 OEM 代码页）";
    if (format == CF_HDROP) return "文件列表句柄";
    if (format == CF_BITMAP || format == CF_METAFILEPICT || format == CF_ENHMETAFILE || format == CF_PALETTE)
        return "GDI 句柄";
    return "二进制/全局内存（如可用）";
}

bool validUtf8(std::string_view value) {
    std::size_t i{};
    while (i < value.size()) {
        const auto first = static_cast<unsigned char>(value[i]);
        std::size_t count{};
        std::uint32_t cp{};
        if (first < 0x80) { count = 1; cp = first; }
        else if (first >= 0xC2 && first <= 0xDF) { count = 2; cp = first & 0x1F; }
        else if (first >= 0xE0 && first <= 0xEF) { count = 3; cp = first & 0x0F; }
        else if (first >= 0xF0 && first <= 0xF4) { count = 4; cp = first & 0x07; }
        else return false;
        if (i + count > value.size()) return false;
        for (std::size_t j = 1; j < count; ++j) {
            const auto next = static_cast<unsigned char>(value[i + j]);
            if ((next & 0xC0) != 0x80) return false;
            cp = (cp << 6) | (next & 0x3F);
        }
        if ((count == 3 && cp < 0x800) || (count == 4 && cp < 0x10000) ||
            (cp >= 0xD800 && cp <= 0xDFFF) || cp > 0x10FFFF) return false;
        i += count;
    }
    return true;
}

std::string preview(std::string_view value) {
    std::ostringstream out;
    for (std::size_t i = 0; i < std::min<std::size_t>(value.size(), 128); ++i) {
        const auto c = static_cast<unsigned char>(value[i]);
        if (c == '\n') out << "\\n";
        else if (c == '\r') out << "\\r";
        else if (c == '\t') out << "\\t";
        else if (c >= 0x20 && c != 0x7F) out << static_cast<char>(c);
        else out << "\\x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(c) << std::dec;
    }
    if (value.size() > 128) out << "…";
    return out.str();
}

std::string hex(std::string_view value) {
    std::ostringstream out;
    const auto size = std::min<std::size_t>(value.size(), 64);
    for (std::size_t i = 0; i < size; ++i)
        out << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<unsigned>(static_cast<unsigned char>(value[i])) << (i + 1 == size ? "" : " ");
    if (value.size() > size) out << " …";
    return out.str();
}

void analyzeText(ClipboardFormatReport& item, UINT format, std::string_view bytes) {
    std::string text;
    if (format == CF_UNICODETEXT) {
        const auto* wide = reinterpret_cast<const wchar_t*>(bytes.data());
        const auto count = bytes.size() / sizeof(wchar_t);
        std::size_t length{};
        while (length < count && wide[length] != L'\0') ++length;
        text = wideToUtf8(wide, static_cast<int>(std::min<std::size_t>(length, INT_MAX)));
        if (!text.empty()) item.readableAs = "UTF-16LE";
    } else if (format == CF_TEXT || format == CF_OEMTEXT) {
        const auto nul = bytes.find('\0');
        const auto length = nul == std::string_view::npos ? bytes.size() : nul;
        const UINT codePage = format == CF_TEXT ? CP_ACP : CP_OEMCP;
        const int wideLength = MultiByteToWideChar(codePage, 0, bytes.data(), static_cast<int>(std::min<std::size_t>(length, INT_MAX)), nullptr, 0);
        if (wideLength > 0) {
            std::wstring wide(static_cast<std::size_t>(wideLength), L'\0');
            if (MultiByteToWideChar(codePage, 0, bytes.data(), static_cast<int>(std::min<std::size_t>(length, INT_MAX)), wide.data(), wideLength) == wideLength)
                text = wideToUtf8(wide.data(), wideLength);
        }
        item.readableAs = format == CF_TEXT ? "系统 ANSI 代码页" : "系统 OEM 代码页";
    } else if (bytes.size() >= 2 && bytes.size() % 2 == 0 &&
               static_cast<unsigned char>(bytes[0]) == 0xFF && static_cast<unsigned char>(bytes[1]) == 0xFE) {
        const auto* wide = reinterpret_cast<const wchar_t*>(bytes.data() + 2);
        const auto count = (bytes.size() - 2) / sizeof(wchar_t);
        std::size_t length{};
        while (length < count && wide[length] != L'\0') ++length;
        text = wideToUtf8(wide, static_cast<int>(std::min<std::size_t>(length, INT_MAX)));
        if (!text.empty()) item.readableAs = "UTF-16LE";
    } else if (bytes.size() >= 2 && bytes.size() % 2 == 0 &&
               static_cast<unsigned char>(bytes[0]) == 0xFE && static_cast<unsigned char>(bytes[1]) == 0xFF) {
        std::wstring wide;
        for (std::size_t i = 2; i + 1 < bytes.size(); i += 2) {
            wchar_t value = static_cast<wchar_t>((static_cast<unsigned char>(bytes[i]) << 8) |
                                                  static_cast<unsigned char>(bytes[i + 1]));
            if (!value) break;
            wide.push_back(value);
        }
        text = wideToUtf8(wide.data(), static_cast<int>(std::min<std::size_t>(wide.size(), INT_MAX)));
        if (!text.empty()) item.readableAs = "UTF-16BE";
    } else if (bytes.size() >= 4 && bytes.size() % 2 == 0) {
        std::size_t oddZeros{};
        std::size_t evenZeros{};
        for (std::size_t i = 1; i < std::min<std::size_t>(bytes.size(), 64); i += 2) if (bytes[i] == '\0') ++oddZeros;
        for (std::size_t i = 0; i < std::min<std::size_t>(bytes.size(), 64); i += 2) if (bytes[i] == '\0') ++evenZeros;
        if (oddZeros >= 4) {
            const auto* wide = reinterpret_cast<const wchar_t*>(bytes.data());
            const auto count = bytes.size() / sizeof(wchar_t);
            std::size_t length{};
            while (length < count && wide[length] != L'\0') ++length;
            text = wideToUtf8(wide, static_cast<int>(std::min<std::size_t>(length, INT_MAX)));
            if (!text.empty()) item.readableAs = "UTF-16LE（按字节特征判断）";
        } else if (evenZeros >= 4) {
            std::wstring wide;
            for (std::size_t i = 0; i + 1 < bytes.size(); i += 2) {
                const wchar_t value = static_cast<wchar_t>((static_cast<unsigned char>(bytes[i]) << 8) |
                                                            static_cast<unsigned char>(bytes[i + 1]));
                if (!value) break;
                wide.push_back(value);
            }
            text = wideToUtf8(wide.data(), static_cast<int>(std::min<std::size_t>(wide.size(), INT_MAX)));
            if (!text.empty()) item.readableAs = "UTF-16BE（按字节特征判断）";
        }
    }
    if (text.empty() && item.readableAs == "否") {
        const auto nul = bytes.find('\0');
        const auto asText = nul == std::string_view::npos ? bytes : bytes.substr(0, nul);
        if (validUtf8(asText)) {
            text.assign(asText);
            item.readableAs = "UTF-8";
        }
    }
    if (text.empty()) return;
    if (text.starts_with("\xEF\xBB\xBF")) text.erase(0, 3);
    item.decodedText = std::move(text);
    auto first = item.decodedText.find_first_not_of(" \t\r\n");
    item.looksLikeXml = first != std::string::npos && item.decodedText[first] == '<';
    item.containsVstXml = item.decodedText.find("<vst-xml") != std::string::npos;
    item.printablePreview = preview(item.decodedText);
}

} // namespace

ClipboardInspection inspectNativeClipboard() noexcept {
    ClipboardInspection result;
    try {
        if (!OpenClipboard(nullptr)) {
            result.summary = "无法打开 Windows 剪贴板；Win32 错误码=" + std::to_string(GetLastError());
            return result;
        }
        ClipboardGuard guard(true);
        result.opened = true;
        std::size_t totalBytes{};
        UINT format = 0;
        std::size_t formatsSeen{};
        while (formatsSeen < maxFormats) {
            SetLastError(ERROR_SUCCESS);
            format = EnumClipboardFormats(format);
            if (format == 0) break;
            ++formatsSeen;
            ClipboardFormatReport item;
            item.formatId = format;
            item.formatName = formatName(format);
            item.payloadType = payloadType(format);
            HANDLE handle = GetClipboardData(format);
            if (!handle) {
                item.error = "读取剪贴板数据失败；Win32 错误码=" + std::to_string(GetLastError());
                result.formats.push_back(std::move(item));
                continue;
            }
            item.dataAvailable = true;
            if (item.payloadType == "GDI 句柄") {
                item.error = "无法读取数据大小；GDI 句柄不会按全局内存读取";
                result.formats.push_back(std::move(item));
                continue;
            }
            const auto declared = GlobalSize(handle);
            item.payloadBytes = declared;
            if (declared == 0) {
                item.error = "此格式或句柄类型不支持读取数据大小";
                result.formats.push_back(std::move(item));
                continue;
            }
            if (item.payloadType == "文件列表句柄") {
                item.error = "检测到文件列表句柄；VSTGUI IDataPackage 路径抽象会单独报告";
                result.formats.push_back(std::move(item));
                continue;
            }
            const auto room = totalBytes < maxTotalBytes ? maxTotalBytes - totalBytes : 0;
            const auto readBytes = std::min<std::size_t>({declared, maxItemBytes, room});
            if (readBytes == 0) {
                item.error = "受 1 MiB 总读取上限约束，已跳过数据预览";
                result.formats.push_back(std::move(item));
                continue;
            }
            const void* locked = GlobalLock(handle);
            if (!locked) {
                item.error = "GlobalLock 失败；Win32 错误码=" + std::to_string(GetLastError());
                result.formats.push_back(std::move(item));
                continue;
            }
            GlobalUnlockGuard unlock(static_cast<HGLOBAL>(handle));
            const std::string_view bytes(static_cast<const char*>(locked), readBytes);
            item.fullPayloadReadable = readBytes == declared;
            item.hexPreview = hex(bytes);
            analyzeText(item, format, bytes);
            totalBytes += readBytes;
            if (declared > readBytes) item.error = "预览受单项/总内存上限截断";
            result.formats.push_back(std::move(item));
        }
        if (format == 0 && GetLastError() != ERROR_SUCCESS)
            result.summary = "枚举 Windows 剪贴板格式时发生 Win32 错误：" + std::to_string(GetLastError()) + "\n";
        std::ostringstream out;
        out << result.summary << "Windows 剪贴板格式（用户主动检查）：已检查 " << result.formats.size() << " 项\n";
        for (const auto& item : result.formats) {
            out << "格式 ID=" << item.formatId << "；名称=" << item.formatName
                << "；数据类型=" << item.payloadType << "；大小=";
            if (item.dataAvailable) out << item.payloadBytes << " 字节"; else out << "不可用";
            out << "；可读编码=" << item.readableAs
                << "；包含 <vst-xml=" << (item.containsVstXml ? "是" : "否")
                << "；XML 开头=" << (item.looksLikeXml ? "是" : "否");
            if (!item.printablePreview.empty()) out << "；文本预览=\"" << item.printablePreview << '"';
            if (!item.hexPreview.empty()) out << "；前 64 字节 HEX=" << item.hexPreview;
            if (!item.error.empty()) out << "；详情=" << item.error;
            out << '\n';
        }
        if (formatsSeen == maxFormats) out << "已达到 128 种格式的安全上限，停止枚举。\n";
        out << "剪贴板格式通过 Win32 枚举；文本解码仅用于诊断，不代表已验证与 Cubase 互通。";
        result.summary = out.str();
    } catch (...) {
        result.summary = "用户主动检查 Windows 剪贴板时发生错误。";
        result.formats.clear();
    }
    return result;
}

} // namespace harmony::plugin::windows
