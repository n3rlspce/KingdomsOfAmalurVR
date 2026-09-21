#include "physical_melee_recipe.hpp"
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
 explicit RecipeEnvironment(uint32_t m):model(m){calls.expected=amalur::physicalMeleeRecipe(model).attack;}
 bool supported(const amalur::MeleeContextRecipe& r)const{return !broken&&r.owner==7&&r.equipmentGeneration==epoch&&amalur::matchesPhysicalRecipe(model,r.baseAsset,amalur::physicalMeleeRecipe(model).flags);}
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
 for(uint32_t model:{1520u,2478u,5457u,1250u,1323u}){
  const auto recipe=physicalMeleeRecipe(model);const auto* geometry=capturedContactProfile(model);
  check(recipe.attack&&supportedPhysicalAttack(recipe.attack,recipe.flags)&&geometry&&geometry->count,"supported recipe has shared geometry");
  check(recipe.hands==(model==1520?2u:1u),"single weapons only use right hand");
  check(matchesPhysicalRecipe(model,recipe.attack,recipe.flags)&&!matchesPhysicalRecipe(model,recipe.attack,recipe.flags^1),"attack flags remain exact");
  check(!matchesPhysicalRecipe(model,model,0),"model resource cannot be attack resource");
  for(unsigned iteration=0;iteration<100;++iteration){RecipeEnvironment e(model);MeleeOwnedBackend<RecipeEnvironment> backend(e);MeleeContextScope<decltype(backend)> scope(backend);
   check(scope.open({7,recipe.attack,recipe.attack,1})==MeleeScopeResult::Ready&&scope.current(),"new recipe opens same owned transaction");
   if(iteration%2){++e.epoch;check(!scope.current(),"equipment epoch invalidates active recipe");}
   check(scope.close()&&scope.close()&&e.released&&!e.broken,"idempotent full retirement");
   check(e.calls.memory[RecipeAccess::part+0x28]==1&&e.calls.memory[RecipeAccess::records+0x100]==11,"native context preserved");
  }
  RecipeEnvironment e(model);MeleeOwnedBackend<RecipeEnvironment> backend(e);MeleeContextScope<decltype(backend)> scope(backend);
  check(scope.open({7,999,999,1})==MeleeScopeResult::Rejected&&e.calls.mutations==0,"wrong recipe rejected before mutation");
 }
 check(currentPhysicalPublication(7,2478,0,0,100,199),"current primary visual identity selects sword despite other stowed recipes");
 check(!currentPhysicalPublication(7,2478,0,0,100,200)&&!currentPhysicalPublication(7,2478,0,0,101,100),"stale/future publication rejected");
 check(!currentPhysicalPublication(7,2478,1,0,100,100)&&!currentPhysicalPublication(7,2478,1,1,100,100),"selection transitions and secondary damage rejected");
 check(!currentPhysicalPublication(0,2478,0,0,100,100)&&!currentPhysicalPublication(7,1689,0,0,100,100),"missing identity and unsupported publishedmodel rejected");
 check(physicalMeleeRecipe(2478).attack==50&&physicalMeleeRecipe(1250).attack==417,"captured namespace mapping");
 for(uint32_t unsupported:{1514u,1689u,1877u,2199u,0u})check(!physicalMeleeRecipe(unsupported).attack,"unverified families remain disabled");
 for(uint32_t attack:{16u,17u,18u,84u,160u,763u})check(!supportedPhysicalAttack(attack,0),"unverified native recipes denied");
 check(physicalMeleeRecipe(1323).attack==16&&physicalMeleeRecipe(1323).flags==1,"hammer basic capture exact mapping");
 check(!supportedPhysicalAttack(16,0)&&!supportedPhysicalAttack(16,2)&&!supportedPhysicalAttack(17,1)&&!supportedPhysicalAttack(18,2),"hammer flags and finishers never broadened");
 puts("PASS: exact model/attack mappings, single-hand policy, shared geometry, owned lifecycle and equipment invalidation");
}
