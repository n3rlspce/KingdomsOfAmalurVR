#pragma once

// Resize through the game's normal WM_SIZE path so its scene targets and
// viewport follow the client size. Do not upscale only the final backbuffer.
namespace source_resolution {
static WNDPROC original{};
static HWND window{};
static int width{},height{};
static UINT resizeMessage{};
static wchar_t configPath[MAX_PATH]{};
static LRESULT CALLBACK procedure(HWND hwnd,UINT message,WPARAM w,LPARAM l){
    if(message==resizeMessage){
        if(w||l){
            if(w<1920||w>4992||l<1200||l>3120||w*5!=static_cast<WPARAM>(l)*8)return 0;
            width=static_cast<int>(w);height=static_cast<int>(l);
        }
        RECT outer{},client{};GetWindowRect(hwnd,&outer);GetClientRect(hwnd,&client);
        SetWindowPos(hwnd,nullptr,0,0,width+(outer.right-outer.left)-(client.right-client.left),
            height+(outer.bottom-outer.top)-(client.bottom-client.top),SWP_NOZORDER|SWP_NOACTIVATE);
        GetClientRect(hwnd,&client);
        log("VR source client requested=%dx%d actual=%ldx%ld\n",width,height,client.right,client.bottom);
        if(client.right==width&&client.bottom==height&&configPath[0]){
            wchar_t value[32];swprintf_s(value,L"%d",width);WritePrivateProfileStringW(L"Source",L"Width",value,configPath);
            swprintf_s(value,L"%d",height);WritePrivateProfileStringW(L"Source",L"Height",value,configPath);
        }
        return 0;
    }
    auto result=CallWindowProcW(original,hwnd,message,w,l);
    if(message==WM_GETMINMAXINFO){
        auto limits=reinterpret_cast<MINMAXINFO*>(l);
        limits->ptMaxTrackSize.x=width+256;limits->ptMaxTrackSize.y=height+256;
        limits->ptMaxSize=limits->ptMaxTrackSize;
    }
    return result;
}
static void apply(IDXGISwapChain* chain){
    static bool attempted=false;if(attempted||!gameBase)return;
    amalur::PosePacket pose;if(!poseChannel.open(false)||!poseChannel.read(pose)||!pose.gameMode)return;
    DXGI_SWAP_CHAIN_DESC desc{};if(FAILED(chain->GetDesc(&desc))||!desc.Windowed)return;
    HWND candidate=desc.OutputWindow;
    if(GetWindowThreadProcessId(candidate,nullptr)!=GetCurrentThreadId())return;
    attempted=true;
    wchar_t path[MAX_PATH]{};GetModuleFileNameW(selfModule,path,MAX_PATH);
    auto slash=wcsrchr(path,L'\\');if(!slash)return;wcscpy_s(slash+1,MAX_PATH-(slash+1-path),L"amalur-source.ini");
    wcscpy_s(configPath,path);
    width=GetPrivateProfileIntW(L"Source",L"Width",0,path);
    height=GetPrivateProfileIntW(L"Source",L"Height",0,path);
    if(width<1280||width>7680||height<720||height>4320)return;
    resizeMessage=RegisterWindowMessageW(L"AmalurVR.ResizeSource.v1");if(!resizeMessage)return;
    SetLastError(0);
    original=reinterpret_cast<WNDPROC>(SetWindowLongPtrW(candidate,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(procedure)));
    if(!original){log("VR source window subclass failed: %lu\n",GetLastError());return;}
    window=candidate;PostMessageW(window,resizeMessage,0,0);
}
}
