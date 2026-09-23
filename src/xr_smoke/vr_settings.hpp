#pragma once
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <stdexcept>
#include "../bridge_tracking/developer_commands.hpp"
#include "../bridge_tracking/head_height.hpp"
#include "../bridge_tracking/camera_depth.hpp"
#include "../bridge_tracking/cinematic_mode.hpp"
#include "../bridge_tracking/arm_thickness_settings.hpp"
#include "menu_recovery.hpp"
#include "../bridge_tracking/attack_panel_settings.hpp"
#include "../bridge_tracking/melee_debug_settings.hpp"
#include "../bridge_tracking/developer_camera.hpp"
#include "../bridge_tracking/body_debug_settings.hpp"
#include "../bridge_tracking/developer_panel_input.hpp"

struct VrSettings {
    int wristHud=1; // 0 original HUD, 1 left wrist, 2 right wrist
    float wristHudScale=1.f;
    int sourceResolutionPercent=100;
    float hudSize=.8f,interfaceScale=1.f,cinematicScale=1.3f;
    bool cinematicFullVR=true;
    bool interfaceView=false,interfacePending=false;
    bool physicalCrouch=true,seatedMode=false;
    float gripPitch{},gripYaw{},gripRoll{};unsigned selectedWeapon{};
    float weaponX{},weaponY{},weaponZ{}; // centimetres, weapon-only offsets
    static constexpr int rowCount=30,sourceResolutionRow=26,cameraDepthRow=25,wristHudRow=24,cinematicModeRow=23,heavyChargeRow=20,headHeightRow=21,armThicknessRow=22;
    unsigned heavyChargeMode{};
    int cameraDepthCm{};int headHeightCm{};int armThicknessPercent=100;
    float depth=20,convergence=100,alignment=26.5f/2560.f,scale=100,fov=130,renderScale=1,sharpness=.25f;
    bool swap=true,visible=false;int selected=0;unsigned recenter=0;
    bool developerVisible=false,developerTogglePending=false;
    inline static amalur::MeleeDebugSettings meleeDebug;
    inline static amalur::DeveloperCameraSettings thirdPersonCamera;
    static constexpr int ablationFirstRow=amalur::developer::panelRows+5;
    static constexpr int ablationResetRow=ablationFirstRow+5;
    static constexpr int attackPanelRow=ablationResetRow+1;
    static constexpr int developerPanelRows=attackPanelRow+1;
    static constexpr int mapPanelRow=developerPanelRows; // retain currently hidden prototype row
    static constexpr LONG ablationBits[]{amalur::skipRigSockets,amalur::skipRootSmoothing,amalur::nativeMeshInput,amalur::nativeMeshPositions,amalur::nativeMeshRotations};
    bool ablationAction(int row,bool activate){
        if(row<ablationFirstRow||row>ablationResetRow)return false;
        if(activate){if(row==ablationResetRow)amalur::bodyDebug.resetAblations();else amalur::bodyDebug.toggle(ablationBits[row-ablationFirstRow]);}
        return true;
    }
    bool mapPanelPrototype=true;
    inline static amalur::AttackPanelSettings attackPanel;
    int developerRow=0,developerAction=-1,developerDestination=0;
    // Exactly one tab can be visible. Every open starts on Settings.
    bool panelOpen() const {return visible||developerVisible;}
    void closePanel(){visible=developerVisible=false;}
    void togglePanel(){if(panelOpen())closePanel();else {visible=true;developerVisible=false;}}
    void switchTab(){if(panelOpen()){bool dev=developerVisible;developerVisible=!dev;visible=dev;}}
    MenuRecovery menuRecovery;
    bool pauseNativeRequested{};
    bool pauseRequested{};uint64_t pauseUntil{};
    void requestPause(){closePanel();pauseRequested=true;pauseUntil=0;}
    bool applyPauseInput(const amalur::TouchInput& t,bool active,amalur::MotionInputPacket& packet,uint64_t now=GetTickCount64()){
        if(!active){pauseRequested=pauseNativeRequested=false;pauseUntil=0;return false;}
        if(!pauseRequested&&!pauseUntil)return false;
        packet.buttons=0;packet.moveX=packet.moveY=0;packet.block=packet.abilities=0;
        packet.lookX=packet.lookY=packet.supportGrip=0;packet.active=1;
        if(pauseRequested){
            if(t.a||t.b||t.x||t.y||t.rightTrigger>.25f||t.leftClick||t.rightClick)return true;
            pauseRequested=false;pauseNativeRequested=true;pauseUntil=now+180;
        }
        if(now>=pauseUntil)pauseUntil=0;
        return true;
    }
    amalur::DeveloperPanelInput developerControllers;
    bool pollDeveloperControllers(const amalur::TouchInput& touch,bool active,bool busy,uint64_t now=GetTickCount64()){
        const auto event=developerControllers.update(touch,active,panelOpen(),developerVisible&&busy&&developerRow<amalur::developer::panelRows,now);
        if(event.toggle)togglePanel();
        if(event.close)closePanel();
        if(event.tab){switchTab();return true;}
        if(visible){
            selected=(selected+event.row+rowCount)%rowCount;
            if(selected==rowCount-3){if(event.activate||event.destination)menuRecovery.toggleAutomaticContinue();}
            else if(selected==rowCount-2){if(event.activate)requestPause();}
            else if(selected==rowCount-1){if(event.activate)++recenter;}
            else if(event.destination||event.reset||((selected==0||selected==18||selected==19||selected==heavyChargeRow||selected==cinematicModeRow||selected==wristHudRow)&&event.activate))adjustSetting(event.destination?event.destination:1,event.reset);
        }
        if(developerVisible){
            developerRow=(developerRow+event.row+developerPanelRows)%developerPanelRows;
            developerDestination=(developerDestination+event.destination+amalur::developer::destinations)%amalur::developer::destinations;
            if(developerRow==attackPanelRow){if(event.activate||event.destination)attackPanel.toggle();}
            else if(ablationAction(developerRow,event.activate||event.destination)){}
            else if(developerRow==amalur::developer::panelRows){if(event.activate||event.destination)meleeDebug.toggle();}
            else if(developerRow==amalur::developer::panelRows+1){if(event.activate||event.destination)thirdPersonCamera.toggle();}
            else if(developerRow==amalur::developer::panelRows+2){if(event.activate||event.destination)amalur::bodyDebug.toggle(amalur::nativeTorso);}
            else if(developerRow==amalur::developer::panelRows+3){if(event.activate||event.destination)amalur::bodyDebug.toggle(amalur::nativeArms);}
            else if(developerRow==mapPanelRow){if(event.activate||event.destination)mapPanelPrototype=!mapPanelPrototype;}
            else if(developerRow==amalur::developer::panelRows+4){if(event.activate||event.destination)amalur::bodyDebug.toggle(amalur::nativeCamera);}
            else if(event.activate)developerAction=amalur::developer::panelAction(developerRow,developerDestination);
        }
        return event.capture;
    }
    bool previous[256]{},held[256]{},consumed[256]{};std::wstring path;
    int heldRowKey{};uint64_t rowKeyRepeatAt{};
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
            bool take=gameFocused()&&(arrow||toggle||developerToggle||interfaceToggle||(input->panelOpen()&&(key==VK_RETURN||key==VK_ESCAPE||key==VK_TAB))||(input->visible&&(key==VK_HOME||key=='R')&&(GetAsyncKeyState(VK_CONTROL)&0x8000)));
            if(!down&&input->consumed[key])take=true;
            if(take){if(down&&developerToggle&&!input->consumed[key])input->developerTogglePending=!input->developerTogglePending;if(down&&interfaceToggle&&!input->consumed[key])input->interfacePending=!input->interfacePending;if(down&&toggle&&!input->consumed[key])input->togglePending=!input->togglePending;input->held[key]=down;input->consumed[key]=down;return 1;}
        }}return CallNextHookEx(nullptr,code,event,data);
    }
    void captureInput(){input=this;keyboard=SetWindowsHookExW(WH_KEYBOARD_LL,hook,GetModuleHandleW(nullptr),0);if(!keyboard)throw std::runtime_error("Panel keyboard capture failed");}
    ~VrSettings(){if(keyboard)UnhookWindowsHookEx(keyboard);if(input==this)input=nullptr;}
    VrSettings(){wchar_t p[MAX_PATH]{};GetModuleFileNameW(nullptr,p,MAX_PATH);path=p;path=path.substr(0,path.find_last_of(L"\\/")+1)+L"amalur-vr.ini";load();}
    bool edge(int key){bool now=held[key];bool result=now&&!previous[key];previous[key]=now;return result;}
    float read(const wchar_t* key,float fallback,float low,float high){wchar_t text[64]{};GetPrivateProfileStringW(L"VR",key,L"",text,64,path.c_str());if(!*text)return fallback;wchar_t* end{};float value=wcstof(text,&end);return end!=text&&!*end&&std::isfinite(value)?std::clamp(value,low,high):fallback;}
    void migrateGripBasis(){
        if(read(L"GripBasisVersion",0,0,1)>=1)return;
        // Old trims compensated for a controller/bone-axis mismatch. Retain
        // their values for reference, but do not stack them on the fixed basis.
        const wchar_t* keys[]{L"PreviousGripPitch",L"PreviousGripYaw",L"PreviousGripRoll",L"PreviousWeaponX",L"PreviousWeaponY",L"PreviousWeaponZ"};
        const float values[]{gripPitch,gripYaw,gripRoll,weaponX,weaponY,weaponZ};
        for(unsigned i=0;i<6;++i){wchar_t text[64];swprintf_s(text,L"%.7g",values[i]);WritePrivateProfileStringW(L"VR",keys[i],text,path.c_str());}
        gripPitch=gripYaw=gripRoll=weaponX=weaponY=weaponZ=0;
        save();
    }
    void load(){sourceResolutionPercent=int(read(L"GameResolutionPercent",100,50,130));cameraDepthCm=int(read(L"CameraDepthCm",0,-100,100));amalur::cameraDepth.set(cameraDepthCm);wristHud=int(read(L"WristHud",1,0,2));wristHudScale=read(L"WristHudScale",1,.5f,2.f);cinematicFullVR=read(L"CinematicFullVR",1,0,1)>.5f;amalur::cinematicMode.setFullVR(cinematicFullVR);armThicknessPercent=int(read(L"ArmThicknessPercent",100,50,150));amalur::armThickness.set(armThicknessPercent);cinematicScale=read(L"CinematicScale",1.3f,.5f,3.f);headHeightCm=int(read(L"HeadHeightCm",0,-100,100));amalur::headHeight.set(headHeightCm);heavyChargeMode=read(L"HeavyChargeInput",0,0,1)>.5f?1u:0u;seatedMode=read(L"SeatedMode",0,0,1)>.5f;physicalCrouch=read(L"PhysicalCrouch",1,0,1)>.5f;amalur::playMode.set(read(L"NormalThirdPerson",0,0,1)>.5f);weaponX=read(L"WeaponX",0,-20,20);weaponY=read(L"WeaponY",0,-20,20);weaponZ=read(L"WeaponZ",0,-20,20);interfaceScale=read(L"InterfaceScale",1,.5f,1.5f);gripPitch=read(L"GripPitch",0,-180,180);gripYaw=read(L"GripYaw",0,-180,180);gripRoll=read(L"GripRoll",0,-180,180);hudSize=read(L"HudSize",.8f,.4f,1.2f);depth=read(L"Depth",20,0,100);convergence=read(L"Convergence",100,1,1000);alignment=read(L"Alignment",26.5f/2560.f,-.05f,.05f);scale=read(L"WorldUnitsPerMeter",100,10,500);fov=read(L"HorizontalFov",130,100,150);renderScale=read(L"RenderScale",1,.5f,1.5f);sharpness=read(L"Sharpness",.25f,0,1);swap=read(L"SwapEyes",1,0,1)>.5f;migrateGripBasis();}
    void saveCinematicScale(){wchar_t text[64];swprintf_s(text,L"%.7g",cinematicScale);WritePrivateProfileStringW(L"VR",L"CinematicScale",text,path.c_str());}
    void save(){amalur::cameraDepth.set(cameraDepthCm);amalur::cinematicMode.setFullVR(cinematicFullVR);amalur::armThickness.set(armThicknessPercent);amalur::headHeight.set(headHeightCm);auto put=[&](const wchar_t* key,float value){wchar_t text[64];swprintf_s(text,L"%.7g",value);WritePrivateProfileStringW(L"VR",key,text,path.c_str());};put(L"GameResolutionPercent",float(sourceResolutionPercent));put(L"WristHud",float(wristHud));put(L"WristHudScale",wristHudScale);put(L"ArmThicknessPercent",float(armThicknessPercent));put(L"CinematicFullVR",cinematicFullVR?1.f:0.f);put(L"CameraDepthCm",float(cameraDepthCm));put(L"HeadHeightCm",float(headHeightCm));put(L"SeatedMode",seatedMode?1.f:0.f);put(L"PhysicalCrouch",physicalCrouch?1.f:0.f);put(L"NormalThirdPerson",amalur::playMode.normal()?1.f:0.f);put(L"GripBasisVersion",1);put(L"WeaponX",weaponX);put(L"WeaponY",weaponY);put(L"WeaponZ",weaponZ);put(L"HeavyChargeInput",float(heavyChargeMode));put(L"InterfaceScale",interfaceScale);put(L"GripPitch",gripPitch);put(L"GripYaw",gripYaw);put(L"GripRoll",gripRoll);put(L"HudSize",hudSize);put(L"Depth",depth);put(L"Convergence",convergence);put(L"Alignment",alignment);put(L"WorldUnitsPerMeter",scale);put(L"HorizontalFov",fov);put(L"RenderScale",renderScale);put(L"Sharpness",sharpness);put(L"SwapEyes",swap?1.f:0.f);}
    void poll(){
        MSG message;while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}
        if(interfacePending){interfaceView=!interfaceView;interfacePending=false;}
        if(togglePending||developerTogglePending){togglePanel();togglePending=developerTogglePending=false;}
        bool up=edge(VK_UP),down=edge(VK_DOWN),left=edge(VK_LEFT),right=edge(VK_RIGHT),home=edge(VK_HOME),r=edge('R');
        bool enter=edge(VK_RETURN),escape=edge(VK_ESCAPE),tab=edge(VK_TAB);
        const int rowKey=panelOpen()?int(held[VK_DOWN])-int(held[VK_UP]):0;
        const auto now=GetTickCount64();
        if(!rowKey){heldRowKey=0;rowKeyRepeatAt=0;}
        else if(rowKey!=heldRowKey){heldRowKey=rowKey;rowKeyRepeatAt=now+350;}
        else if(now>=rowKeyRepeatAt){up=rowKey<0;down=rowKey>0;rowKeyRepeatAt=now+75;}
        if(up||down){enter=false;home=false;}

        if(escape&&panelOpen()){closePanel();return;}
        if(tab&&panelOpen()){switchTab();return;}
        if(developerVisible){
            if(escape){developerVisible=false;return;}
            if(up)developerRow=(developerRow+developerPanelRows-1)%developerPanelRows;
            if(down)developerRow=(developerRow+1)%developerPanelRows;
            if(ablationAction(developerRow,enter||left||right))return;
            if(developerRow==amalur::developer::panelRows){if(enter||left||right)meleeDebug.toggle();return;}
            if(developerRow==amalur::developer::panelRows+1){if(enter||left||right)thirdPersonCamera.toggle();return;}
            if(developerRow==amalur::developer::panelRows+2){if(enter||left||right)amalur::bodyDebug.toggle(amalur::nativeTorso);return;}
            if(developerRow==amalur::developer::panelRows+3){if(enter||left||right)amalur::bodyDebug.toggle(amalur::nativeArms);return;}
            if(developerRow==mapPanelRow){if(enter||left||right)mapPanelPrototype=!mapPanelPrototype;return;}
            if(developerRow==amalur::developer::panelRows+4){if(enter||left||right)amalur::bodyDebug.toggle(amalur::nativeCamera);return;}
            if(left)developerDestination=(developerDestination+amalur::developer::destinations-1)%amalur::developer::destinations;
            if(right)developerDestination=(developerDestination+1)%amalur::developer::destinations;
            if(developerRow==attackPanelRow){if(enter||left||right)attackPanel.toggle();return;}
            if(enter)developerAction=amalur::developer::panelAction(developerRow,developerDestination);
            return;
        }
        if(!visible)return;
        if(r)++recenter;
        if(up)selected=(selected+rowCount-1)%rowCount;if(down)selected=(selected+1)%rowCount;
        if(selected==rowCount-3){if(enter||left||right)menuRecovery.toggleAutomaticContinue();return;}
        if(selected==rowCount-2){if(enter)requestPause();return;}
        if(selected==rowCount-1){if(enter)++recenter;return;}
        adjustSetting(((selected==0||selected==18||selected==19||selected==heavyChargeRow||selected==cinematicModeRow||selected==wristHudRow)&&enter)?1:int(right)-int(left),home);
    }
    void adjustSetting(int direction,bool home){
        if(!direction&&!home)return;
        switch(selected){
        case sourceResolutionRow:sourceResolutionPercent=home?100:std::clamp(sourceResolutionPercent+direction*10,50,130);break;
        case wristHudRow:wristHud=home?1:(wristHud+direction+3)%3;break;
        case 0:amalur::playMode.set(home?false:!amalur::playMode.normal());++recenter;break;
        case 1:hudSize=home?.8f:std::clamp(hudSize+direction*.05f,.4f,1.2f);break;
        case 2:depth=home?20:std::clamp(depth+direction*2.f,0.f,100.f);break;
        case 3:convergence=home?100:std::clamp(convergence+direction*5.f,1.f,1000.f);break;
        case 4:alignment=home?26.5f/2560.f:std::clamp(alignment+direction*.0005f,-.05f,.05f);break;
        case 5:scale=home?100:std::clamp(scale+direction*5.f,10.f,500.f);break;
        case 6:fov=home?130:std::clamp(fov+direction*2.f,100.f,150.f);break;
        case 7:renderScale=home?1:std::clamp(renderScale+direction*.1f,.5f,1.5f);break;
        case 8:sharpness=home?.25f:std::clamp(sharpness+direction*.05f,0.f,1.f);break;
        case 9:swap=home?true:!swap;break;
        case 10:gripPitch=home?0:std::clamp(gripPitch+direction*5.f,-180.f,180.f);break;
        case 11:gripYaw=home?0:std::clamp(gripYaw+direction*5.f,-180.f,180.f);break;
        case 12:gripRoll=home?0:std::clamp(gripRoll+direction*5.f,-180.f,180.f);break;
        case 13:interfaceView=home?false:!interfaceView;break;
        case 14:interfaceScale=home?1.f:std::clamp(interfaceScale+direction*.05f,.5f,1.5f);break;
        case 15:weaponX=home?0:std::clamp(weaponX+direction*.5f,-20.f,20.f);break;
        case 16:weaponY=home?0:std::clamp(weaponY+direction*.5f,-20.f,20.f);break;
        case 19:{const bool next=home?false:!seatedMode;if(next!=seatedMode){seatedMode=next;++recenter;}break;}
        case cinematicModeRow:cinematicFullVR=home?true:!cinematicFullVR;break;
        case 22:armThicknessPercent=home?100:std::clamp(armThicknessPercent+direction*5,50,150);break;
        case cameraDepthRow:cameraDepthCm=home?0:std::clamp(cameraDepthCm+direction,-100,100);break;
        case 21:headHeightCm=home?0:std::clamp(headHeightCm+direction,-100,100);break;
        case 20:heavyChargeMode=home?0u:1u-heavyChargeMode;break;
        case 18:physicalCrouch=home?true:!physicalCrouch;break;
        case 17:weaponZ=home?0:std::clamp(weaponZ+direction*.5f,-20.f,20.f);break;
        }save();
    }
};
