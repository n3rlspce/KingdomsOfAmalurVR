#include "weapon_drawn_scale.hpp"
#include <cstdio>
#include <cstdlib>
using namespace amalur;
static void check(bool good,const char* message){if(!good){printf("FAIL: %s\n",message);std::exit(1);}}
static RigBone bone(float scale){RigBone b{};b.position={3,4,5};b.positionW=1;b.orientation={0,0,0,1};float s[]{scale,scale,scale};memcpy(b.opaque,s,12);b.opaque[12]=0x5e;b.opaque[13]=0xa5;b.opaque[14]=0x12;b.opaque[15]=0x34;return b;}
int main(){
    DrawnScaleIdentity id{100,200,300,4,5,5457,4,6};
    RigBone input[4];for(auto& b:input)b=bone(.8f);RigBone original[4];memcpy(original,input,sizeof(input));
    DrawnScaleLease lease;
    check(lease.begin(input,id),"measured rusty longsword admitted");
    check(drawnScaleValue(input[0]).xyz[0]==1&&drawnScaleValue(input[1]).xyz[0]==1,"only native remapper anchor scales set to measured full draw");
    check(!memcmp(input+2,original+2,2*sizeof(RigBone)),"descendants left for native reconstruction");
    for(unsigned i=0;i<4;++i){check(!memcmp(input+i,original+i,32),"positions and quaternion untouched");check(!memcmp(input[i].opaque+12,original[i].opaque+12,4),"unrelated flags and tail bytes untouched");}
    // Model the native remapper preserving root/handle scale and constructing
    // descendant scales. The actual remapper requires headset runtime validation;
    // a full DLL build checks integration compilation, not native execution.
    for(unsigned i=2;i<4;++i){float one[]{1,1,1};memcpy(input[i].opaque,one,12);}
    lease.finish(input,id);check(!lease.begin(input,id),"cannot compound active lease");
    input[0].position.x=123;input[0].opaque[12]|=1;
    input[1]=bone(.95f); // Native animator overwrote scale after our output.
    lease.restore(input,id);
    check(input[0].position.x==123&&(input[0].opaque[12]&1)&&drawnScaleValue(input[0]).xyz[0]==.8f,"restore scale preserves newly written pose and unrelated flags");
    check(drawnScaleValue(input[1]).xyz[0]==.95f,"native replacement scale not overwritten");
    check(drawnScaleValue(input[2]).xyz[0]==.8f&&drawnScaleValue(input[3]).xyz[0]==.8f&&!lease.active(),"owned native descendant scales restored and lease cleared");
    for(unsigned mismatch=0;mismatch<8;++mismatch){
        for(auto& b:input)b=bone(.8f);check(lease.begin(input,id),"identity fixture begins");
        auto changed=id;
        if(mismatch==0)++changed.object;if(mismatch==1)++changed.root;if(mismatch==2)++changed.buffer;
        if(mismatch==3)++changed.owner;if(mismatch==4)++changed.rootOwner;if(mismatch==5)++changed.asset;if(mismatch==6)++changed.count;
        if(mismatch==7)++changed.fabIndex;
        RigBone current[4];memcpy(current,input,sizeof(input));lease.restore(input,changed);
        check(!lease.active()&&!memcmp(current,input,sizeof(input)),"reallocation ownership asset or count change loses lease without writing");
    }
    for(unsigned round=0;round<20;++round){
        for(auto& b:input)b=bone(.8f);check(lease.begin(input,id),"repeated remap begins only after retiring preceding output");
        lease.finish(input,id);lease.restore(input,id);
        for(auto& b:input)check(drawnScaleValue(b).xyz[0]==.8f,"repeated remap restores without accumulating scale");
    }
    for(auto& b:input)b=bone(.8f);check(lease.begin(input,id),"removed weapon fixture begins");
    lease.restore(nullptr,{}); // Native Fab-table resolution no longer found the old identity.
    check(!lease.active(),"removed weapon drops lease without touching its stale buffer");
    auto replacement=id;replacement.object=101;replacement.buffer=301;replacement.owner=7;replacement.fabIndex=8;
    for(auto& b:input)b=bone(.8f);check(lease.begin(input,replacement),"replacement measured weapon is not blocked by removed lease");
    lease.restore(input,replacement);
    for(float value:{.8f,.81526f,.86186f,.90433f,.95216f,.95873f,.99464f}){
        for(auto& b:input)b=bone(value);check(lease.begin(input,id),"each observed animation transition admitted");lease.finish(input,id);lease.restore(input,id);
        for(auto& b:input)check(drawnScaleValue(b).xyz[0]==value,"native transition value restored exactly");
    }
    for(auto& b:input)b=bone(1);check(!lease.begin(input,id),"already fully drawn native frame needs no edit");
    for(float invalid:{.7f,1.2f,NAN}){for(auto& b:input)b=bone(invalid);check(!lease.begin(input,id),"unmeasured or invalid scale rejected");}
    for(auto& b:input)b=bone(.8f);auto wrong=id;wrong.asset=2478;check(!lease.begin(input,wrong),"other longsword model not generalized");
    wrong.asset=2621;check(!lease.begin(input,wrong),"new character weapon rejected without measurement");
    input[2]=bone(.9f);check(!lease.begin(input,id),"nonuniform per bone scales are not normalized");
    input[2]=bone(.8f);float unusual=.9f;memcpy(input[0].opaque+4,&unusual,4);check(!lease.begin(input,id),"anisotropic scaling rejected");
    input[0]=bone(.8f);input[2].opaque[12]&=~0x40;check(!lease.begin(input,id),"unexpected inactive native scale representation rejected");
    puts("PASS: measured5457 scale anchors/native descendant lease, transitions, fullsize no-op, no compounding, non-scale preservation, native overwrite and identity resets");
}
