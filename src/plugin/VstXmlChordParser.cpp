#include "VstXmlChordParser.h"

#include <expat.h>

#include <charconv>
#include <cmath>
#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace harmony::plugin {
namespace {
struct Failure {
    VstXmlStatus status;
    const char* message;
};

[[noreturn]] void fail(VstXmlStatus status, const char* message) {
    throw Failure{status, message};
}

struct XmlNode {
    std::string name;
    std::map<std::string, std::string> attributes;
    std::vector<XmlNode> children;
    std::string text;
};

constexpr std::size_t maxXmlNodes = VstXmlChordParser::maxChords * 8 + 8;
constexpr std::size_t maxXmlDepth = 32;

struct XmlBuildContext {
    XML_Parser parser{};
    XmlNode root;
    std::vector<XmlNode*> stack;
    std::size_t nodes{};
    bool rootSeen{};
    bool failed{};
    VstXmlStatus status{VstXmlStatus::InvalidXml};
    const char* message{"XML 格式错误"};

    void stop(VstXmlStatus failureStatus, const char* failureMessage) noexcept {
        if (failed) return;
        failed = true;
        status = failureStatus;
        message = failureMessage;
        XML_StopParser(parser, XML_FALSE);
    }
};

template <typename Callback>
void callbackGuard(XmlBuildContext& context, Callback&& callback) noexcept {
    try {
        callback();
    } catch (const Failure& error) {
        context.stop(error.status, error.message);
    } catch (...) {
            context.stop(VstXmlStatus::InternalError, "XML 树内存分配失败");
    }
}

void XMLCALL startElement(void* userData, const XML_Char* name, const XML_Char** attributes) {
    auto& context = *static_cast<XmlBuildContext*>(userData);
    callbackGuard(context, [&] {
        if (context.nodes >= maxXmlNodes || context.stack.size() > maxXmlDepth) {
            context.stop(VstXmlStatus::ResourceLimit, "XML 深度或节点数超过安全上限");
            return;
        }

        XmlNode* node{};
        if (context.stack.empty()) {
            if (context.rootSeen) {
                context.stop(VstXmlStatus::InvalidXml, "XML 文档包含多个根节点");
                return;
            }
            context.rootSeen = true;
            node = &context.root;
            node->name = name;
        } else {
            auto& children = context.stack.back()->children;
            children.emplace_back();
            node = &children.back();
            node->name = name;
        }
        ++context.nodes;
        for (auto attribute = attributes; attribute && *attribute; attribute += 2)
            node->attributes.emplace(attribute[0], attribute[1]);
        context.stack.push_back(node);
    });
}

void XMLCALL endElement(void* userData, const XML_Char*) {
    auto& context = *static_cast<XmlBuildContext*>(userData);
    if (!context.stack.empty()) context.stack.pop_back();
}

void XMLCALL characterData(void* userData, const XML_Char* text, int length) {
    auto& context = *static_cast<XmlBuildContext*>(userData);
    if (context.stack.empty() || length <= 0) return;
    callbackGuard(context, [&] {
        context.stack.back()->text.append(text, static_cast<std::size_t>(length));
    });
}

void XMLCALL startDoctype(void* userData, const XML_Char*, const XML_Char*, const XML_Char*, int hasInternalSubset) {
    if (!hasInternalSubset) return;
    auto& context = *static_cast<XmlBuildContext*>(userData);
    context.stop(VstXmlStatus::InvalidXml, "不允许内部 DTD 子集");
}

int XMLCALL externalEntity(XML_Parser parser, const XML_Char*, const XML_Char*, const XML_Char*, const XML_Char*) {
    auto* context = static_cast<XmlBuildContext*>(XML_GetUserData(parser));
    if (context) context->stop(VstXmlStatus::InvalidXml, "不允许外部 XML 实体");
    return XML_STATUS_ERROR;
}

XmlNode readXml(std::string_view input) {
    using ParserHandle = std::unique_ptr<XML_ParserStruct, decltype(&XML_ParserFree)>;
    ParserHandle parser(XML_ParserCreate(nullptr), &XML_ParserFree);
    if (!parser) fail(VstXmlStatus::InternalError, "无法创建 Expat XML 解析器");

    XmlBuildContext context;
    context.parser = parser.get();
    XML_SetUserData(parser.get(), &context);
    XML_SetElementHandler(parser.get(), &startElement, &endElement);
    XML_SetCharacterDataHandler(parser.get(), &characterData);
    XML_SetStartDoctypeDeclHandler(parser.get(), &startDoctype);
    XML_SetExternalEntityRefHandler(parser.get(), &externalEntity);
    XML_SetParamEntityParsing(parser.get(), XML_PARAM_ENTITY_PARSING_NEVER);

    // Payloads are capped at 1 MiB before reaching Expat, so the size fits int.
    const auto parseStatus = XML_Parse(parser.get(), input.data(), static_cast<int>(input.size()), XML_TRUE);
    if (context.failed) fail(context.status, context.message);
    if (parseStatus != XML_STATUS_OK) fail(VstXmlStatus::InvalidXml, "XML 文档格式错误");
    if (!context.rootSeen || !context.stack.empty()) fail(VstXmlStatus::InvalidXml, "XML 文档缺少根节点");
    return std::move(context.root);
}

std::string_view trim(std::string_view value) {
    auto isSpace = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    while (!value.empty() && isSpace(value.front())) value.remove_prefix(1);
    while (!value.empty() && isSpace(value.back())) value.remove_suffix(1);
    return value;
}

bool whitespaceOnly(std::string_view value) { return trim(value).empty(); }

std::optional<std::string> attribute(const XmlNode& node, const char* name) {
    const auto it = node.attributes.find(name);
    if (it == node.attributes.end()) return std::nullopt;
    return it->second;
}

const XmlNode* singleChild(const XmlNode& node, const char* name, bool required) {
    const XmlNode* found{};
    for (const auto& child : node.children) {
        if (child.name != name) continue;
        if (found) fail(VstXmlStatus::InvalidChord, "和弦字段重复");
        found = &child;
    }
        if (!found && required) fail(VstXmlStatus::InvalidChord, "缺少必需的和弦字段");
    return found;
}

std::string leafText(const XmlNode& node) {
        if (!node.children.empty()) fail(VstXmlStatus::InvalidChord, "此处应为 VST-XML 叶节点");
    return std::string(trim(node.text));
}

std::int32_t noteNumber(const std::string& value) {
    std::int32_t number{};
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), number);
    if (value.empty() || parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
    fail(VstXmlStatus::InvalidChord, "keyNote/bassNote 必须是整数音符值");
    return number;
}

