#pragma once
#include <windows.h>
#include <string>

// Called on first Present, outside DllMain/loader lock. Only release installs
// containing AutoStart.ps1 opt in; development bridge launching is unchanged.
namespace bridge_autostart {
inline void start() {
    wchar_t path[32768]{};
    const auto size=GetModuleFileNameW(nullptr,path,32768);
    if(!size||size>=32768)return;
    std::wstring game(path);const auto slash=game.find_last_of(L"\\/");
    if(slash==std::wstring::npos||_wcsicmp(game.c_str()+slash+1,L"koa.exe"))return;
    const auto folder=game.substr(0,slash)+L"\\AmalurVR";
    const auto script=folder+L"\\AutoStart.ps1";
    if(GetFileAttributesW(script.c_str())==INVALID_FILE_ATTRIBUTES)return;
    wchar_t system[32768]{};
    if(!GetSystemDirectoryW(system,32768))return;
    const auto powershell=std::wstring(system)+L"\\WindowsPowerShell\\v1.0\\powershell.exe";
    auto command=L"\""+powershell+L"\" -NoProfile -ExecutionPolicy Bypass -File \""+script+L"\"";
    STARTUPINFOW startup{};startup.cb=sizeof(startup);
    startup.dwFlags=STARTF_USESHOWWINDOW;startup.wShowWindow=SW_HIDE;
    PROCESS_INFORMATION process{};
    if(CreateProcessW(powershell.c_str(),&command[0],nullptr,nullptr,FALSE,
        CREATE_NO_WINDOW,nullptr,folder.c_str(),&startup,&process)){
        CloseHandle(process.hThread);CloseHandle(process.hProcess);
    }
}
inline void once(){static const bool started=[](){start();return true;}();(void)started;}
}
