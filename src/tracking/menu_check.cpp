#define NOMINMAX
#include "menu_view.hpp"
#include "menu_rotation.hpp"
#include "../xr_smoke/vr_settings.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
static void check(bool ok,const char* msg){if(!ok){std::fprintf(stderr,"FAIL: %s\n",msg);std::exit(1);}}
int main(){
    using namespace amalur;
    check(fullscreenMenu(1,false),"paused non-dialogue interface is classified as menu");
    check(!fullscreenMenu(1,true),"dialogue remains first person even when time stopped");
    check(!fullscreenMenu(-1,false)&&!fullscreenMenu(0,false),"unknown state and gameplay not forced into menu");
    PosePacket pose;pose.valid=1;pose.tick=123;pose.gameMode=1;
    presentAsMenu(pose,false);check(pose.valid==1&&pose.tick==123,"gameplay attribution preserved");
    presentAsMenu(pose,true);check(pose.valid==1&&pose.gameMode==4&&pose.tick==123,"menu preserves the tracked image and pose epoch");
    pose.valid=0;presentAsMenu(pose,true);check(pose.valid==0,"untracked menus are never assigned a fabricated camera");
    check(menuScale(.65f)==.65f&&menuScale(1.2f)==1.2f,"common menu size supports shrinking and enlarging");
    check(menuScale(std::numeric_limits<float>::quiet_NaN())==1&&menuScale(0)==.5f,"invalid menu scale bounded");
    MenuRotation rotation;PosePacket camera;camera.valid=1;camera.projectionX=1;camera.projectionY=1;
    rotation.update(camera);check(std::abs(rotation.matrix[0]-1)<1e-5f,"menu anchor starts with identity placement");
    camera.orientation[1]=std::sin(.25f);camera.orientation[3]=std::cos(.25f);rotation.update(camera);
    check(std::abs(rotation.matrix[2]-std::sin(.5f))<1e-5f&&std::abs(rotation.matrix[10]-std::cos(.5f))<1e-5f,"menu remains at opening heading under head yaw");
    camera.recenter++;rotation.update(camera);check(std::abs(rotation.matrix[2])<1e-5f,"recenter places menu at current heading");
    rotation.reset();check(!rotation.active,"menu closing clears anchor");
    VrSettings settings;settings.path+=L".menu-test";settings.visible=true;settings.selected=13;settings.interfaceScale=1;
    settings.held[VK_LEFT]=true;settings.poll();check(std::abs(settings.interfaceScale-.95f)<1e-5f,"fullscreen menu row changes shared size");
    settings.held[VK_LEFT]=false;settings.poll();settings.interfaceScale=1;settings.load();
    check(std::abs(settings.interfaceScale-.95f)<1e-5f,"menu size persists");
    DeleteFileW(settings.path.c_str());
    std::puts("PASS: automatic menu/first-person-dialogue separation, paired metadata and shared menu scaling persistence");
}
