#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <string>
#include <cstdio>
#include <cstdlib>
static const wchar_t* testMapping(){static std::wstring name=L"Local\\AmalurRigAblationTest-"+std::to_wstring(GetCurrentProcessId());return name.c_str();}
#define AMALUR_BODY_DEBUG_MAPPING testMapping()
#include "src/xr_smoke/vr_settings.hpp"
void check(bool b,const char* m){if(!b){printf("FAIL: %s\n",m);exit(1);}}
int main(){
 VrSettings s;s.developerVisible=true;
 amalur::BodyDebugSettings observer;
 check(amalur::bodyDebug.read()==0,"isolated default off");
 const LONG preserved=amalur::nativeTorso|amalur::nativeArms|amalur::nativeCamera;
 amalur::bodyDebug.toggle(preserved);
 for(int i=0;i<5;++i){
  s.developerRow=VrSettings::ablationFirstRow+i;
  s.held[VK_RETURN]=true;s.poll();
  check(observer.read()==(preserved|VrSettings::ablationBits[i]),"keyboard row toggles only its own shared bit");
  s.held[VK_RETURN]=false;s.poll();
  check(s.ablationAction(s.developerRow,true),"controller action uses same row dispatch");
  check(observer.read()==preserved,"controller dispatch reverses only selected bit");
 }
 for(int mask=0;mask<32;++mask){
  amalur::bodyDebug.resetAblations();
  for(int i=0;i<5;++i)if(mask&(1<<i))s.ablationAction(VrSettings::ablationFirstRow+i,true);
  LONG wanted=preserved;for(int i=0;i<5;++i)if(mask&(1<<i))wanted|=VrSettings::ablationBits[i];
  check(observer.read()==wanted,"all 32 independent combinations");
  s.ablationAction(VrSettings::ablationResetRow,true);
  check(observer.read()==preserved,"reset retains native camera/animation modes");
 }
 check(!s.ablationAction(VrSettings::ablationFirstRow-1,true)&&!s.ablationAction(VrSettings::ablationResetRow+1,true),"other rows excluded");
 check(s.developerAction==-1,"experiments never dispatch gameplay commands");
 check(220+(VrSettings::developerPanelRows-1)*29+28<1045,"all rows fit above footer");
 check(VrSettings::mapPanelRow==VrSettings::developerPanelRows,"hidden prototype remains hidden");
 puts("PASS: isolated shared IPC, independent keyboard/controller rows, all combinations, reset and panel bounds");
}
