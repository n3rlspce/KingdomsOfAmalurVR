#include "physical_melee_recipe.hpp"
#include "melee_native_family.hpp"
#include "weapon_contact_profile.hpp"
#include "melee_fake_access.hpp"
#include "melee_owned_backend.hpp"
struct RecipeAccess:FakeAccess {
 uint32_t expected{};
 uint32_t create(uintptr_t p,uint32_t asset,uint32_t owner,uint32_t target){
  ++mutations;check(p==pool&&asset==expected&&owner==7&&target==0,"mapped recipe reaches native allocation");
  if(failCreate)return 0;
  memory[table+4]=runtime;memory[runtime+4]=asset;memory[runtime+0x1c]=1;memory[runtime+0x20]=1;memory[runtime+0x24]=owner;return 1;
 }
};
struct RecipeEnvironment {
 using Access=RecipeAccess;Access calls;uint32_t model;uint64_t epoch=1;bool broken{},released{};
 explicit RecipeEnvironment(uint32_t m):model(m){calls.expected=amalur::nativeFamilyAttack(model).attack;}
 bool supported(const amalur::MeleeContextRecipe& r)const{return !broken&&r.owner==7&&r.equipmentGeneration==epoch&&r.baseAsset==model&&amalur::nativeFamilyAttack(model).attack!=0;}
 uintptr_t part()const{return Access::part;}uintptr_t pool()const{return Access::pool;}uint32_t target()const{return 0;}uint32_t nextKey()const{return 12;}
 bool current(uintptr_t a,uint64_t g)const{return a==Access::runtime&&g==1;}
 bool release(uintptr_t a,uint64_t g,uint32_t index,uint32_t asset,uint32_t owner){
  check(a==Access::runtime&&g==1&&index==1&&asset==calls.expected&&owner==7,"owned matching recipe released");
  check(calls.memory[Access::records+0x118]==0xffffffffu,"unbind precedes release");
  check(!released,"one retirement");released=true;calls.memory[a+0x1c]=0;return true;
 }
 void fault(const char*){broken=true;}
};
int main(){using namespace amalur;
 for(uint32_t attack:{199u,200u,201u,202u,417u,418u,419u,420u,16u,17u,18u}){
  const auto r=nativeFamilyAttack(attack);check(supportedNativeFamilyAttack(r.model,attack,r.flags),"exact resolver flags accepted");
  check(!supportedNativeFamilyAttack(r.model,attack,r.flags^1),"wrong resolver flags denied");
  for(unsigned iteration=0;iteration<100;++iteration){RecipeEnvironment e(attack);MeleeOwnedBackend<RecipeEnvironment> backend(e);MeleeContextScope<decltype(backend)> scope(backend);
   check(scope.open({7,attack,attack,1})==MeleeScopeResult::Ready&&scope.current(),"direct or scripted recipe creates owned transaction");
   if(iteration%2){++e.epoch;check(!scope.current(),"equipment change invalidates runtime");}
   check(scope.close()&&scope.close()&&e.released&&!e.broken,"idempotent retirement");
   check(e.calls.memory[RecipeAccess::part+0x28]==1&&e.calls.memory[RecipeAccess::records+0x100]==11,"native unrelated key preserved");
  }
  if(!r.scripted){DirectWeaponDefinition d{0,1,1,0,0,0,0,r.field208,0x01000104};check(matchesFamilyDefinition(attack,d),"exact captured direct schema");
   auto bad=d;bad.listenerCount=1;check(!matchesFamilyDefinition(attack,bad),"unexpected script listener denied");
   bad=d;bad.script=1077;check(!matchesFamilyDefinition(attack,bad),"unexpected script denied");
   bad=d;bad.field1f8=1;check(!matchesFamilyDefinition(attack,bad),"extra native modifiers denied");
   bad=d;bad.field208^=0x2000;check(!matchesFamilyDefinition(attack,bad),"other definition variant denied");
  }
 }
 for(auto a:{421u,1519u,1272u,1320u,1441u,1437u,2015u,0u})check(!nativeFamilyAttack(a).attack,"unproven and unrelated routes denied");
 puts("PASS exact family definitions and1100 owned lifecycle transactions; complex heavies excluded");
}
