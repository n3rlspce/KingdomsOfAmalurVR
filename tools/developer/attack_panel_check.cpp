#define NOMINMAX
#define AMALUR_ATTACK_PANEL_MAPPING L"Local\\AmalurAttackPanelOfflineTest"
#include "../../src/xr_smoke/vr_settings.hpp"
#include <cassert>
int main(){
 VrSettings s;amalur::AttackPanelSettings renderer;assert(renderer.enabled());
 s.developerVisible=true;s.developerRow=VrSettings::attackPanelRow;
 amalur::TouchInput t;s.pollDeveloperControllers(t,true,false,0);s.pollDeveloperControllers(t,true,false,1);
 t.a=true;s.pollDeveloperControllers(t,true,false,2);assert(!renderer.enabled());
 s.pollDeveloperControllers(t,true,false,3);assert(!renderer.enabled());
 t.a=false;s.pollDeveloperControllers(t,true,false,4);t.a=true;s.pollDeveloperControllers(t,true,false,5);assert(renderer.enabled());
 s.held[VK_RETURN]=true;s.poll();assert(!renderer.enabled());s.poll();assert(!renderer.enabled());
 s.held[VK_RETURN]=false;s.poll();s.held[VK_RETURN]=true;s.poll();assert(renderer.enabled());
}
