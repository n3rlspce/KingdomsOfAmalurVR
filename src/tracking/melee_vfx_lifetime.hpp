#pragma once
#include "melee_swing_event.hpp"
#include <cmath>
#include <cstdint>
namespace amalur {
// Native group/spatial indices have different namespaces and no generation.
// Slots hold PRIVATE monotonic backend tokens, never those native indices.
// Pool epochs also invalidate tokens across in-place native manager resets.
struct MeleeVfxEpoch {
    uintptr_t groups{},instances{},spatial{};
    uint32_t owner{},weapon{},model{};unsigned generation{},poolGeneration{};
    bool valid()const{return groups&&instances&&spatial&&owner&&weapon&&model;}
    bool pools(const MeleeVfxEpoch& b)const{return groups==b.groups&&instances==b.instances&&spatial==b.spatial&&poolGeneration==b.poolGeneration;}
    bool same(const MeleeVfxEpoch& b)const{return pools(b)&&owner==b.owner&&weapon==b.weapon&&model==b.model&&generation==b.generation;}
};
struct MeleeVfxRecipe {
    uint32_t asset{},durationMs{};
    // An independently resolved current resource, never a captured binding.
    bool descriptorVerified{},attachmentVerified{},parametersVerified{},retirementVerified{};
    bool complete()const{return asset>=2&&durationMs&&durationMs<=600&&descriptorVerified&&attachmentVerified&&parametersVerified&&retirementVerified;}
};
struct MeleeVfxSlot {uint32_t group{};uint64_t born{},until{};};
class MeleeVfxLifetime {
public:
    const MeleeVfxEpoch& epoch()const{return epoch_;}
    const MeleeVfxSlot& slot(unsigned hand)const{return slots_[hand];}
    unsigned abandoned()const{return abandoned_;}
    bool faulted()const{return faulted_;}
    template<class Backend> void release(Backend& backend,MeleeVfxSlot& s,const MeleeVfxEpoch& current){
        if(!s.group)return;
        if(!epoch_.pools(current)||!backend.owns(epoch_,s.group)){++abandoned_;s={};return;}
        if(!backend.cancel(epoch_,s.group))faulted_=true;
        s={};
    }
    template<class Backend> void reset(Backend& backend,const MeleeVfxEpoch& current){
        for(auto& s:slots_)release(backend,s,current);
        epoch_={};lastTick_=0;serial_[0]=serial_[1]=0;allowed_=false;
    }
    template<class Backend> void update(Backend& backend,const MeleeVfxEpoch& current,bool allowed,uint64_t now){
        if(!epoch_.same(current)||!allowed||!current.valid()||!now||(lastTick_&&now<lastTick_)){
            // A pause/focus loss cancels effects, but retains accepted serials
            // for the same identity: an old inbox event cannot restart a trail.
            const bool retain=epoch_.same(current);const auto right=serial_[0],left=serial_[1];
            reset(backend,current);epoch_=current;
            if(retain){serial_[0]=right;serial_[1]=left;}
        }
        allowed_=!faulted_&&allowed&&current.valid()&&now;lastTick_=now;
        for(auto& s:slots_)if(s.group&&now>=s.until){release(backend,s,current);}
    }
    template<class Backend> bool start(Backend& backend,const MeleeSwingEvent& e,const MeleeVfxRecipe& recipe,uint64_t now){
        if(!allowed_||!recipe.complete()||e.hand>=2||!e.serial||serial_[e.hand]==e.serial
            ||e.owner!=epoch_.owner||e.weapon!=epoch_.weapon||e.asset!=epoch_.model||e.generation!=epoch_.generation
            ||!e.tick||e.tick>now||now-e.tick>100||now!=lastTick_)return false;
        const auto& p=e.weaponPose.position;
        if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)
            ||std::fabs(p.x)>1000000||std::fabs(p.y)>1000000||std::fabs(p.z)>1000000)return false;
        // Commit before allocation; a failed native request is never retried
        // every frame, and re-entrant delivery cannot duplicate the gesture.
        serial_[e.hand]=e.serial;
        auto& slot=slots_[e.hand];
        if(slot.group){release(backend,slot,epoch_);if(faulted_)return false;}
        uint32_t group{};
        if(!backend.create(epoch_,group)||!group){if(group&&!backend.cancel(epoch_,group))faulted_=true;return false;}
        // Registration rollback always cancels the whole newly owned group,
        // including partially constructed attachment caches or queued effects.
        if(!backend.attach(epoch_,group,e)||!backend.schedule(epoch_,group,e,recipe)){
            if(!backend.cancel(epoch_,group))faulted_=true;return false;
        }
        slot={group,now,now+recipe.durationMs+100};return true;
    }
    template<class Backend> void pose(Backend& backend,unsigned hand,const MeleeSwingEvent& current,uint64_t now){
        if(hand>=2||!slots_[hand].group)return;
        if(!allowed_||current.hand!=hand||current.owner!=epoch_.owner||current.weapon!=epoch_.weapon
            ||current.asset!=epoch_.model||current.generation!=epoch_.generation||!current.tick
            ||current.tick>now||now-current.tick>100||!backend.pose(epoch_,slots_[hand].group,current)){
            release(backend,slots_[hand],epoch_);
        }
    }
private:
    MeleeVfxEpoch epoch_{};MeleeVfxSlot slots_[2]{};unsigned serial_[2]{},abandoned_{};
    uint64_t lastTick_{};bool allowed_{},faulted_{};
};
}
