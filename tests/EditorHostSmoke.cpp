// Drive the SDK example's own native window via standard Win32 lifecycle APIs.
// This test never controls a DAW, changes host settings or decodes private data.
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
struct Found {DWORD process{};HWND window{};};
BOOL CALLBACK findWindow(HWND window,LPARAM value){
    auto& result=*reinterpret_cast<Found*>(value);DWORD pid{};GetWindowThreadProcessId(window,&pid);
    wchar_t klass[256]{};GetClassNameW(window,klass,256);
    if(pid==result.process&&std::wstring(klass)==L"VSTSDK WindowClass"){result.window=window;return FALSE;}return TRUE;
}
BOOL CALLBACK countChild(HWND,LPARAM count){++*reinterpret_cast<int*>(count);return TRUE;}
int wmain(int argc,wchar_t** argv){
    if(argc<4)return 2;std::ofstream report{std::filesystem::path(argv[3])};bool passed=true;
    const auto executable=std::filesystem::absolute(argv[1]),plugin=std::filesystem::absolute(argv[2]);
    for(int cycle=0;cycle<2;++cycle){
        std::wstring command=L"\""+executable.wstring()+L"\" --componentHandler \""+plugin.wstring()+L"\"";
        STARTUPINFOW startup{};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESHOWWINDOW;startup.wShowWindow=SW_HIDE;
        PROCESS_INFORMATION process{};
        if(!CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process)){report<<"launch failed\n";return 1;}
        WaitForInputIdle(process.hProcess,5000);
        Found found{process.dwProcessId};
        for(int retry=0;retry<200&&!found.window;++retry){EnumWindows(findWindow,reinterpret_cast<LPARAM>(&found));if(WaitForSingleObject(process.hProcess,50)==WAIT_OBJECT_0)break;}
        bool okay=found.window!=nullptr;int children{};
        if(found.window){ShowWindow(found.window,SW_HIDE);EnumChildWindows(found.window,countChild,reinterpret_cast<LPARAM>(&children));okay=okay&&children>0;
            RECT before{},after{},outer{};GetClientRect(found.window,&before);GetWindowRect(found.window,&outer);
            okay=okay&&SetWindowPos(found.window,nullptr,0,0,outer.right-outer.left+200,outer.bottom-outer.top+150,SWP_NOMOVE|SWP_NOACTIVATE|SWP_NOZORDER);
            Sleep(150);GetClientRect(found.window,&after);
            okay=okay&&after.right>before.right&&after.bottom>before.bottom;
            SendMessageTimeoutW(found.window,WM_CLOSE,0,0,SMTO_ABORTIFHUNG,2000,nullptr);
        }
        if(WaitForSingleObject(process.hProcess,5000)!=WAIT_OBJECT_0){TerminateProcess(process.hProcess,1);okay=false;}
        DWORD exit{};GetExitCodeProcess(process.hProcess,&exit);okay=okay&&exit==0;
        report<<"EditorHost open / child editor / resize / close cycle "<<cycle+1<<": "<<(okay?"PASS":"FAIL")<<"; child windows="<<children<<"; exit="<<exit<<'\n';
        passed=passed&&okay;CloseHandle(process.hThread);CloseHandle(process.hProcess);
    }
    report<<"EditorHost "<<(passed?"2/2 PASS":"ISSUE")<<"; SDK example, not FL verification\n";return passed?0:1;
}