double quarterNotes(const std::string& value) {
    double number{};
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), number);
    if (value.empty() || parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || !std::isfinite(number))
    fail(VstXmlStatus::InvalidChord, "projectTime 无效或不是有限数值");
    return number;
}
} // namespace

VstXmlParseResult VstXmlChordParser::parse(std::string_view xml, ChordSource source) const noexcept {
    VstXmlParseResult result;
    try {
    if (xml.size() > maxPayloadBytes) fail(VstXmlStatus::ResourceLimit, "数据超过 1 MiB 安全上限");
        const auto root = readXml(xml);
        const auto version = attribute(root, "version");
        if (root.name != "vst-xml" || !version || (*version != "1.3" && *version != "1.4"))
            fail(VstXmlStatus::UnsupportedSchema, "预期根节点为 Clipboard VST-XML 1.3 或 1.4");
        if (!whitespaceOnly(root.text)) fail(VstXmlStatus::UnsupportedSchema, "Clipboard VST-XML 根节点含有意外文本");

        const XmlNode* sourceApp{};
        std::size_t chordCount{};
        for (const auto& child : root.children) {
            if (child.name == "sourceApp") {
            if (sourceApp) fail(VstXmlStatus::UnsupportedSchema, "sourceApp 节点重复");
                sourceApp = &child;
            } else if (child.name == "chord") {
                ++chordCount;
            }
        }
            if (!sourceApp) fail(VstXmlStatus::UnsupportedSchema, "Clipboard VST-XML 必须包含 sourceApp");
        result.sourceApp = leafText(*sourceApp);
        if (chordCount == 0) fail(VstXmlStatus::InvalidChord, "VST-XML 数据中没有和弦节点");
        if (chordCount > maxChords) fail(VstXmlStatus::ResourceLimit, "和弦数量超过安全上限");

        std::size_t chordIndex{};
        for (const auto& node : root.children) {
            if (node.name != "chord") continue;
            result.failedChordIndex = chordIndex;
            if (!whitespaceOnly(node.text)) fail(VstXmlStatus::InvalidChord, "chord 节点中含有意外文本");
            const auto id = attribute(node, "id");
            if (!id || id->empty()) fail(VstXmlStatus::InvalidChord, "chord 节点必须带有 id 属性");

            const auto* nameNode = singleChild(node, "name", false);
            const auto* timeNode = singleChild(node, "projectTime", false);
            const auto* keyNode = singleChild(node, "keyNote", true);
            const auto* bassNode = singleChild(node, "bassNote", false);
            const auto* colorNode = singleChild(node, "color", false);
            const auto* pitchesNode = singleChild(node, "pitches", true);
            const auto* maskNode = singleChild(node, "mask", true);

            if (!timeNode) fail(VstXmlStatus::MissingProjectTime, "和弦缺少 projectTime，未猜测起始位置");
            const auto domain = attribute(*timeNode, "domain");
            const auto timeValue = leafText(*timeNode);
            result.rawTimeDomain = domain.value_or("");
            result.rawTimeValue = timeValue;
            if (!domain) fail(VstXmlStatus::InvalidChord, "projectTime 必须带有 domain 属性");
            if (*domain != "quarterNotes")
                fail(VstXmlStatus::UnsupportedTimeDomain, "不支持此 projectTime domain；目前只接受 quarterNotes");

            ChordEvent chord;
            chord.source = source;
            chord.rawId = *id;
            chord.rawProjectTime = timeValue;
            chord.rawTimeDomain = *domain;
            chord.startQN = quarterNotes(timeValue);
            chord.rawKeyNote = leafText(*keyNode);
            chord.keyNoteValue = noteNumber(*chord.rawKeyNote);
            if (bassNode) {
                chord.rawBassNote = leafText(*bassNode);
                chord.bassNoteValue = noteNumber(*chord.rawBassNote);
            }
            if (nameNode) chord.rawName = leafText(*nameNode);
            chord.name = chord.rawName && !chord.rawName->empty() ? *chord.rawName : "(unnamed)";
            chord.extensions.pitches = leafText(*pitchesNode);
            chord.extensions.mask = leafText(*maskNode);
            if (colorNode) chord.extensions.color = leafText(*colorNode);
            chord.quality = ChordQuality::Unknown;
            result.chords.push_back(std::move(chord));
            ++chordIndex;
        }
        if (!sortAndInferDurations(result.chords)) fail(VstXmlStatus::InvalidChord, "推导和弦时长时数值溢出");
        result.failedChordIndex = 0;
        result.status = VstXmlStatus::Success;
        result.detail = "Clipboard VST-XML " + *version + " 解析完成；宿主同步效果仍需在 Cubase 手动验证";
    } catch (const Failure& error) {
        result.chords.clear();
        result.status = error.status;
        try { result.detail = error.message; } catch (...) {}
    } catch (...) {
        result.chords.clear();
        result.status = VstXmlStatus::InternalError;
        try { result.detail = "解析器内部错误或内存分配失败"; } catch (...) {}
    }
    return result;
}

} // namespace harmony::plugin
