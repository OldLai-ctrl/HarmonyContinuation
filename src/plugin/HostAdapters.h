#pragma once
#include "host/HostEnvironment.h"
#include "core/ImportedProgression.h"
#include "DropCapture.h"
#include <memory>
#if defined(_WIN32)
#include "platform/windows/ClipboardInspector.h"
#endif
namespace harmony::plugin {
struct HostChordInput {Progression chords;std::string details;};
class HostAdapter {
public:
    virtual ~HostAdapter()=default;
    virtual HostChordInput chordInput(const DropReport&) const=0;
};
class GenericVst3HostAdapter : public HostAdapter {
public: HostChordInput chordInput(const DropReport&) const override;
};
class CubaseHostAdapter final : public GenericVst3HostAdapter {
public:
    HostChordInput chordInput(const DropReport&) const override;
    static std::string parseXml(const std::vector<DropItemReport>&,ChordSource,Progression&);
#if defined(_WIN32)
    static std::string parseClipboard(const windows::ClipboardInspection&,Progression&);
#endif
};
class FLStudioHostAdapter final : public GenericVst3HostAdapter {
public: HostChordInput chordInput(const DropReport&) const override;
};
std::unique_ptr<HostAdapter> makeHostAdapter(host::HostFamily);
}
