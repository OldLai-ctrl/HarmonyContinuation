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
        else if(argc>1&&std::string(argv[1])=="rc-visibility") {
            auto view=VSTGUI::owned(new harmony::ui::MainView({0,0,1100,900},{}));
            int displays=0;
            const bool pass=view->runColorVisibilitySmoke(canvas.get(),[&](VSTGUI::CRect card,VSTGUI::CRect label,harmony::ui::ColorTone tone,bool shown){
                ++displays;
                auto pixels=VSTGUI::owned(VSTGUI::CBitmapPixelAccess::create(canvas->getBitmap()));if(!pixels)return false;
                const auto expected=tone==harmony::ui::ColorTone::Warm?VSTGUI::CColor(232,184,94,255):
                    tone==harmony::ui::ColorTone::Cool?VSTGUI::CColor(95,202,220,255):VSTGUI::CColor(180,185,194,255);
                const VSTGUI::CColor background(33,49,69,255);
                const auto pixel=[&](int x,int y){VSTGUI::CColor c;pixels->setPosition(x,y);pixels->getColor(c);return c;};
                for(int x=1;x<=4;++x)
                    if(pixel(static_cast<int>(card.left)+x,static_cast<int>(card.top)+10)!=(shown?expected:background)){std::cout<<"edge mismatch, display="<<displays<<" x="<<x<<'\n';return false;}
                // Below the mini timeline: its first block starts next to the stripe.
                if(pixel(static_cast<int>(card.left)+5,static_cast<int>(card.top)+30)!=background){std::cout<<"edge width mismatch, display="<<displays<<'\n';return false;}
                bool textPainted=false;
                for(int y=static_cast<int>(label.top);y<static_cast<int>(label.bottom);++y)
                    for(int x=static_cast<int>(label.left);x<static_cast<int>(label.right);++x)
                        textPainted|=pixel(x,y)==expected;
                if(textPainted!=shown)std::cout<<"label mismatch, display="<<displays<<'\n';
                return textPainted==shown;
            });
            std::cout<<"Continuation + Enrichment: known warmth / ambiguous neutral / 4px edge + label pixels / hints toggle independent of ranking / zh+en "<<(pass?"PASS":"FAIL")<<'\n';
            if(!pass)result=1;
            if(!pass)std::cout<<"Completed display checks: "<<displays<<'\n';
        }
        else if(argc>1&&std::string(argv[1])=="rc2-hints") {
            auto view=VSTGUI::owned(new harmony::ui::MainView({0,0,1100,900},{}));
            const bool pass=view->runEnrichmentHintSmoke(canvas.get(),[&](VSTGUI::CRect rect,harmony::ui::ColorTone tone){
                auto pixels=VSTGUI::owned(VSTGUI::CBitmapPixelAccess::create(canvas->getBitmap()));if(!pixels)return false;
                const VSTGUI::CColor expected=tone==harmony::ui::ColorTone::Warm?VSTGUI::CColor(232,184,94,255):
                    tone==harmony::ui::ColorTone::Cool?VSTGUI::CColor(95,202,220,255):VSTGUI::CColor(180,185,194,255);
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
