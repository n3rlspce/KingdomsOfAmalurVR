#pragma once
#include "heavy_charge_input.hpp"
#include "tracking_snapshot.hpp"
#include "motion_input.hpp"
namespace amalur {
inline HeavyChargePacket heavyChargeFrame(unsigned mode,unsigned session,const TrackingSnapshot& frame,
    const TouchInput& touch,const MotionInputPacket& mapped,bool gameplay,bool captured){
    HeavyChargePacket p;p.mode=mode;p.session=session;p.tick=frame.head.tick;p.grip=touch.rightGrip;
    p.active=gameplay&&mapped.active&&!captured&&frame.head.valid&&frame.right.valid
        &&frame.head.gameMode==1&&frame.right.gameMode==1&&frame.head.tick==frame.right.tick
        &&frame.head.recenter==frame.right.recenter;
    p.spell=mapped.abilities>.65f&&(mapped.buttons&(XINPUT_GAMEPAD_A|XINPUT_GAMEPAD_B|XINPUT_GAMEPAD_X|XINPUT_GAMEPAD_Y));
    return p;
}
}
