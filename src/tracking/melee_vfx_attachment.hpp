#pragma once
// Included inside native_vfx; also usable independently by the offline check.
// Frozen native-reference traces34/257 prove dagger aliases -> bones6/3.
inline uint32_t trailAttachmentAlias(uint32_t model,unsigned hand){
    if(hand>1||(hand==1&&model!=1520))return 0;
    return hand?0x00ceac76:0x00858053;
}
inline int trailAttachmentIndex(uint32_t model,unsigned hand,unsigned boneCount,
    unsigned aliasCount,const uint32_t* aliases,const int16_t* indices){
    const auto alias=trailAttachmentAlias(model,hand);
    if(!alias||!aliases||!indices||!aliasCount||aliasCount>4096||!boneCount||boneCount>64)return -1;
    int result=-1;bool found=false;
    for(unsigned i=0;i<aliasCount;++i)if(aliases[i]==alias){
        if(found)return -1;found=true;result=indices[i];
    }
    if(result<0||unsigned(result)>=boneCount)return -1;
    // Do not apply the captured dagger hand assignment to another skeleton.
    if(model==1520&&(boneCount!=7||result!=(hand?3:6)))return -1;
    return result;
}
