// Bounded desktop replay: emits machine-readable feedback, no screenshot polling.
// The XR bridge must be stopped; this owns the same head/hand/input channels.
#include "pose_channel.hpp"
#include "motion_input.hpp"
#include "rig_status.hpp"
#include <tlhelp32.h>
#include <cstdio>
#include <cmath>
static bool bridgeRunning(){
    HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);if(snapshot==INVALID_HANDLE_VALUE)return true;
    PROCESSENTRY32W entry{};entry.dwSize=sizeof(entry);bool found=false;
    if(Process32FirstW(snapshot,&entry))do{if(!_wcsicmp(entry.szExeFile,L"amalur-xr-smoke.exe"))found=true;}while(Process32NextW(snapshot,&entry));
    CloseHandle(snapshot);return found;
}
int main(int argc,char** argv){
    bool statusOnly=argc>1&&!strcmp(argv[1],"--status");
    bool observe=argc>1&&!strcmp(argv[1],"--observe");
    bool preview=argc>1&&!strcmp(argv[1],"--static");
    bool cameraSweep=argc>1&&!strcmp(argv[1],"--camera-sweep");
    bool turnSweep=argc>1&&!strcmp(argv[1],"--turn-sweep");
    bool interfaceCheck=argc>1&&!strcmp(argv[1],"--interface-check");
    bool meleeCheck=argc>1&&!strcmp(argv[1],"--melee-check");
    bool mouseAttack=argc>1&&!strcmp(argv[1],"--mouse-attack");
    if(!observe&&!statusOnly&&bridgeRunning()){std::fprintf(stderr,"Stop the XR bridge before desktop replay.\n");return 2;}
    amalur::RigStatusChannel status;amalur::RigStatus state;
    if(!status.transfer(state,false)||state.version!=1||GetTickCount()-state.tick>1000){std::fprintf(stderr,"No live game telemetry.\n");return 3;}
    if(statusOnly){std::printf("{\"pid\":%u,\"frames\":%u,\"weapons\":%u,\"focused\":%u,\"tracked\":%u,\"hand\":%u,\"bone\":%u,\"paused\":%d}\n",state.pid,state.frames,state.weaponRemaps,state.focused,state.tracked,state.handFresh,state.sourceBone,state.paused);return 0;}
    if(!observe&&state.paused!=0){std::fprintf(stderr,"Gameplay is paused or its state is unknown.\n");return 7;}
    amalur::PoseChannel head,hand(L"Local\\AmalurVRRightHandV3",L"Local\\AmalurVRRightHandMutexV3");
    amalur::PoseChannel leftHand(L"Local\\AmalurVRLeftHandV3",L"Local\\AmalurVRLeftHandMutexV3");
    amalur::MotionInputChannel input;
    if(!observe&&(!state.focused||!state.weaponRemaps||!head.open(true)||!hand.open(true)||!leftHand.open(true)||!input.open(true))){std::fprintf(stderr,"Load gameplay with the equipped weapon and focus the game first.\n");return 4;}
    struct MouseRelease {bool down{};~MouseRelease(){if(down)mouse_event(MOUSEEVENTF_LEFTUP,0,0,0,0);}} mouse;
    auto start=GetTickCount64(),lastPrint=uint64_t{};auto initial=state.weaponRemaps;auto initialSlot=state.nativeWeaponSlot;bool drewWeapon=false;
    if(turnSweep)std::printf("{\"test\":\"turn-sweep-v1\",\"startTick\":%llu,\"durationMs\":18500}\n",start);
    std::puts(meleeCheck?"{\"test\":\"dagger-attachment-v1\",\"durationMs\":8000,\"attacks\":false}":cameraSweep?"{\"test\":\"camera-sweep-v1\",\"durationMs\":4000,\"maxAngleDegrees\":20}":"{\"test\":\"rig-replay-v1\",\"units\":\"game units\"}");
    while(GetTickCount64()-start<(preview?600000:meleeCheck?8000:turnSweep?18500:interfaceCheck?9000:cameraSweep?4000:2500)){
        auto now=GetTickCount64();float t=static_cast<float>(now-start)/1000.f;
        if(!status.transfer(state,false)){Sleep(8);continue;}
        if(GetTickCount()-state.tick>1000||(!observe&&!preview&&!state.focused)){std::fprintf(stderr,"Stopped: game telemetry stale or focus lost.\n");return 5;}
        if((cameraSweep||turnSweep)&&state.paused!=0){std::fprintf(stderr,"Stopped: gameplay paused or state unavailable.\n");return 7;}
        if(initialSlot==8&&state.nativeWeaponSlot==5)drewWeapon=true;
        const char* phase=preview?"preview":meleeCheck?(t<2?"hands_static":t<6?"hands_sweep":"hands_return"):turnSweep?"turn_sweep":cameraSweep?"camera_sweep":t<.3f?"idle":t<.7f?"attack":t<2?"hand_sweep":"recover";
        if(!observe){
            amalur::PosePacket p;p.valid=1;p.gameMode=(preview||meleeCheck)?2:1;p.recenter=(preview||meleeCheck)?1000:0;p.tick=now;p.position[1]=1.7f;
            if(interfaceCheck&&t>=3&&t<6)p.gameMode=3;
            if(cameraSweep){
                p.recenter=1001;
                // qYaw * qPitch: independent bounded physical head axes.
                const float halfYaw=.1745329252f*std::sin(t*3.141592654f);
                const float halfPitch=.1745329252f*std::sin(t*4.712388980f);
                const float sy=std::sin(halfYaw),cy=std::cos(halfYaw),sp=std::sin(halfPitch),cp=std::cos(halfPitch);
                p.orientation[0]=cy*sp;p.orientation[1]=sy*cp;p.orientation[2]=-sy*sp;p.orientation[3]=cy*cp;
            }
            if(turnSweep){
                p.recenter=1002;
                // Full physical turn, then short forward probes at 360, 450,
                // 540 and 630 degrees. No attacks or menu input.
                const float yaw=t<6?std::fmax(0.f,t-1)*6.283185307f/5.f:
                    6.283185307f+float(int((t-6)/2.5f))*1.570796327f;
                p.orientation[1]=std::sin(yaw*.5f);p.orientation[3]=std::cos(yaw*.5f);
            }
            head.publish(p);p.position[0]=.25f;p.position[1]=1.3f;p.position[2]=-.35f;
            if(cameraSweep){p.orientation[0]=p.orientation[1]=p.orientation[2]=0;p.orientation[3]=1;}
            if(preview||meleeCheck){p.position[0]=.65f;p.position[1]=1.3f;p.position[2]=0;}
            if(meleeCheck&&t>=2&&t<6){p.position[1]+=.2f*std::sin((t-2)*3.14159265f);p.orientation[2]=std::sin(.45f*std::sin((t-2)*3.14159265f));p.orientation[3]=std::sqrt(1-p.orientation[2]*p.orientation[2]);}
            if(!preview&&!meleeCheck&&!cameraSweep&&!turnSweep&&!interfaceCheck&&t>=.7f&&t<2){p.position[0]+=.15f*std::sin((t-.7f)*4);p.orientation[1]=std::sin(.4f*std::sin(t));p.orientation[3]=std::sqrt(1-p.orientation[1]*p.orientation[1]);}
            hand.publish(p);amalur::MotionInputPacket m;m.active=1;
            if(preview||meleeCheck){p.position[0]=-.65f;p.position[1]=1.15f;p.orientation[2]=-p.orientation[2];leftHand.publish(p);}
            if(mouseAttack){bool down=t>=.3f&&t<.65f;if(down!=mouse.down){mouse_event(down?MOUSEEVENTF_LEFTDOWN:MOUSEEVENTF_LEFTUP,0,0,0,0);mouse.down=down;}}
            else if(!preview&&!meleeCheck&&!cameraSweep&&!turnSweep&&!interfaceCheck&&t>=.3f&&t<.65f)m.buttons=XINPUT_GAMEPAD_X;
            if(turnSweep&&t>=6){const float phase=std::fmod(t-6,2.5f);if(phase>=.8f&&phase<1.5f)m.moveY=.65f;}
            input.publish(m);
        }
        if(now-lastPrint>=50){
            std::printf("{\"ms\":%llu,\"phase\":\"%s\",\"frames\":%u,\"remaps\":%u,\"weapons\":%u,\"tracked\":%u,\"hand\":%u,\"slot\":%u,\"bone\":%u,\"nativeSlot\":%u,\"wrist\":[%.3f,%.3f,%.3f],\"socket\":[%.3f,%.3f,%.3f],\"rendered\":[%.3f,%.3f,%.3f]}\n",now-start,phase,state.frames,state.remaps,state.weaponRemaps,state.tracked,state.handFresh,state.weaponSlot,state.sourceBone,state.nativeWeaponSlot,state.nativeWrist[0],state.nativeWrist[1],state.nativeWrist[2],state.nativeSocket[0],state.nativeSocket[1],state.nativeSocket[2],state.renderedSocket[0],state.renderedSocket[1],state.renderedSocket[2]);
            lastPrint=now;
        }
        Sleep(8);
    }
    if(!observe){head.publish({});hand.publish({});leftHand.publish({});input.publish({});}
    std::printf("{\"captureComplete\":true,\"nativeDrawObserved\":%s}\n",drewWeapon?"true":"false");
    return state.weaponRemaps>initial?0:6;
}
