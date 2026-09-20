#pragma once
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <stdexcept>
#include "../tracking/developer_commands.hpp"
#include "../tracking/developer_panel_input.hpp"

struct VrSettings {
    float hudSize=.8f,interfaceScale=1.f;
    bool interfaceView=false,interfacePending=false;
    float gripPitch{},gripYaw{},gripRoll{};unsigned selectedWeapon{};
    static constexpr int rowCount=14;
    float depth=20,convergence=100,alignment=26.5f/2560.f,scale=100,fov=130,renderScale=1,sharpness=.25f;
    bool swap=true,visible=false;int selected=0;unsigned recenter=0;
    bool developerVisible=false,developerTogglePending=false;
    int developerRow=0,developerAction=-1,developerDestination=0;
    amalur::DeveloperPanelInput developerControllers;
    bool pollDeveloperControllers(const amalur::TouchInput& touch,bool active,bool busy,uint64_t now=GetTickCount64()){
        const auto event=developerControllers.update(touch,active,developerVisible,busy,now);
        if(event.toggle){developerVisible=!developerVisible;visible=false;}
        if(event.close)developerVisible=false;
        if(developerVisible){
            developerRow=(developerRow+event.row+amalur::developer::panelRows)%amalur::developer::panelRows;
            developerDestination=(developerDestination+event.destination+amalur::developer::destinations)%amalur::developer::destinations;
            if(event.activate)developerAction=amalur::developer::panelAction(developerRow,developerDestination);
        }
        return event.capture;
    }
    bool previous[256]{},held[256]{},consumed[256]{};std::wstring path;
    HHOOK keyboard{};bool togglePending{};inline static VrSettings* input{};
    static bool starKey(DWORD key){
        if(key==VK_MULTIPLY)return true;
        DWORD thread=GetWindowThreadProcessId(GetForegroundWindow(),nullptr);
        SHORT mapping=VkKeyScanExW(L'*',GetKeyboardLayout(thread));
        if(mapping==-1||key!=LOBYTE(mapping))return false;
        int modifiers=((GetAsyncKeyState(VK_SHIFT)&0x8000)?1:0)|((GetAsyncKeyState(VK_CONTROL)&0x8000)?2:0)|((GetAsyncKeyState(VK_MENU)&0x8000)?4:0);
        return modifiers==HIBYTE(mapping);
    }
    static bool gameFocused(){DWORD pid{};GetWindowThreadProcessId(GetForegroundWindow(),&pid);if(pid==GetCurrentProcessId())return true;HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);if(!process)return false;wchar_t name[MAX_PATH];DWORD n=MAX_PATH;bool game=false;if(QueryFullProcessImageNameW(process,0,name,&n)){auto slash=wcsrchr(name,L'\\');game=_wcsicmp(slash?slash+1:name,L"koa.exe")==0;}CloseHandle(process);return game;}
    static LRESULT CALLBACK hook(int code,WPARAM event,LPARAM data){
        if(code==HC_ACTION&&input){auto key=reinterpret_cast<KBDLLHOOKSTRUCT*>(data)->vkCode;if(key<256){
            bool down=event==WM_KEYDOWN||event==WM_SYSKEYDOWN;
            bool arrow=key==VK_UP||key==VK_DOWN||key==VK_LEFT||key==VK_RIGHT;
            bool toggle=starKey(key);
            bool developerToggle=key==VK_F11&&!(GetAsyncKeyState(VK_CONTROL)&0x8000)&&!(GetAsyncKeyState(VK_MENU)&0x8000)&&!(GetAsyncKeyState(VK_SHIFT)&0x8000);
            bool interfaceToggle=key=='I'&&(GetAsyncKeyState(VK_CONTROL)&0x8000);
            bool take=gameFocused()&&(arrow||toggle||developerToggle||interfaceToggle||(input->developerVisible&&(key==VK_RETURN||key==VK_ESCAPE))||(input->visible&&(key==VK_HOME||key=='R')&&(GetAsyncKeyState(VK_CONTROL)&0x8000)));
            if(!down&&input->consumed[key])take=true;
            if(take){if(down&&developerToggle&&!input->consumed[key])input->developerTogglePending=!input->developerTogglePending;if(down&&interfaceToggle&&!input->consumed[key])input->interfacePending=!input->interfacePending;if(down&&toggle&&!input->consumed[key])input->togglePending=!input->togglePending;input->held[key]=down;input->consumed[key]=down;return 1;}
        }}return CallNextHookEx(nullptr,code,event,data);
    }
    void captureInput(){input=this;keyboard=SetWindowsHookExW(WH_KEYBOARD_LL,hook,GetModuleHandleW(nullptr),0);if(!keyboard)throw std::runtime_error("Panel keyboard capture failed");}
    ~VrSettings(){if(keyboard)UnhookWindowsHookEx(keyboard);if(input==this)input=nullptr;}
    VrSettings(){wchar_t p[MAX_PATH]{};GetModuleFileNameW(nullptr,p,MAX_PATH);path=p;path=path.substr(0,path.find_last_of(L"\\/")+1)+L"amalur-vr.ini";load();}
    bool edge(int key){bool now=held[key];bool result=now&&!previous[key];previous[key]=now;return result;}
    float read(const wchar_t* key,float fallback,float low,float high){wchar_t text[64]{};GetPrivateProfileStringW(L"VR",key,L"",text,64,path.c_str());if(!*text)return fallback;wchar_t* end{};float value=wcstof(text,&end);return end!=text&&!*end&&std::isfinite(value)?std::clamp(value,low,high):fallback;}
    void load(){interfaceScale=read(L"InterfaceScale",1,.5f,1.5f);gripPitch=read(L"GripPitch",0,-180,180);gripYaw=read(L"GripYaw",0,-180,180);gripRoll=read(L"GripRoll",0,-180,180);hudSize=read(L"HudSize",.8f,.4f,1.2f);depth=read(L"Depth",20,0,100);convergence=read(L"Convergence",100,1,1000);alignment=read(L"Alignment",26.5f/2560.f,-.05f,.05f);scale=read(L"WorldUnitsPerMeter",100,10,500);fov=read(L"HorizontalFov",130,100,150);renderScale=read(L"RenderScale",1,.5f,1.5f);sharpness=read(L"Sharpness",.25f,0,1);swap=read(L"SwapEyes",1,0,1)>.5f;}
    void save(){auto put=[&](const wchar_t* key,float value){wchar_t text[64];swprintf_s(text,L"%.7g",value);WritePrivateProfileStringW(L"VR",key,text,path.c_str());};put(L"InterfaceScale",interfaceScale);put(L"GripPitch",gripPitch);put(L"GripYaw",gripYaw);put(L"GripRoll",gripRoll);put(L"HudSize",hudSize);put(L"Depth",depth);put(L"Convergence",convergence);put(L"Alignment",alignment);put(L"WorldUnitsPerMeter",scale);put(L"HorizontalFov",fov);put(L"RenderScale",renderScale);put(L"Sharpness",sharpness);put(L"SwapEyes",swap?1.f:0.f);}
    void poll(){
        MSG message;while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}
        if(interfacePending){interfaceView=!interfaceView;interfacePending=false;}
        if(togglePending){visible=!visible;developerVisible=false;togglePending=false;}
        if(developerTogglePending){developerVisible=!developerVisible;visible=false;developerTogglePending=false;}
        bool up=edge(VK_UP),down=edge(VK_DOWN),left=edge(VK_LEFT),right=edge(VK_RIGHT),home=edge(VK_HOME),r=edge('R');
        bool enter=edge(VK_RETURN),escape=edge(VK_ESCAPE);
        if(developerVisible){
            if(escape){developerVisible=false;return;}
            if(up)developerRow=(developerRow+amalur::developer::panelRows-1)%amalur::developer::panelRows;
            if(down)developerRow=(developerRow+1)%amalur::developer::panelRows;
            if(left)developerDestination=(developerDestination+amalur::developer::destinations-1)%amalur::developer::destinations;
            if(right)developerDestination=(developerDestination+1)%amalur::developer::destinations;
            if(enter)developerAction=amalur::developer::panelAction(developerRow,developerDestination);
            return;
        }
        if(!visible)return;
        if(r)++recenter;
        if(up)selected=(selected+rowCount-1)%rowCount;if(down)selected=(selected+1)%rowCount;
        int direction=int(right)-int(left);if(!direction&&!home)return;
        switch(selected){
        case 0:hudSize=home?.8f:std::clamp(hudSize+direction*.05f,.4f,1.2f);break;
        case 1:depth=home?20:std::clamp(depth+direction*2.f,0.f,100.f);break;
        case 2:convergence=home?100:std::clamp(convergence+direction*5.f,1.f,1000.f);break;
        case 3:alignment=home?26.5f/2560.f:std::clamp(alignment+direction*.0005f,-.05f,.05f);break;
        case 4:scale=home?100:std::clamp(scale+direction*5.f,10.f,500.f);break;
        case 5:fov=home?130:std::clamp(fov+direction*2.f,100.f,150.f);break;
        case 6:renderScale=home?1:std::clamp(renderScale+direction*.1f,.5f,1.5f);break;
        case 7:sharpness=home?.25f:std::clamp(sharpness+direction*.05f,0.f,1.f);break;
        case 8:swap=home?true:!swap;break;
        case 9:gripPitch=home?0:std::clamp(gripPitch+direction*5.f,-180.f,180.f);break;
        case 10:gripYaw=home?0:std::clamp(gripYaw+direction*5.f,-180.f,180.f);break;
        case 11:gripRoll=home?0:std::clamp(gripRoll+direction*5.f,-180.f,180.f);break;
        case 12:interfaceView=home?false:!interfaceView;break;
        case 13:interfaceScale=home?1.f:std::clamp(interfaceScale+direction*.05f,.5f,1.5f);break;
        }save();
    }
};
