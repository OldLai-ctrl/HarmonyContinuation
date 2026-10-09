#include "ui/MainView.h"
#include "vstgui/lib/coffscreencontext.h"
#include "vstgui/lib/vstguiinit.h"
#include <windows.h>
#include <objbase.h>
#include <iostream>
int main(int argc,char** argv) {
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);VSTGUI::init(GetModuleHandleW(nullptr));
    int result=0;
    {
        auto canvas=VSTGUI::COffscreenContext::create({1100,900});
        if(!canvas)result=1;
        else for(const bool enrich:{false,true}) {
            if(argc>1&&std::string(argv[1])!=(enrich?"enrichment":"continuation"))continue;
            auto view=VSTGUI::owned(new harmony::ui::MainView({0,0,1100,900},{}));
            const bool pass=view->runWhyV2Smoke(canvas.get(),enrich);
            std::cout<<(enrich?"Enrichment":"Continuation")<<" card / Why V2 / toggles / neutral fallback / Preview+MIDI identity "<<(pass?"PASS":"FAIL")<<'\n';
            if(!pass){result=1;break;}
        }
    }
    VSTGUI::exit();CoUninitialize();return result;
}
