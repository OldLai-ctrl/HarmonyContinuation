#include "ui/MainView.h"
#include "vstgui/lib/coffscreencontext.h"
#include "vstgui/lib/vstguiinit.h"
#include "vstgui/lib/cbitmap.h"
#include <windows.h>
#include <objbase.h>
#include <iostream>
int main(int argc,char** argv) {
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);VSTGUI::init(GetModuleHandleW(nullptr));
    int result=0;
    {
        auto canvas=VSTGUI::COffscreenContext::create({1100,900});
        if(!canvas)result=1;
        else if(argc>1&&std::string(argv[1])=="rc2-hints") {
            auto view=VSTGUI::owned(new harmony::ui::MainView({0,0,1100,900},{}));
            const bool pass=view->runEnrichmentHintSmoke(canvas.get(),[&](VSTGUI::CRect rect,harmony::ui::ColorTone tone){
                auto pixels=VSTGUI::owned(VSTGUI::CBitmapPixelAccess::create(canvas->getBitmap()));if(!pixels)return false;
                const VSTGUI::CColor expected=tone==harmony::ui::ColorTone::Warm?VSTGUI::CColor(202,177,119,255):
                    tone==harmony::ui::ColorTone::Cool?VSTGUI::CColor(114,171,188,255):VSTGUI::CColor(148,173,200,255);
                VSTGUI::CColor actual;pixels->setPosition(static_cast<uint32_t>(rect.left+1),static_cast<uint32_t>(rect.top+10));pixels->getColor(actual);
                return actual==expected;
            });
            std::cout<<"Enrichment complete / OPEN static hints / ambiguous fallback / Continuation stripe pixels "<<(pass?"PASS":"FAIL")<<'\n';
            if(!pass)result=1;
        }
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
