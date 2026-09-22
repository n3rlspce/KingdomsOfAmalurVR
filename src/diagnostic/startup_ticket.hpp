#pragma once
#include <windows.h>
#include <cwchar>
// Written once when the game DLL attaches. Lua consumes it before its first
// main-menu decision; recreating a Lua state cannot rearm automatic Continue.
namespace startup_ticket {
inline void initialize(){
    wchar_t path[32768]{};
    DWORD size=GetModuleFileNameW(nullptr,path,32768);
    if(!size||size>=32768)return;
    auto slash=wcsrchr(path,L'\\');if(!slash||_wcsicmp(slash+1,L"koa.exe"))return;
    constexpr wchar_t suffix[]=L"mods\\amalur_boot_continue.lua";
    if(size_t(slash-path)+1+sizeof(suffix)/sizeof(wchar_t)>32768)return;
    wcscpy_s(slash+1,32768-size_t(slash-path)-1,suffix);
    HANDLE file=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return;
    constexpr char script[]="return true\n";DWORD written{};
    WriteFile(file,script,sizeof(script)-1,&written,nullptr);CloseHandle(file);
}
}
