#pragma once
#include <cstdint>
namespace amalur {
// Whole-object hiding is safe only when every mapped joint belongs to the
// head subtree or its attachment ancestry. Unknown layouts stay visible.
inline bool headOnlyPalette(const uint32_t* rootIds,const int16_t* parents,unsigned rootCount,
                            const uint32_t* meshIds,unsigned meshCount){
    if(!rootIds||!parents||!meshIds||!rootCount||rootCount>128||!meshCount||meshCount>128)return false;
    unsigned head=rootCount;
    for(unsigned i=0;i<rootCount;++i){
        if(parents[i]<-1||parents[i]>=int(i))return false;
        for(unsigned j=0;j<i;++j)if(rootIds[i]==rootIds[j])return false;
        if(rootIds[i]==0x5a2e4c)head=i;
    }
    if(head==rootCount)return false;
    bool allowed[128]{};
    for(int i=int(head);i>=0;i=parents[i])allowed[i]=true;
    bool descendant[128]{};
    for(unsigned i=0;i<rootCount;++i){
        descendant[i]=i==head||(parents[i]>=0&&descendant[parents[i]]);
        allowed[i]=allowed[i]||descendant[i]||parents[i]==-1;
    }
    bool containsHead=false;
    for(unsigned i=0;i<meshCount;++i){
        unsigned mapped=rootCount;
        for(unsigned j=0;j<rootCount;++j)if(meshIds[i]==rootIds[j]){mapped=j;break;}
        if(mapped==rootCount||!allowed[mapped])return false;
        containsHead|=mapped==head;
    }
    return containsHead;
}
}
