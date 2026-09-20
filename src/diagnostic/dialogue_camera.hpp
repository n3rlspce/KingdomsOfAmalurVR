#pragma once
#include "../tracking/dialogue_view.hpp"

namespace dialogue_camera {
struct State {
    uintptr_t dialog{},core{},playerCore{};uint32_t owner{};
    mgs5vr::Vec3 position{},facing{};
};
// Read native ownership; never call Lua wrappers or suppress dialogue updates.
// DialogMgr's active PlayerDialog is also checked by getters at 9CEE70/9CEEB0.
static bool sample(State& out){
    __try {
        const auto word=player_rig::word;
        const auto manager=word(gameBase+0x15fec38);if(!manager)return false;
        const auto dialogs=manager+0x2df8;
        if(word(dialogs)!=gameBase+0x134e074)return false;
        const auto dialog=word(dialogs+0x148);
        if(!dialog||word(dialog)!=gameBase+0x1343210||!word(dialog+0x5c)||!word(dialog+0x60)||
           (*reinterpret_cast<const unsigned char*>(dialog+0x6c)&0x11))return false;
        const auto globals=word(gameBase+0x15fe9c4);if(!globals)return false;
        const auto windows=globals+0x397c;
        if(word(windows)!=gameBase+0x134e60c||word(windows+8)!=1)return false;
        const auto entries=word(windows+4);if(!entries)return false;
        const auto game=word(entries+4);if(!game)return false;
        if(word(game)!=gameBase+0x1328914&&word(game)!=gameBase+0x132ae7c)return false;
        const auto scene=word(game+0x40c);
        if(!scene||word(scene)!=gameBase+0x1326e9c)return false;
        const auto camera=word(scene+0x410);
        if(!camera||word(camera)!=gameBase+0x1335d08||word(camera+8)!=1)return false;
        const auto p=reinterpret_cast<uintptr_t>(player_rig::player.load());
        if(!p||(word(p)!=gameBase+0x1359f14&&word(p)!=gameBase+0x1359e94))return false;
        const auto playerCamera=word(p+0x108);
        if(!playerCamera||word(playerCamera)!=gameBase+0x1335d08||camera==playerCamera)return false;
        State s;s.dialog=dialog;s.core=camera+8;s.playerCore=playerCamera+8;
        if(!player_rig::location(reinterpret_cast<void*>(s.playerCore),s.position))return false;
        s.owner=static_cast<uint32_t>(word(p+0x1ec));
        const auto entity=player_rig::resolve(s.owner),loc=player_rig::part(entity,6,s.owner,0x1355cdc);
        if(!loc)return false;
        const double angle=static_cast<uint32_t>(word(loc+0xb0))*(6.283185307179586/4294967296.0);
        s.facing={static_cast<float>(std::cos(angle)),static_cast<float>(std::sin(angle)),0};
        if(word(dialogs+0x148)!=dialog||word(scene+0x410)!=camera||word(p+0x108)!=playerCamera||word(p+0x1ec)!=s.owner)return false;
        out=s;return true;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static amalur::DialogueView view;
static amalur::CameraInputs inputs;
static bool wasActive{},resumeGameplay{};
static uint32_t inputOwner{};
static uintptr_t inputDialog{};
static uint64_t lastOpen{};

// Called before the gameplay camera's FOV/identity gates. Return true when the
// original rebuild has already been called, including non-scene cameras while
// the dialogue is active. UI and dialogue logic never pass through this hook.
static bool rebuild(void* camera){
    auto core=static_cast<unsigned char*>(camera);
    State s;const bool dialogue=sample(s);
    motion_controls::dialogueActive.store(dialogue);
    const bool active=dialogue&&firstPerson.load()&&headTracking.load();
    if(inputs.core==core){
        // The same address alone is not a lifetime guarantee.
        if(dialogue&&inputOwner==s.owner&&inputDialog==s.dialog){
            __try {inputs.restore();} __except(EXCEPTION_EXECUTE_HANDLER){inputs.core=nullptr;}
        }else inputs.core=nullptr;
    }
    if(!active){
        if(wasActive){resumeGameplay=true;view.reset();inputs.core=nullptr;log("First-person dialogue inactive\n");}
        wasActive=false;return false;
    }
    if(!wasActive){log("First-person dialogue active core=%p playerCore=%p\n",reinterpret_cast<void*>(s.core),reinterpret_cast<void*>(s.playerCore));}
    wasActive=true;
    motion_controls::sampleMovementBasis({}, {},false,GetTickCount64());
    if(reinterpret_cast<uintptr_t>(camera)!=s.core){realRebuildCamera(camera);return true;}
    const auto now=GetTickCount64();
    if(now-lastOpen>1000){poseChannel.open(false);lastOpen=now;}
    amalur::PosePacket packet{};poseChannel.read(packet);
    amalur::CameraPose native{},adjusted{};
    memcpy(&native.eye,core+4,12);memcpy(&native.target,core+0x14,12);memcpy(&native.up,core+0x1c0,12);
    const float fov=*reinterpret_cast<float*>(core+0x2c);
    const mgs5vr::Pose head{{packet.orientation[0],packet.orientation[1],packet.orientation[2],packet.orientation[3]},
        {packet.position[0],packet.position[1],packet.position[2]}};
    const bool tracked=packet.version==3&&packet.valid&&packet.gameMode==1&&packet.tick<=now&&now-packet.tick<250&&
        std::isfinite(packet.horizontalFov)&&packet.horizontalFov>50&&packet.horizontalFov<179&&
        view.apply(s.dialog,s.owner,recenterGeneration.load(),packet.recenter,s.position,s.facing,head,packet.worldScale,adjusted);
    if(tracked){
        memcpy(core+4,&adjusted.eye,12);memcpy(core+0x14,&adjusted.target,12);memcpy(core+0x1c0,&adjusted.up,12);
        *reinterpret_cast<float*>(core+0x2c)=packet.horizontalFov;
    }
    // Dialogue's native controller may cache its narrow lens. Always rebuild
    // when applying or withdrawing the override, even for equal headset ticks.
    core[0x35e]|=1;realRebuildCamera(camera);
    packet.valid=tracked?1:0;
    packet.projectionX=*reinterpret_cast<float*>(core+0xc4);packet.projectionY=*reinterpret_cast<float*>(core+0xd8);
    packet.stereoStatus=stereoStatus.load();
    if(frameChannel.open(true))frameChannel.publish(packet);
    render_pose::camera(core,packet);trackedCameraAvailable.store(tracked);
    camera_status::publish(packet,tracked,true,s.position,native,tracked?adjusted:native,core);
    if(tracked&&coherentCamera.load()){
        inputs={core,native,adjusted,fov,packet.horizontalFov};inputOwner=s.owner;inputDialog=s.dialog;
    }else{
        memcpy(core+4,&native.eye,12);memcpy(core+0x14,&native.target,12);memcpy(core+0x1c0,&native.up,12);
        *reinterpret_cast<float*>(core+0x2c)=fov;
    }
    return true;
}
}
