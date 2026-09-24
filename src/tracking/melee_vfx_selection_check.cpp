#include "melee_vfx_selection.hpp"
#include <cassert>
#include <cstdio>
int main(){
 using namespace amalur;
 for(auto model:{2478u,5457u}){
  auto h=meleeVfxSelection(model,81,1,true,0);assert(h.selector==0x32dcd4&&h.attachment==0x6666f1&&h.durationMs==400);
  assert(!meleeVfxSelection(model,81,0,true,0).selector);
  assert(!meleeVfxSelection(model,81,1,false,0).selector);
  assert(!meleeVfxSelection(model,81,1,true,1).selector);
  assert(!meleeVfxSelection(model,50,0,true,0).selector);
  auto n=meleeVfxSelection(model,50,0,false,0);assert(n.attachment==0x858053&&n.durationMs==200);
 }
 for(auto model:{1520u,1250u,1323u})for(unsigned step=1;step<=4;++step){
  auto r=nativeFamilyRecipe(model,step);if(!r.attack)continue;
  assert(meleeVfxSelection(model,r.attack,r.flags,false,0).selector);
  assert(!meleeVfxSelection(model,r.attack,r.flags,true,0).selector);
  auto left=meleeVfxSelection(model,r.attack,r.flags,false,1);
  if(model==1520)assert(left.selector==0x32dcd4&&left.attachment==0xceac76&&left.durationMs==160);
  else assert(!left.selector);
 }
 assert(!meleeVfxSelection(1514,81,1,true,0).selector);
 MeleeVfxPoseIdentity pose;MeleeSwingEvent e{};e.owner=1;e.weapon=2;e.asset=5457;e.generation=3;e.attackAsset=81;e.attackFlags=1;e.heavy=true;
 pose.committed(e);auto p=pose.current(1,2,5457,3,120);assert(p.tick==120&&meleeVfxSelection(p).attachment==0x6666f1);
 assert(!meleeVfxSelection(pose.current(1,3,5457,3,120)).selector);
 assert(!meleeVfxSelection(pose.current(1,2,5457,4,120)).selector);
 pose.clear();assert(!meleeVfxSelection(pose.current(1,2,5457,3,120)).selector);
 puts("melee_vfx_selection_check PASS");
}
