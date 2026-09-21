#include "held_weapon_profile.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace amalur;
void check(bool value,const char* message){if(!value){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
int main(){
    check(trackedWeaponSelection(0,false)&&trackedWeaponSelection(0,true),"primary behavior preserved");
    check(trackedWeaponSelection(1,true)&&!trackedWeaponSelection(1,false),"secondary requires unique current attachment");
    check(!trackedWeaponSelection(2,true)&&!trackedWeaponSelection(0xffffffff,true),"invalid selected slots rejected");
    for(const auto& profile:capturedHeldWeaponProfiles){
        check(capturedHeldWeapon(profile.asset,profile.count,profile.ids,profile.parents)==profile.kind,"captured identity");
        check(capturedHeldWeapon(999999,profile.count,profile.ids,profile.parents)==HeldWeaponKind::None,"wrong asset");
        check(capturedHeldWeapon(profile.asset,profile.count-1,profile.ids,profile.parents)==HeldWeaponKind::None,"wrong count");
        uint32_t ids[7];int16_t parents[7];
        memcpy(ids,profile.ids,sizeof(ids));memcpy(parents,profile.parents,sizeof(parents));
        for(unsigned i=0;i<profile.count;++i){
            ++ids[i];check(capturedHeldWeapon(profile.asset,profile.count,ids,parents)==HeldWeaponKind::None,"wrong bone ID at same count");--ids[i];
            ++parents[i];check(capturedHeldWeapon(profile.asset,profile.count,ids,parents)==HeldWeaponKind::None,"wrong parent at same count");--parents[i];
        }
        const bool dual=profile.kind==HeldWeaponKind::Faeblades;
        check(expectedHeldSlot(profile.kind)==(dual?7u:5u),"held slot");
        for(unsigned slot=0;slot<32;++slot)for(unsigned tracking=0;tracking<4;++tracking){
            const bool right=tracking&1,left=tracking&2;
            const auto expected=slot==8&&(right||(dual&&left))?(dual?7u:5u):slot;
            check(chooseHeldWeaponSlot(profile.kind,slot,right,left)==expected,"native slots and tracking policy");
        }
    }
    // Independent recorded IDs distinguish same-count representatives.
    const uint32_t longsword[]{0xae838d,0x6666f1,0xea7b92,0x858053};const int16_t single[]{-1,0,1,1};
    check(capturedHeldWeapon(2478,4,longsword,single)==HeldWeaponKind::Longsword,"recorded longsword");
    check(capturedHeldWeapon(1514,4,longsword,single)==HeldWeaponKind::None,"staff cannot use longsword skeleton");
    const uint32_t rusty[]{0xae838d,0x6666f1,0xb1fe66,0x858053};
    check(capturedHeldWeapon(5457,4,rusty,single)==HeldWeaponKind::Longsword,"rusty exact skeleton");
    check(capturedHeldWeapon(5457,4,longsword,single)==HeldWeaponKind::None&&capturedHeldWeapon(2478,4,rusty,single)==HeldWeaponKind::None,"longsword variants cannot impersonate skeleton");
    check(knownLongswordModel(5457)&&knownLongswordModel(2478)&&!knownLongswordModel(2199),"family exact allowlist");
    const uint32_t chakrams[]{0xae838d,0x6666f1,0xdbd751,0x12d1b1d,0x863d06,0xc7b304,0x1453f90};
    const int16_t chakramParents[]{-1,0,1,2,0,4,5};
    check(capturedHeldWeapon(1877,7,chakrams,chakramParents)==HeldWeaponKind::None,"chakrams excluded");
    check(capturedHeldWeapon(2478,4,nullptr,single)==HeldWeaponKind::None&&capturedHeldWeapon(2478,4,longsword,nullptr)==HeldWeaponKind::None,"null inputs");
    check(expectedHeldSlot(HeldWeaponKind::None)==0&&chooseHeldWeaponSlot(HeldWeaponKind::None,8,true,true)==8,"unknown never remapped");
    puts("PASS: five exact captured identities, skeleton rejection, held slots and tracking/native-slot policy");
}
