#pragma once
#include <cstdint>

namespace amalur {
// Tokens include a lifetime serial, not just an address/index. The backend must
// observe native creation/destruction to distinguish in-place pool reuse.
struct MeleeRuntimeToken {
    uint32_t index{};
    uint64_t generation{};
    explicit operator bool() const {return index!=0&&generation!=0;}
};
struct MeleeKeyToken {
    uint32_t key{};
    uint64_t generation{};
    explicit operator bool() const {return key!=0&&generation!=0;}
};
struct MeleeContextRecipe {
    uint32_t owner{},baseAsset{},selectedAsset{};
    uint64_t equipmentGeneration{};
};
enum class MeleeScopeResult {Rejected,ReserveFailed,CreateFailed,BindFailed,Expired,CleanupFailed,Ready};

// One synchronous native-thread operation, never a cached attack context.
// Backend contract:
// - supported verifies retained assets, equipment epoch and supported effects;
// - reserve returns a distinct owned key, never an existing native attack key;
// - create returns a fresh lifetime token, with failure fully rolled back;
// - bind attaches both runtimes (or rolls back every partial attachment);
// - matches rejects changed key/runtime generations;
// - unbind detaches runtime references before any release callback can reenter
//   native key cleanup. It must preserve replacement lifetimes and return false
//   if it cannot establish safe detachment. This permanently stops the scope;
// - erase detaches this key WITHOUT freeing any runtime; release validates its
//   token's generation before touching a runtime (retired/reused => no action).
//   Both return success; a cleanup failure permanently stops further opens.
// This class does not allocate, read or write game memory.
template<class Backend>
class MeleeContextScope {
public:
    explicit MeleeContextScope(Backend& backend):backend_(backend){}
    MeleeContextScope(const MeleeContextScope&)=delete;
    MeleeContextScope& operator=(const MeleeContextScope&)=delete;
    ~MeleeContextScope(){close();}

    MeleeScopeResult open(const MeleeContextRecipe& recipe){
        if(!close())return MeleeScopeResult::CleanupFailed;
        if(!recipe.owner||recipe.baseAsset<2||recipe.selectedAsset<2
            ||!recipe.equipmentGeneration||!backend_.supported(recipe))return MeleeScopeResult::Rejected;
        recipe_=recipe;
        key_=backend_.reserve(recipe.owner);
        if(!key_)return MeleeScopeResult::ReserveFailed;
        base_=backend_.create(recipe.owner,recipe.baseAsset);
        if(!base_)return close()?MeleeScopeResult::CreateFailed:MeleeScopeResult::CleanupFailed;
        if(recipe.selectedAsset==recipe.baseAsset)selected_=base_;
        else selected_=backend_.create(recipe.owner,recipe.selectedAsset);
        if(!selected_)return close()?MeleeScopeResult::CreateFailed:MeleeScopeResult::CleanupFailed;
        if(!backend_.bind(recipe.owner,key_,base_,selected_)){
            return close()?MeleeScopeResult::BindFailed:MeleeScopeResult::CleanupFailed;
        }
        bound_=true;
        if(!current())return close()?MeleeScopeResult::Expired:MeleeScopeResult::CleanupFailed;
        return MeleeScopeResult::Ready;
    }
    bool current() const {
        return bound_&&backend_.supported(recipe_)
            &&backend_.matches(recipe_.owner,key_,base_,selected_);
    }
    // Caller must revalidate current() immediately before native resolution.
    MeleeKeyToken key() const {return current()?key_:MeleeKeyToken{};}
    MeleeRuntimeToken selected() const {return current()?selected_:MeleeRuntimeToken{};}
    // A native exception can interrupt a mutation before ownership is known.
    // Quarantine instead of guessing which partially attached objects to free.
    void quarantine(){key_={};base_={};selected_={};bound_=false;recipe_={};cleanupFailed_=true;}
    bool close(){
        // Clear local ownership first: cleanup callbacks cannot release twice.
        const auto owner=recipe_.owner;
        const auto key=key_;const auto base=base_,selected=selected_;const auto bound=bound_;
        key_={};base_={};selected_={};bound_=false;recipe_={};
        if(cleanupFailed_)return false;
        // Native effect-complete scripts can reenter talent cleanup. Break our
        // key's references first so they cannot cause a second runtime release.
        if(bound&&!backend_.unbind(owner,key,base,selected)){
            cleanupFailed_=true;return false;
        }
        // After detachment, retain native base-before-secondary release order.
        bool clean=true;
        if(base)clean=backend_.release(base)&&clean;
        if(selected&&(selected.index!=base.index||selected.generation!=base.generation))
            clean=backend_.release(selected)&&clean;
        if(key)clean=backend_.erase(owner,key)&&clean;
        cleanupFailed_=!clean;
        return clean;
    }
private:
    Backend& backend_;
    MeleeContextRecipe recipe_{};
    MeleeKeyToken key_{};
    MeleeRuntimeToken base_{},selected_{};
    bool bound_{};
    bool cleanupFailed_{};
};
}
