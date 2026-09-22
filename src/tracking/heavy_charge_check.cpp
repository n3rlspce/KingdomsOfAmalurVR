#define AMALUR_HEAVY_INPUT_MAPPING L"Local\\AmalurHeavyChargeTest"
#define AMALUR_HEAVY_INPUT_MUTEX L"Local\\AmalurHeavyChargeTestMutex"
#include "heavy_charge_input.hpp"
#include "longsword_gesture.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace amalur;
void check(bool v,const char* why){if(!v){std::printf("FAIL: %s\n",why);std::exit(1);}}
int main(){
    HeavyChargeChannel writer,reader;HeavyChargePacket packet;packet.mode=1;packet.session=7;packet.active=1;packet.tick=100;packet.grip=.9f;
    check(writer.transfer(packet,true),"publish separate versioned input");HeavyChargePacket read;check(reader.transfer(read,false)&&read.mode==1&&read.grip==packet.grip,"shared input read");
    GripChargeGate grip;LongswordGesture gesture;
    auto frame=[&](float amount,bool active=true,bool spell=false){packet.tick+=10;packet.grip=amount;packet.active=active;packet.spell=spell;
        const bool held=grip.sample(packet,packet.tick,true);
        gesture.sample(1,1,packet.tick,active&&!spell,held,0,false);return held;};
    for(int i=0;i<120;++i)frame(.9f);check(!gesture.ready(),"held on startup cannot charge");
    frame(0);for(int i=0;i<110;++i)frame(.9f);check(gesture.ready(),"grip held one second readies heavy at any position");
    check(gesture.commit(packet.tick).heavy,"native heavy selected");grip.consume();
    for(int i=0;i<120;++i)frame(.9f);check(!gesture.ready(),"one heavy per grip hold");
    frame(0);for(int i=0;i<110;++i)frame(.9f);frame(0);for(int i=0;i<30;++i)frame(0);check(gesture.commit(packet.tick).heavy,"release then strike in grace");
    frame(0);for(int i=0;i<110;++i)frame(.9f);for(int i=0;i<75;++i)frame(0);check(!gesture.ready(),"release grace expires");
    for(int i=0;i<110;++i)frame(.9f);frame(.9f,true,true);check(!gesture.ready(),"explicit spell cancels charge");
    for(int i=0;i<120;++i)frame(.9f);check(!gesture.ready(),"spell cannot leave held charge armed");
    frame(0);for(int i=0;i<110;++i)frame(.9f);frame(.9f,false);for(int i=0;i<120;++i)frame(.9f);check(!gesture.ready(),"focus loss requires fresh release");
    frame(0);frame(.9f);check(!grip.sample(packet,packet.tick+250,true),"stale input rejected");
    packet.mode=0;check(!grip.sample(packet,packet.tick,true),"position setting never reads grip as charge");
    packet.mode=1;packet.grip=std::numeric_limits<float>::quiet_NaN();check(!grip.sample(packet,packet.tick,true),"invalid grip rejected");
    puts("PASS: grip charge timing, native heavy, single consumption, release grace, spell/focus/mode/stale safety, shared input");
}
