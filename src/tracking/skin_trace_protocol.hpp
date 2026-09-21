#pragma once
#include "arm_pose.hpp"
#include "arm_trace.hpp"
#include <array>
namespace amalur::skin_audit {
constexpr uint32_t version=4,objectSlots=16,historyDepth=4,drawCapacity=128;
enum Origin:uint32_t {Remap=1,Getter=2};
// None of these statuses means a verified player association. That requires
// native palette/bind evidence which this discovery capture does not yet have.
enum Association:uint32_t {Unassociated=0,Ambiguous=1};
enum Counter:uint32_t {Draws,Deferred,MissingWorld,MissingSkin,CpuPublished,CpuEvicted,
    NotSelected,Published,Evicted,WorldUploads,SkinUploads,PendingFull,UploadBudget,UpdateHookCalls,CpuDropped,UploadOutsideWindow,
    Selected,WorldUnbound,SkinUnbound,WorldQueryFailed,SkinQueryFailed,WorldSizeRejected,SkinSizeRejected,
    WatchedWorld,WatchedSkin,MapCalls,MapUnwatched,UnmapMiss,CopyCalls,CopyUnwatched,WorldTypeRejected,SkinTypeRejected};
enum DrawFlags:uint32_t {CameraMatched=1,WorldCaptured=2,SkinCaptured=4};
#pragma pack(push,4)
struct CpuRecord {
    ArmTraceRecord trace{};
    uint64_t serial{},qpc{},solveId{};
    uint32_t origin{},thread{},asset{},sourceBuffer{},count{},childAsset{},childBuffer{},childCount{},modes{},center{};
    uint64_t nativeHash{},solvedHash{},childHash{},layoutHash{},bodyBefore{},bodyAfter{},visualBefore{},visualAfter{};
    RigBone rootWorldRaw{},childWorldBefore{}; // includes raw native scale/flags
    uint32_t ids[64]{},childIds[64]{};
    int16_t parents[64]{},childParents[64]{};
    RigBone original[64]{},solved[64]{},child[64]{};
};
struct DrawRecord {
    uint32_t frame{},worldFrame{},skinFrame{},vertexBuffer{},indexBuffer{},stride{},count{},flags{};
    uint32_t worldBuffer{},skinBuffer{},vertexShader{},pixelShader{},ordinal{},association{},candidateCount{},thread{};
    uint32_t worldBytes{},skinBytes{},worldBindFlags{},skinBindFlags{};
    uint64_t tick{},cameraTick{},worldSerial{},skinSerial{},cpuSerials[objectSlots]{};
    float world[64]{},skin[936]{},vp[16]{};
};
struct Buffer {
    uint32_t version{},pid{},drawStride{},drawCapacity{},published{},dropped{};
    uint64_t until{};
    uint32_t cpuStride{},cpuSlots{},cpuDepth{},cpuPublished{};
    uint32_t counters[32]{};
    CpuRecord cpu[objectSlots][historyDepth];
    DrawRecord draws[skin_audit::drawCapacity];
};
#pragma pack(pop)
static_assert(sizeof(CpuRecord)==10708&&sizeof(DrawRecord)==4304);
static_assert(offsetof(Buffer,cpu)==176);
inline uint64_t hash(const void* data,size_t bytes,uint64_t seed=14695981039346656037ull){
    auto p=static_cast<const unsigned char*>(data);
    while(bytes--){seed^=*p++;seed*=1099511628211ull;}return seed;
}
// Two consecutive frames per window: first watches bindings, second can read
// their following upload. Inspect every ordinal over a stable draw list, not
// just its first candidates. Discovery window never depends on capture success.
struct Coverage {
    uint32_t frame{},observed{},previous{},start{},phase{};bool ready{};
    bool select(uint32_t current,uint32_t& ordinal){
        if(!ready||frame!=current){
            if(ready){previous=observed;if(++phase==2){phase=0;start+=24;}}
            if(start>=previous)start=0;
            ready=true;frame=current;observed=0;
        }
        ordinal=observed++;return ordinal>=start&&ordinal-start<24;
    }
    bool wantsNext(uint32_t current)const{auto copy=*this;uint32_t ordinal{};return copy.select(current,ordinal);}
};
struct CopyBudget {
    uint32_t frame{},world{},skin{};bool ready{};
    bool take(uint32_t current,bool isSkin){
        if(!ready||frame!=current){ready=true;frame=current;world=skin=0;}
        auto& n=isSkin?skin:world;if(n>=48)return false;++n;return true;
    }
};
}
