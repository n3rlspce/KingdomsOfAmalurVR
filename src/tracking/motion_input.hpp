#pragma once
#include <windows.h>
#include <Xinput.h>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace amalur {
struct MotionInputPacket {
    uint32_t version{2},active{};
    uint64_t tick{};
    float moveX{},moveY{};
    uint32_t buttons{};
    float block{},abilities{};
};
inline bool validMotionInput(const MotionInputPacket& p,uint64_t now){
    return p.version==2&&p.active==1&&p.tick<=now&&now-p.tick<250
        &&std::isfinite(p.moveX)&&std::isfinite(p.moveY)
        &&std::abs(p.moveX)<=1&&std::abs(p.moveY)<=1
        &&!(p.buttons&~0xf3ffu)&&std::isfinite(p.block)&&std::isfinite(p.abilities)
        &&p.block>=0&&p.block<=1&&p.abilities>=0&&p.abilities<=1;
}
inline void deadzone(float& x,float& y){
    float n=std::sqrt(x*x+y*y);
    if(!std::isfinite(n)||n<=.2f){x=y=0;return;}
    float magnitude=(std::fmin(n,1.f)-.2f)/.8f;
    x=x/n*magnitude;y=y/n*magnitude;
}
// Face buttons keep Xbox labels. Grip/trigger placement leaves all four spell
// slots available while holding the ability modifier. Stick clicks use the two
// otherwise missing gamepad buttons (Back and RB); the right stick is the D-pad.
struct TouchInput {
    float leftX{},leftY{},rightX{},rightY{},leftTrigger{},rightTrigger{},leftGrip{},rightGrip{};
    bool a{},b{},x{},y{},leftClick{},rightClick{},menu{};
};
class TouchMapper {
    bool attack_{};
public:
    MotionInputPacket map(TouchInput t,bool active){
        MotionInputPacket p;if(!active){attack_=false;return p;}p.active=1;
        p.moveX=t.leftX;p.moveY=t.leftY;deadzone(p.moveX,p.moveY);
        p.block=t.leftTrigger;p.abilities=t.rightGrip;
        attack_=t.rightTrigger>=(attack_?.45f:.65f);
        if(t.a)p.buttons|=XINPUT_GAMEPAD_A;if(t.b)p.buttons|=XINPUT_GAMEPAD_B;
        if(t.x||attack_)p.buttons|=XINPUT_GAMEPAD_X;if(t.y)p.buttons|=XINPUT_GAMEPAD_Y;
        if(t.leftGrip>.65f)p.buttons|=XINPUT_GAMEPAD_LEFT_SHOULDER;
        if(t.rightClick)p.buttons|=XINPUT_GAMEPAD_RIGHT_SHOULDER;
        if(t.leftClick)p.buttons|=XINPUT_GAMEPAD_BACK;if(t.menu)p.buttons|=XINPUT_GAMEPAD_START;
        if(t.rightX<-.65f)p.buttons|=XINPUT_GAMEPAD_DPAD_LEFT;
        if(t.rightX>.65f)p.buttons|=XINPUT_GAMEPAD_DPAD_RIGHT;
        if(t.rightY<-.65f)p.buttons|=XINPUT_GAMEPAD_DPAD_DOWN;
        if(t.rightY>.65f)p.buttons|=XINPUT_GAMEPAD_DPAD_UP;
        return p;
    }
};
inline void mergeMotion(XINPUT_GAMEPAD& pad,const MotionInputPacket& p){
    pad.wButtons|=static_cast<WORD>(p.buttons);
    pad.bLeftTrigger=static_cast<BYTE>(std::fmax(pad.bLeftTrigger,p.block*255));
    pad.bRightTrigger=static_cast<BYTE>(std::fmax(pad.bRightTrigger,p.abilities*255));
    // A real controller remains usable when the virtual stick is neutral.
    if(p.moveX!=0||p.moveY!=0){pad.sThumbLX=static_cast<SHORT>(p.moveX*32767);pad.sThumbLY=static_cast<SHORT>(p.moveY*32767);}
}
class MotionInputChannel {
    HANDLE mapping_{},mutex_{};void* memory_{};bool writer_{};
public:
    ~MotionInputChannel(){if(writer_)publish({});if(memory_)UnmapViewOfFile(memory_);if(mapping_)CloseHandle(mapping_);if(mutex_)CloseHandle(mutex_);}
    bool open(bool writer){
        if(memory_)return writer==writer_;
        writer_=writer;
        constexpr auto name=L"Local\\AmalurMotionInputV2",lock=L"Local\\AmalurMotionInputMutexV2";
        mapping_=writer?CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(MotionInputPacket),name):OpenFileMappingW(FILE_MAP_READ,FALSE,name);
        mutex_=writer?CreateMutexW(nullptr,FALSE,lock):OpenMutexW(SYNCHRONIZE|MUTEX_MODIFY_STATE,FALSE,lock);
        if(mapping_&&mutex_)memory_=MapViewOfFile(mapping_,writer?FILE_MAP_WRITE:FILE_MAP_READ,0,0,sizeof(MotionInputPacket));
        if(!memory_){if(mapping_)CloseHandle(mapping_);if(mutex_)CloseHandle(mutex_);mapping_=mutex_=nullptr;}
        return memory_!=nullptr;
    }
    void publish(MotionInputPacket p){
        if(!writer_||!memory_)return;
        DWORD w=WaitForSingleObject(mutex_,0);if(w!=WAIT_OBJECT_0&&w!=WAIT_ABANDONED)return;
        p.tick=GetTickCount64();memcpy(memory_,&p,sizeof(p));ReleaseMutex(mutex_);
    }
    bool read(MotionInputPacket& p){
        if(!memory_)return false;
        DWORD w=WaitForSingleObject(mutex_,0);if(w!=WAIT_OBJECT_0&&w!=WAIT_ABANDONED)return false;
        memcpy(&p,memory_,sizeof(p));ReleaseMutex(mutex_);return validMotionInput(p,GetTickCount64());
    }
};
}
