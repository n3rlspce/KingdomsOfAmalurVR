#include "held_weapon_profile.hpp"
#include "weapon_contact_profile.hpp"
#include "melee_family_policy.hpp"
#include "melee_vfx_selection.hpp"
#include <cstdio>
#include <cstdlib>
using namespace amalur;
static void check(bool ok,const char* message){if(!ok){std::fprintf(stderr,"FAIL %s\n",message);std::exit(1);}}
int main(){
 check(knownHammerModel(1323)&&knownHammerModel(5215)&&!knownHammerModel(5214),"exact hammer models");
 const uint32_t ids[]{0xae838d,0x6666f1,0x1947c74,0x17311f,0x858053};
 const int16_t parents[]{-1,0,1,1,1};
 check(capturedHeldWeapon(5215,5,ids,parents)==HeldWeaponKind::Hammer,"recorded Reckoning skeleton");
 auto badIds=std::array<uint32_t,5>{ids[0],ids[1],ids[2],ids[3],ids[4]};badIds[2]++;
 check(capturedHeldWeapon(5215,5,badIds.data(),parents)==HeldWeaponKind::None,"same-count wrong skeleton rejected");
 const int16_t wrongParents[]{-1,0,1,2,1};
 check(capturedHeldWeapon(5215,5,ids,wrongParents)==HeldWeaponKind::None,"wrong skeleton topology rejected");
 check(capturedHeldWeapon(5215,4,ids,parents)==HeldWeaponKind::None,"wrong skeleton count rejected");
 check(chooseHeldWeaponSlot(HeldWeaponKind::Hammer,8,true,false)==5,"native hidden slot remaps to tracked right hand");
 check(chooseHeldWeaponSlot(HeldWeaponKind::Hammer,8,false,true)==8,"left-only tracking cannot draw hammer");
 check(chooseHeldWeaponSlot(HeldWeaponKind::Hammer,9,true,true)==9,"other native slots preserved");
 const auto contact=capturedContactProfile(5215);check(contact&&contact->count==1,"dedicated head contact exists");
 const auto basic=physicalMeleeRecipe(5215);check(basic.model==5215&&basic.attack==16&&basic.flags==1&&basic.hands==1,"physical recipe preserves exact skin");
 for(unsigned step=1;step<=3;++step){
  const auto r=nativeFamilyRecipe(5215,step);const auto normal=nativeFamilyRecipe(1323,step);
  check(r.model==5215&&r.attack==15+step&&r.flags==(step==3?2u:1u)&&r.step==step,"native chain preserves exact model and recorded phases");
  check(supportedNativeFamilyAttack(5215,r.attack,r.flags)&&!supportedNativeFamilyAttack(5215,r.attack,r.flags+1),"strict attack flags");
  const auto sounds=nativeSwingSounds(5215,r.attack,r.flags),oldSounds=nativeSwingSounds(1323,normal.attack,normal.flags);
  check(sounds.motion&&sounds.motion==oldSounds.motion&&sounds.voice==oldSounds.voice,"hammer sound selection shared by verified family");
  const auto fx=meleeVfxSelection(5215,r.attack,r.flags,false,0);
  check(fx.selector==0x32dcd4&&fx.attachment==0x858053,"right-hand hammer trail selector and attachment");
  check(!meleeVfxSelection(5215,r.attack,r.flags,false,1).selector&&!meleeVfxSelection(5215,r.attack,r.flags,true,0).selector,"unsupported left/heavy trails closed");
 }
 MeleeSwingEvent event{};event.weapon=9;event.asset=5215;event.serial=7;event.generation=3;event.hand=0;
 check(assignBasicStrokeRecipe(event),"basic stroke admitted");
 check(sameMeleeContact(event,event,9,5215,3,0,100,100,16,1),"exact Reckoning contact admitted");
 auto wrong=event;wrong.asset=1323;
 check(!sameMeleeContact(event,wrong,9,5215,3,0,100,100,16,1),"ordinary model cannot impersonate Reckoning publication");
 check(!sameMeleeContact(wrong,event,9,1323,3,0,100,100,16,1),"Reckoning model cannot impersonate ordinary publication");
 NativeFamilyChain chain;
 check(chain.commit(9,1323,3,100).step==1&&chain.commit(9,1323,3,150).step==2,"ordinary chain advances");
 check(chain.commit(9,5215,3,200).step==1&&chain.commit(9,5215,3,250).step==2,"same owner model transition resets chain");
 check(chain.commit(9,1323,3,300).step==1,"leaving Reckoning resets chain");
 check(!meleeContactSpeed(5215,{0,0,2},{0,0,1},10,100),"hammer head never gets blade stab discount");
 check(meleeContactSpeed(5215,{5,0,0},{0,0,1},10,100),"qualified hammer swing contact");
 check(currentPhysicalPublication(9,5215,1,1,100,110)&&!currentPhysicalPublication(9,5215,1,0,100,110),"publication current selection still required");
 puts("PASS Reckoning hammer exact identity, tracked mapping, contact recipe, combos, feedback and head-speed policy");
}
