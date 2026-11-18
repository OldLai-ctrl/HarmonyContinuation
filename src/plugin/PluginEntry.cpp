#include "Processor.h"
#include "Controller.h"
#include "PluginIds.h"
#include "product/ProductVersion.h"
#include "public.sdk/source/main/pluginfactory.h"
using namespace Steinberg;
using namespace Steinberg::Vst;
using namespace harmony::plugin;
BEGIN_FACTORY_DEF("HarmonyContinuation", "", "")
DEF_CLASS2(INLINE_UID_FROM_FUID(processorId), PClassInfo::kManyInstances, kVstAudioEffectClass,
           "HarmonyContinuation", Vst::kDistributable, "Fx|Tools", HC_PRODUCT_VERSION, kVstVersionString, Processor::create)
DEF_CLASS2(INLINE_UID_FROM_FUID(controllerId), PClassInfo::kManyInstances, kVstComponentControllerClass,
           "HarmonyContinuation Controller", 0, "", HC_PRODUCT_VERSION, kVstVersionString, Controller::create)
END_FACTORY
