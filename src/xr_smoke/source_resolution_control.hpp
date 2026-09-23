#pragma once
#include <windows.h>
#include <cstdint>
struct SourceResolutionControl {
    int pending=0,sent=0;DWORD producer=0;uint64_t changed=0;
    struct Search{DWORD pid;HWND window;};
    static BOOL CALLBACK find(HWND window,LPARAM data){
        auto& s=*reinterpret_cast<Search*>(data);DWORD pid=0;GetWindowThreadProcessId(window,&pid);
        if(pid==s.pid&&IsWindowVisible(window)&&!GetWindow(window,GW_OWNER)){s.window=window;return FALSE;}return TRUE;
    }
    void update(int percent,DWORD pid,unsigned actualWidth,unsigned actualHeight,uint64_t now){
        if(percent<50||percent>130)return;
        if(pid!=producer){producer=pid;sent=0;changed=now;}
        if(percent!=pending){pending=percent;changed=now;}
        if(!pid||!actualWidth||now-changed<800||sent==percent)return;
        const unsigned width=3840*percent/100,height=2400*percent/100;
        if(width==actualWidth&&height==actualHeight){sent=percent;return;}
        Search search{pid,nullptr};EnumWindows(find,reinterpret_cast<LPARAM>(&search));if(!search.window)return;
        const auto message=RegisterWindowMessageW(L"AmalurVR.ResizeSource.v1");
        if(message&&PostMessageW(search.window,message,width,height))sent=percent;
    }
};
