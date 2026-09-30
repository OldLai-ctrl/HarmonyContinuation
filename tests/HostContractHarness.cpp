#include "vstgui/lib/vstguiinit.h"
#include "host/HostEnvironment.h"
#include "plugin/HostContextAdapter.h"
#include "plugin/HostAdapters.h"
#include "plugin/RecommendationWorker.h"
#include "io/AsyncMidiImport.h"
#include "io/FileDropRouter.h"
#include "io/MidiDragService.h"
#include "ui/EffectiveScale.h"
#include "ui/MainView.h"
#include "library/LibraryStore.h"
#include "session/PluginSessionState.h"
#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/plugprovider.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/common/memorystream.h"
#include "pluginterfaces/gui/iplugviewcontentscalesupport.h"
#include "pluginterfaces/gui/iplugview.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "vstgui/lib/cframe.h"
#include "vstgui/lib/platform/platformfactory.h"
#include "vstgui/lib/platform/win32/win32factory.h"
#include <windows.h>
#include <objbase.h>
#include <ole2.h>
#include <fstream>
#include <iostream>
#include <thread>
#include <limits>
#include <set>
#include <functional>
using namespace harmony;
using namespace Steinberg;
using namespace Steinberg::Vst;
namespace {
void require(bool value){if(!value)throw std::runtime_error("contract assertion failed");}
bool ok(tresult v){return v==kResultOk;}
struct NamedHost final:HostApplication {
    std::string name;
    explicit NamedHost(std::string v):name(std::move(v)){}
    tresult PLUGIN_API getName(String128 output)override{
        std::fill_n(output,128,0);for(std::size_t i=0;i<std::min<std::size_t>(127,name.size());++i)output[i]=static_cast<TChar>(name[i]);return kResultOk;
    }
};
struct Frame final:IPlugFrame {
    int calls{};bool reject{},mutate{},reenter{};tresult reentryResult{kResultOk};
    tresult PLUGIN_API queryInterface(const TUID iid,void** obj)override{
        if(!obj)return kInvalidArgument;*obj=nullptr;
        if(FUnknownPrivate::iidEqual(iid,IPlugFrame::iid)||FUnknownPrivate::iidEqual(iid,FUnknown::iid)){*obj=this;return kResultOk;}return kNoInterface;
    }
    uint32 PLUGIN_API addRef()override{return 1;}uint32 PLUGIN_API release()override{return 1;}
    tresult PLUGIN_API resizeView(IPlugView* view,ViewRect* size)override{
        ++calls;if(reject)return kResultFalse;
        if(reenter){FUnknownPtr<IPlugViewContentScaleSupport> scale(view);if(scale)reentryResult=scale->setContentScaleFactor(1.25f);}
        if(mutate){size->right=1800;size->bottom=1000;}
        return view->onSize(size);
    }
};
struct Window {
    HWND handle{CreateWindowExW(0,L"STATIC",L"Host Contract",WS_OVERLAPPEDWINDOW,0,0,2400,1800,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr)};
    ~Window(){if(handle)DestroyWindow(handle);}
};
struct Instance {
    std::unique_ptr<PlugProvider> provider;IPtr<IComponent> component;IPtr<IEditController> controller;IPtr<IPlugView> view;Frame frame;
    Instance(const VST3::Hosting::PluginFactory& factory,const VST3::Hosting::ClassInfo& info){
        provider=std::make_unique<PlugProvider>(factory,info,true);require(provider->initialize());
        component=provider->getComponentPtr();controller=provider->getControllerPtr();require(component&&controller);
    }
    ~Instance(){if(view){view->removed();view->setFrame(nullptr);view=nullptr;}controller=nullptr;component=nullptr;provider.reset();}
    void create(){view=owned(controller->createView(ViewType::kEditor));require(view!=nullptr);require(ok(view->setFrame(&frame)));}
    void attach(HWND parent){require(ok(view->attached(parent,kPlatformTypeHWND)));}
    std::string save(){MemoryStream stream;require(ok(controller->getState(&stream)));return {stream.getData(),static_cast<std::size_t>(stream.getSize())};}
    bool restore(std::string& bytes){MemoryStream stream(bytes.data(),bytes.size());return ok(controller->setState(&stream));}
    session::PluginSessionState state(){auto bytes=save();require(bytes.size()>4);auto decoded=session::deserialize(bytes.substr(4));require(static_cast<bool>(decoded));return decoded.state;}
};
std::string streamState(const session::PluginSessionState& state){
    const auto json=session::serialize(state);const auto n=static_cast<uint32_t>(json.size());std::string bytes;
    for(int i=0;i<4;++i)bytes.push_back(static_cast<char>(n>>(i*8)));return bytes+json;
}
std::optional<io::CompletedMidiImport> wait(io::AsyncMidiImport& worker){
    for(int i=0;i<500;++i){if(auto ready=worker.takeLatest())return ready;std::this_thread::sleep_for(std::chrono::milliseconds(10));}return {};
}
}
int main(int argc,char** argv){
    if(argc<3){std::cerr<<"usage: HostContractHarness plugin.vst3 host-name [report]\n";return 2;}
    std::cout<<std::unitbuf;
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);OleInitialize(nullptr);
    int passed=0,total=0;std::ofstream report;if(argc>3)report.open(argv[3]);
    auto test=[&](const char* name,const std::function<void()>& run){++total;try{run();++passed;std::cout<<"PASS "<<name<<'\n';if(report)report<<"PASS "<<name<<'\n';}
        catch(const std::exception& e){std::cout<<"FAIL "<<name<<": "<<e.what()<<'\n';if(report)report<<"FAIL "<<name<<": "<<e.what()<<'\n';}};
    try{
    const std::string profile=argv[2];host::HostEnvironment env;env.identify(profile);
    const auto fixtures=std::filesystem::path(HC_MIDI_FIXTURES),directory=std::filesystem::path(HC_TEST_OUTPUT_DIR)/("host-contract-"+std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directories(directory);
    auto imported=midi::importFile(fixtures/"pop_block.mid");require(static_cast<bool>(imported));
    session::PluginSessionState original;midi::applyImport(imported,original.imported);original.style=Style::Jazz;original.uiZoomPercent=125;
    original.locale=session::Locale::EnUS;original.productMode=session::ProductMode::Enrich;original.constraints.melody.push_back(*parseMelodyNote("E4"));
    auto initial=streamState(original);
    test("01 standard host name classification",[&]{require(env.name==profile);require(!env.version);});
    test("02 host name does not invent capabilities",[&]{require(env.capabilities.filePathDrop==host::Observed::Unknown&&env.capabilities.editorResize==host::Observed::Unknown);});
    test("03 diagnostics privacy",[&]{auto e=env;e.identify("personal-computer-user");auto text=e.diagnostics(false,true);require(text.find("personal-computer-user")==std::string::npos&&text.find("Track ")==std::string::npos);e.identify("C:\\Users\\private");require(e.name=="Unknown");});
    test("04 bilingual wrapper help",[&]{host::HostEnvironment e;e.identify("FL Studio");require(e.diagnostics().find("Accept dropped files")!=std::string::npos&&e.diagnostics(true).find("Accept dropped files")!=std::string::npos);});
    plugin::HostContextAdapter context;ProcessContext pc{};pc.state=ProcessContext::kTempoValid|ProcessContext::kTimeSigValid|ProcessContext::kProjectTimeMusicValid|ProcessContext::kPlaying;
    pc.tempo=128;pc.projectTimeMusic=16;pc.timeSigNumerator=7;pc.timeSigDenominator=8;
    auto timeline=[&]{plugin::HostSnapshot s;context.capture(&pc,true);require(context.read(s));return plugin::HostContextAdapter::timeline(s);};
    test("05 complete context",[&]{auto t=timeline();require(t.projectTimeMusic==16&&t.tempo==128&&t.timeSignature==std::pair(7,8)&&t.playing==true&&t.hostMidiEvents);});
    test("06 missing meter retains QN and tempo",[&]{pc.state&=~ProcessContext::kTimeSigValid;auto t=timeline();require(!t.timeSignature&&t.projectTimeMusic&&t.tempo);pc.state|=ProcessContext::kTimeSigValid;});
    test("07 missing QN retains tempo and meter",[&]{pc.state&=~ProcessContext::kProjectTimeMusicValid;auto t=timeline();require(!t.projectTimeMusic&&t.tempo&&t.timeSignature);pc.state|=ProcessContext::kProjectTimeMusicValid;});
    test("08 stop play and seek",[&]{pc.state&=~ProcessContext::kPlaying;pc.projectTimeMusic=4;require(timeline().playing==false&&timeline().projectTimeMusic==4);pc.state|=ProcessContext::kPlaying;pc.projectTimeMusic=40;require(timeline().playing==true&&timeline().projectTimeMusic==40);});
    test("09 tempo and meter change",[&]{pc.tempo=96;pc.timeSigNumerator=3;pc.timeSigDenominator=4;auto t=timeline();require(t.tempo==96&&t.timeSignature==std::pair(3,4));});
    test("10 null context",[&]{plugin::HostSnapshot s;context.capture(nullptr);require(context.read(s));auto t=plugin::HostContextAdapter::timeline(s);require(!t.available&&!t.playing&&!t.tempo&&!t.projectTimeMusic);});
    test("11 invalid optional fields isolated",[&]{pc.projectTimeMusic=std::numeric_limits<double>::quiet_NaN();pc.timeSigDenominator=3;auto t=timeline();require(!t.projectTimeMusic&&!t.timeSignature&&t.tempo==96);});
    test("12 MIDI filepath routing",[&]{auto r=io::routeFiles({"input.MID","input.midi"});require(r.hasFiles&&r.midiFiles.size()==2);});
    test("13 unsupported files and embedded null rejected",[&]{require(io::routeFiles({"x.wav","x.flp",std::string("x.mid\0suffix",12)}).midiFiles.empty());});
    test("14 bounded multi-file routing",[&]{std::vector<std::string> files(100,"x.mid");require(io::routeFiles(files).midiFiles.size()==32);require(io::routeFiles({std::string(32769,'a')}).midiFiles.empty());});
    test("15 async valid import",[&]{io::AsyncMidiImport w;w.submit({fixtures/"pop_block.mid"},false);auto r=wait(w);require(r&&r->result&&r->result.extraction.chords.size()==4);});
    test("16 missing malformed multi-file first valid",[&]{io::AsyncMidiImport w;w.submit({fixtures/"missing.mid",fixtures/"malformed_truncated.mid",fixtures/"pop_block.mid"},false);auto r=wait(w);require(r&&r->result);});
    test("17 cancel and superseded job",[&]{io::AsyncMidiImport w;w.submit({fixtures/"pop_block.mid"},false);w.cancel();require(!w.takeLatest());auto generation=w.submit({fixtures/"inversion_block.mid"},false);auto r=wait(w);require(r&&r->generation==generation&&r->result);});
    test("18 async OPEN",[&]{io::AsyncMidiImport w;w.submit({fixtures/"pop_block.mid"},true);auto r=wait(w);require(r&&r->result&&r->result.extraction.chords.back().openEnded);});
    auto sequence=preview::buildSequence(original.imported,nullptr,120).sequence;
    auto clip=midi::buildClip(sequence,midi::ArrangementMode::BlockChords,midi::ExportScope::CurrentOnly,{4,4},{},{});
    require(static_cast<bool>(clip));auto payload=midi::makePayload(clip.sequence,"contract.mid");midi::ExportConfig dragConfig;dragConfig.profile=midi::ExportProfile::DAWClip;payload.dawClipBytes=midi::writeToMemory(clip.sequence,dragConfig).bytes;
    test("19 unique minimal MIDI drag files",[&]{std::set<std::filesystem::path> paths;for(int i=0;i<8;++i){auto f=io::MidiDragService{}.prepare(payload,directory);require(static_cast<bool>(f));paths.insert(f.path);std::ifstream input(f.path,std::ios::binary);std::vector<unsigned char> data(std::istreambuf_iterator<char>(input),{});require(data==payload.dawClipBytes); }require(paths.size()==8);});
    test("20 drag preparation failure safe",[&]{auto blocker=directory/"file-not-directory";std::ofstream(blocker)<<"x";require(!io::MidiDragService{}.prepare(payload,blocker));});
    test("21 preview ownership 2 4 8",[&]{for(int count:{2,4,8}){std::vector<std::unique_ptr<host::PreviewOwnership>> owners;for(int i=0;i<count;++i){owners.push_back(std::make_unique<host::PreviewOwnership>());owners.back()->claim();}for(int i=0;i<count-1;++i)require(!owners[i]->release());require(owners.back()->owns()&&owners.back()->release());}});
    test("22 DPI user zoom layout matrix 36",[&]{for(double scale:{1.,1.25,1.5,2.})for(double zoom:{1.,1.25,1.5})for(double width:{900.,1300.,2200.}){
        ui::EffectiveScale transform(scale,zoom);auto layout=ui::computeLayout({width,900,scale,session::Tab::Recommend,true,true,zoom});require(std::abs(transform.logicalEditor(transform.physicalEditor(width))-width)<.001);
        require(std::abs(transform.logicalContent(transform.logicalEditor(transform.physicalContent(20)))-20)<.001);
        require(layout.viewport.width()==width/zoom&&layout.inspector.width()>0);for(auto r:layout.topControls)require(std::isfinite(r.left)&&r.width()>0&&r.height()>0&&r.contains((r.left+r.right)/2,(r.top+r.bottom)/2));
        for(auto lane:layout.lanes){ui::ScrollableCandidateList list(lane,6,9999);require(std::isfinite(list.maximum)&&list.maximum>=0);}
    }});
    test("23 Cubase XML and Generic capability fallback",[&]{std::ifstream input(std::filesystem::path(HC_FIXTURE_DIR)/"vstxml-1.3-normal.xml");std::string xml(std::istreambuf_iterator<char>(input),{});plugin::DropReport r;plugin::DropItemReport item;item.looksLikeXml=item.containsVstXml=item.fullPayloadReadable=true;item.decodedText=xml;r.items.push_back(item);
        auto a=plugin::CubaseHostAdapter{}.chordInput(r),b=plugin::GenericVst3HostAdapter{}.chordInput(r);require(a.chords.size()==4&&b.chords.size()==4&&a.chords[1].startQN==4);require(plugin::FLStudioHostAdapter{}.chordInput(r).chords.empty());});
    std::string error;auto module=VST3::Hosting::Module::create(argv[1],error);if(!module)throw std::runtime_error(error);
    auto host=owned(new NamedHost(profile));PluginContextFactory::instance().setPluginContext(host);
    auto factory=module->getFactory();factory.setHostContext(host);std::optional<VST3::Hosting::ClassInfo> info;
    for(const auto& item:factory.classInfos())if(item.category()==kVstAudioEffectClass){info=item;break;}require(info.has_value());
    std::unique_ptr<Instance> instance;Window window,other;require(window.handle&&other.handle);
    test("24 actual SDK initialize state without view",[&]{instance=std::make_unique<Instance>(factory,*info);require(instance->restore(initial));require(instance->state().uiZoomPercent==125&&instance->state().imported.events.size()==4);});
    test("25 state before view attach",[&]{instance->create();instance->attach(window.handle);require(instance->state().style==Style::Jazz&&instance->state().constraints.melody.size()==1);});
    test("26 view before setState",[&]{auto changed=original;changed.uiZoomPercent=150;auto bytes=streamState(changed);require(instance->restore(bytes));require(instance->state().uiZoomPercent==150);});
    test("27 remove reattach recreated parent",[&]{auto before=instance->save();for(int i=0;i<6;++i){require(ok(instance->view->removed()));instance->attach(i%2?window.handle:other.handle);}require(instance->save()==before);});
    test("28 resize before attach",[&]{Instance second(factory,*info);second.create();ViewRect r{0,0,1300,900};require(ok(second.view->onSize(&r)));second.attach(other.handle);require(second.state().editorWidth==1300);});
    test("29 host resize rejected rollback",[&]{auto before=instance->save();FUnknownPtr<IPlugViewContentScaleSupport> scale(instance->view);require(scale!=nullptr);instance->frame.reject=true;require(!ok(scale->setContentScaleFactor(1.5f)));instance->frame.reject=false;require(instance->save()==before);});
    test("30 host resize accepted returned geometry",[&]{FUnknownPtr<IPlugViewContentScaleSupport> scale(instance->view);instance->frame.mutate=true;require(ok(scale->setContentScaleFactor(1.5f)));instance->frame.mutate=false;require(instance->state().editorWidth==1200&&instance->state().editorHeight==667);require(ok(scale->setContentScaleFactor(1.f)));});
    test("31 resize repeated and reentry bounded",[&]{FUnknownPtr<IPlugViewContentScaleSupport> scale(instance->view);instance->frame.reenter=true;require(ok(scale->setContentScaleFactor(2.f)));require(!ok(instance->frame.reentryResult));auto count=instance->frame.calls;require(ok(scale->setContentScaleFactor(2.f)));require(instance->frame.calls==count);instance->frame.reenter=false;require(ok(scale->setContentScaleFactor(1.f)));});
    test("32 actual editor content scale and user zoom matrix",[&]{FUnknownPtr<IPlugViewContentScaleSupport> scale(instance->view);for(float value:{1.f,1.25f,1.5f,2.f})for(unsigned zoom:{100u,125u,150u}){auto changed=original;changed.uiZoomPercent=zoom;auto bytes=streamState(changed);require(instance->restore(bytes));require(ok(scale->setContentScaleFactor(value)));ViewRect r;require(ok(instance->view->getSize(&r)));require(r.getWidth()>0&&instance->state().uiZoomPercent==zoom);}require(ok(scale->setContentScaleFactor(1.f)));});
    test("33 deactivate activate processor state and recreate",[&]{FUnknownPtr<IAudioProcessor> processor(instance->component);require(processor!=nullptr);ProcessSetup setup{kRealtime,kSample32,64,48000};require(ok(processor->setupProcessing(setup)));require(ok(instance->component->setActive(true)));require(ok(processor->setProcessing(true)));ProcessData data{};data.processMode=kRealtime;data.symbolicSampleSize=kSample32;require(ok(processor->process(data)));require(ok(processor->setProcessing(false)));require(ok(instance->component->setActive(false)));MemoryStream state;require(ok(instance->component->getState(&state)));state.seek(0,IBStream::kIBSeekSet,nullptr);require(ok(instance->component->setState(&state)));auto bytes=instance->save();instance.reset();instance=std::make_unique<Instance>(factory,*info);require(instance->restore(bytes));require(instance->save()==bytes);});
    test("34 corrupt state preserves phrase",[&]{auto before=instance->save();std::string bad="xxxx";require(!instance->restore(bad)&&instance->save()==before);});
    auto multi=[&](int count){std::vector<std::unique_ptr<Instance>> many;std::vector<std::string> states;for(int i=0;i<count;++i){many.push_back(std::make_unique<Instance>(factory,*info));auto s=original;s.editorWidth=1000+i*25;s.style=i%2?Style::Pop:Style::Jazz;states.push_back(streamState(s));require(many.back()->restore(states.back()));}for(int i=0;i<count;++i)require(many[i]->save()==states[i]);};
    test("35 two actual plugin instances",[&]{multi(2);});test("36 four actual plugin instances",[&]{multi(4);});test("37 eight actual plugin instances",[&]{multi(8);});
    test("38 isolated jobs shared read-only factory and user DB",[&]{const auto user=directory/"user.db";library::UserLibrary library(user);auto all=library.loadAll();require(static_cast<bool>(all));for(int count:{2,4,8}){std::vector<std::unique_ptr<plugin::RecommendationWorker>> workers;for(int i=0;i<count;++i){workers.push_back(std::make_unique<plugin::RecommendationWorker>(HC_FACTORY_DB_PATH,user));workers.back()->submit(original.imported.events,original.analysisContext(),{},false,original.imported.revision);}for(auto& worker:workers){bool ready=false;for(int i=0;i<1000;++i){if(auto r=worker->takeLatest()){require(r->error.empty()&&r->factoryCount==161);ready=true;break;}std::this_thread::sleep_for(std::chrono::milliseconds(10));}require(ready);}}require(library.loadAll().templates.size()==all.templates.size());
        auto templates=library::loadFactory(HC_FACTORY_DB_PATH);require(static_cast<bool>(templates));
        std::atomic<int> saved{};std::vector<std::thread> writers;
        for(int i=0;i<8;++i)writers.emplace_back([&,i]{auto item=templates.templates.front();item.sourceType="user";item.id="user-contract-"+std::to_string(i);item.name="Contract "+std::to_string(i);item.nameZh=item.nameEn=item.name;item.tags={"test"};item.favorite=true;item.note="kept";std::string error;
            if(library::UserLibrary(user).addProgression(item,error))++saved;});
        for(auto& writer:writers)writer.join();require(saved==8);auto rows=library.loadAll();require(rows.templates.size()==8);for(const auto& row:rows.templates)require(row.favorite&&row.note=="kept");
    });
    instance.reset();factory.setHostContext(nullptr);PluginContextFactory::instance().setPluginContext(nullptr);
    VSTGUI::init(GetModuleHandleW(nullptr));
    auto* platform=VSTGUI::getPlatformFactory().asWin32Factory();platform->disableDirectComposition();platform->useD2DHardwareRenderer(false);
    auto* uiFrame=new VSTGUI::CFrame({0,0,1100,900},nullptr);auto* main=new ui::MainView({0,0,1100,900},{});uiFrame->addView(main);require(uiFrame->open(window.handle,VSTGUI::PlatformType::kHWND));main->setSessionState(original);uiFrame->onActivate(true);
    test("39 focus ESC persistent compare host shortcuts",[&]{require(main->runHostInteractionSmoke());});
    test("40 More scrolling drag input identity and UI restore",[&]{require(main->runMidiWorkflowSmoke());auto saved=main->captureEditorUiState();main->restoreEditorUiState(saved);require(main->captureEditorUiState().continuationId==saved.continuationId);});
    uiFrame->close();VSTGUI::exit();
    }catch(const std::exception& e){++total;std::cerr<<"Harness setup failure: "<<e.what()<<'\n';}
    if(report)report<<"RESULT "<<passed<<'/'<<total<<" (standard SDK simulation; real FL pending)\n";
    std::cout<<"RESULT "<<passed<<'/'<<total<<" (standard SDK simulation; real FL pending)\n";
    OleUninitialize();CoUninitialize();return passed==total?0:1;
}
