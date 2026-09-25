#include "Processor.h"
#include "Controller.h"
#include "PluginIds.h"
#include "public.sdk/source/main/pluginfactory.h"
using namespace Steinberg;
using namespace Steinberg::Vst;
using namespace harmony::plugin;
BEGIN_FACTORY_DEF("HarmonyContinuation", "", "")
DEF_CLASS2(INLINE_UID_FROM_FUID(processorId), PClassInfo::kManyInstances, kVstAudioEffectClass,
           "HarmonyContinuation", Vst::kDistributable, "Fx|Tools", "0.0.1", kVstVersionString, Processor::create)
DEF_CLASS2(INLINE_UID_FROM_FUID(controllerId), PClassInfo::kManyInstances, kVstComponentControllerClass,
           "HarmonyContinuation Controller", 0, "", "0.0.1", kVstVersionString, Controller::create)
END_FACTORY
