#pragma once
#include <windows.h>
#include <Xinput.h>
#include <cmath>
#include <cstdint>
#include <cstring>
#include "play_mode.hpp"
#include <initializer_list>

namespace amalur {
struct MotionInputPacket {
    uint32_t version{5},active{};
    uint64_t tick{};
    float moveX{},moveY{},lookX{},lookY{};
    uint32_t buttons{};
    float block{},abilities{},supportGrip{};
    uint32_t selectedWeapon{};float turnYawDegrees{};uint32_t session{GetCurrentProcessId()};
};
inline bool validMotionInput(const MotionInputPacket& p,uint64_t now){
    return p.version==5&&p.active==1&&p.tick<=now&&now-p.tick<250
        &&std::isfinite(p.lookX)&&std::isfinite(p.lookY)&&std::abs(p.lookX)<=1&&std::abs(p.lookY)<=1
        &&std::isfinite(p.moveX)&&std::isfinite(p.moveY)
        &&std::abs(p.moveX)<=1&&std::abs(p.moveY)<=1
        &&!(p.buttons&~0xf3ffu)&&std::isfinite(p.block)&&std::isfinite(p.abilities)
        &&std::isfinite(p.supportGrip)&&p.supportGrip>=0&&p.supportGrip<=1
        &&p.block>=0&&p.block<=1&&p.abilities>=0&&p.abilities<=1
        &&p.selectedWeapon<=1&&p.session!=0&&std::isfinite(p.turnYawDegrees);
}
inline void deadzone(float& x,float& y){
    float n=std::sqrt(x*x+y*y);
    if(!std::isfinite(n)||n<=.2f){x=y=0;return;}
    float magnitude=(std::fmin(n,1.f)-.2f)/.8f;
    x=x/n*magnitude;y=y/n*magnitude;
}
// Gameplay Y selects a weapon; RT attacks it. Right grip retains native spell slots; left grip is reserved for grabbing.
// The thumb-rest (or both stick clicks) shifts the left stick to the D-pad.
struct TouchInput {
    float leftX{},leftY{},rightX{},rightY{},leftTrigger{},rightTrigger{},leftGrip{},rightGrip{};
    bool a{},b{},x{},y{},leftClick{},rightClick{},menu{},rightThumbrest{};
};
class TouchMapper {
    bool active_{},gameplay_{true},ready_{},attack_{},previousY_{},turnArmed_{true},movementBlocked_{};
    bool actionContext_{},abilityContext_{},clicksBlocked_{};float heldAbilities_{};
    uint32_t selected_{},attackOwner_{};float turn_{};
    bool previousLeftClick_{},previousRightClick_{};uint64_t mapPulseUntil_{},stealthPulseUntil_{},leftPressTick_{};bool wheelHeld_{};
    static bool neutral(TouchInput t){
        return std::abs(t.leftX)<.25f&&std::abs(t.leftY)<.25f&&std::abs(t.rightX)<.25f&&std::abs(t.rightY)<.25f
            &&t.leftTrigger<.25f&&t.rightTrigger<.25f&&t.leftGrip<.25f&&t.rightGrip<.25f
            &&!t.a&&!t.b&&!t.x&&!t.y&&!t.leftClick&&!t.rightClick&&!t.menu&&!t.rightThumbrest;
    }
    static bool valid(TouchInput t){
        for(float v:{t.leftX,t.leftY,t.rightX,t.rightY})if(!std::isfinite(v)||std::abs(v)>1)return false;
        for(float v:{t.leftTrigger,t.rightTrigger,t.leftGrip,t.rightGrip})if(!std::isfinite(v)||v<0||v>1)return false;
        return true;
    }
    void cancel(){ready_=attack_=previousY_=actionContext_=abilityContext_=false;heldAbilities_=0;turnArmed_=true;movementBlocked_=clicksBlocked_=previousLeftClick_=previousRightClick_=false;mapPulseUntil_=stealthPulseUntil_=leftPressTick_=0;wheelHeld_=false;}
    static void dpad(MotionInputPacket& p,float x,float y){
        if(x<-.65f)p.buttons|=XINPUT_GAMEPAD_DPAD_LEFT;
        if(x>.65f)p.buttons|=XINPUT_GAMEPAD_DPAD_RIGHT;
        if(y<-.65f)p.buttons|=XINPUT_GAMEPAD_DPAD_DOWN;
        if(y>.65f)p.buttons|=XINPUT_GAMEPAD_DPAD_UP;
    }
public:
    MotionInputPacket map(TouchInput t,bool active,bool gameplay=true,uint64_t now=GetTickCount64()){
        MotionInputPacket p;p.selectedWeapon=selected_;p.turnYawDegrees=turn_;
        if(!active||!valid(t)){cancel();active_=false;return p;}
        if(!active_||gameplay_!=gameplay){cancel();active_=true;gameplay_=gameplay;}
        p.active=1;
        if(!ready_){if(neutral(t))ready_=true;return p;}
        const bool chord=t.leftClick&&t.rightClick;
        const bool shift=gameplay&&(t.rightThumbrest||chord);
        if(chord||(shift&&(t.leftClick||t.rightClick||previousLeftClick_||previousRightClick_)))clicksBlocked_=true;
        if(t.leftClick&&!previousLeftClick_)leftPressTick_=now;
        if(gameplay&&!shift&&!clicksBlocked_&&t.leftClick&&now>=leftPressTick_&&now-leftPressTick_>=350)wheelHeld_=true;
        // Defer single-click actions until release, allowing the second stick
        // click to arrive on a later XR frame without opening Map first.
        if(!shift&&!clicksBlocked_){
            if(previousLeftClick_&&!t.leftClick&&!wheelHeld_)mapPulseUntil_=now+80;
            if(previousRightClick_&&!t.rightClick)stealthPulseUntil_=now+80;
        }else mapPulseUntil_=stealthPulseUntil_=0;
        if(!t.leftClick||shift||clicksBlocked_)wheelHeld_=false;
        previousLeftClick_=t.leftClick;previousRightClick_=t.rightClick;
        if(!t.leftClick&&!t.rightClick)clicksBlocked_=false;
        const bool leftNeutral=std::abs(t.leftX)<.25f&&std::abs(t.leftY)<.25f;
        if(shift){movementBlocked_=true;dpad(p,t.leftX,t.leftY);}
        else {
            if(leftNeutral)movementBlocked_=false;
            if(!movementBlocked_){p.moveX=t.leftX;p.moveY=t.leftY;deadzone(p.moveX,p.moveY);}
        }
        p.block=t.leftTrigger;
        const bool faces=t.a||t.b||t.x||t.y;
        if(actionContext_&&!faces&&t.rightTrigger<.45f)actionContext_=false;
        if(!actionContext_&&(faces||t.rightTrigger>=.65f)){
            actionContext_=true;abilityContext_=t.rightGrip>.65f;heldAbilities_=abilityContext_?t.rightGrip:0;
        }
        p.abilities=actionContext_?heldAbilities_:(t.rightGrip>.65f?t.rightGrip:0);
        const bool abilities=actionContext_?abilityContext_:t.rightGrip>.65f;
        const bool attack=t.rightTrigger>=(attack_?.45f:.65f);
        if(attack&&!attack_)attackOwner_=selected_;
        attack_=attack;
        if(t.a)p.buttons|=XINPUT_GAMEPAD_A;if(t.b)p.buttons|=XINPUT_GAMEPAD_B;
        if(t.x)p.buttons|=XINPUT_GAMEPAD_X;
        if(!gameplay||abilities){if(t.y)p.buttons|=XINPUT_GAMEPAD_Y;}
        else if(t.y&&!previousY_)selected_^=1;
        previousY_=t.y;
        // In the spell layer RT retains the primary face-slot equivalent. The
        // ability context is held until the whole action releases, preventing a
        // spell button becoming a weapon attack when the grip is released first.
        if(attack_)p.buttons|=gameplay&&!abilities&&attackOwner_?XINPUT_GAMEPAD_Y:XINPUT_GAMEPAD_X;
        p.supportGrip=gameplay?t.leftGrip:0;
        if(wheelHeld_)p.buttons|=XINPUT_GAMEPAD_LEFT_SHOULDER;
        // Keep release actions visible long enough for the game's slower poll.
        if(now<mapPulseUntil_)p.buttons|=XINPUT_GAMEPAD_BACK;
        if(now<stealthPulseUntil_)p.buttons|=XINPUT_GAMEPAD_RIGHT_SHOULDER;
        if(t.menu)p.buttons|=XINPUT_GAMEPAD_START;
        if(!gameplay)dpad(p,t.rightX,t.rightY);
        else if(playMode.normal()){
            p.lookX=t.rightX;p.lookY=t.rightY;deadzone(p.lookX,p.lookY);
        }else {
            if(std::abs(t.rightX)<.25f)turnArmed_=true;
            if(turnArmed_&&std::abs(t.rightX)>.7f){turn_+=t.rightX>0?30.f:-30.f;turnArmed_=false;}
        }
        p.selectedWeapon=selected_;p.turnYawDegrees=turn_;return p;
    }
};
// The bridge exposes a held right grip as native RT (the spell modifier), even
// without a spell button. In physical melee that idle modifier can enter native
// ability state while the player merely squeezes the sword. Require an explicit
// face-slot action before forwarding it. RT spells are already mapped to X by
// TouchMapper, and its latched action context survives an early grip release.
// Apply at the receiver so this also works with the installed V5 bridge packet.
inline constexpr uint32_t spellSlots=XINPUT_GAMEPAD_A|XINPUT_GAMEPAD_B|XINPUT_GAMEPAD_X|XINPUT_GAMEPAD_Y;
inline bool explicitSpellRequest(const MotionInputPacket& p){return p.abilities>.65f&&(p.buttons&spellSlots);}
inline void suppressIdleMeleeSpellModifier(MotionInputPacket& p,bool physicalMelee){
    if(physicalMelee&&!(p.buttons&spellSlots))p.abilities=0;
}
// Native ability selection gets a modifier-only preparation interval before the
// requested slot. Retain a short tap until a later game poll actually emits it;
// held slots stay held, never becoming a stream of fresh button presses.
class MeleeSpellSequence {
    uint32_t session_{},slots_{};float modifier_{};
    uint64_t preparedAt_{},deliveredAt_{},lastNow_{};
public:
    void reset(){*this=MeleeSpellSequence{};}
    bool active()const{return slots_!=0;}
    void sample(MotionInputPacket& p,uint64_t now,bool physicalMelee){
        if(!physicalMelee||!validMotionInput(p,now)){reset();return;}
        if(lastNow_&&(now<lastNow_||now-lastNow_>=250||p.session!=session_))reset();
        lastNow_=now;session_=p.session;
        const bool requested=explicitSpellRequest(p);
        if(requested){
            if(!active())preparedAt_=now;
            slots_=p.buttons&spellSlots;modifier_=p.abilities;
        }else if(deliveredAt_&&now-deliveredAt_>=80){
            slots_=0;preparedAt_=deliveredAt_=0;
        }
        if(!active()){suppressIdleMeleeSpellModifier(p,true);return;}
        p.abilities=modifier_;p.buttons&=~spellSlots;
        if(now-preparedAt_>=35){
            p.buttons|=slots_;
            if(!deliveredAt_)deliveredAt_=now;
        }
    }
};
inline void mergeMotion(XINPUT_GAMEPAD& pad,const MotionInputPacket& p){
    pad.wButtons|=static_cast<WORD>(p.buttons);
    pad.bLeftTrigger=static_cast<BYTE>(std::fmax(pad.bLeftTrigger,p.block*255));
    pad.bRightTrigger=static_cast<BYTE>(std::fmax(pad.bRightTrigger,p.abilities*255));
    // A real controller remains usable when the virtual stick is neutral.
    if(p.lookX!=0||p.lookY!=0){pad.sThumbRX=static_cast<SHORT>(p.lookX*32767);pad.sThumbRY=static_cast<SHORT>(p.lookY*32767);}
    if(p.moveX!=0||p.moveY!=0){pad.sThumbLX=static_cast<SHORT>(p.moveX*32767);pad.sThumbLY=static_cast<SHORT>(p.moveY*32767);}
}
class MotionInputChannel {
    HANDLE mapping_{},mutex_{};void* memory_{};bool writer_{};
public:
    ~MotionInputChannel(){if(writer_)publish({});if(memory_)UnmapViewOfFile(memory_);if(mapping_)CloseHandle(mapping_);if(mutex_)CloseHandle(mutex_);}
    bool open(bool writer){
        if(memory_)return writer==writer_;
        writer_=writer;
        constexpr auto name=L"Local\\AmalurMotionInputV5",lock=L"Local\\AmalurMotionInputMutexV5";
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
