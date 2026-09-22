#pragma once
#include <cstdint>
namespace amalur {
struct ChargedFeedbackPacket {
    uint32_t version{1},writer{},bridge{},generation{},weapon{},active{};
    uint64_t epoch{},tick{},readySequence{},readyTick{},heavySerial{},heavyTick{};
};
static_assert(sizeof(ChargedFeedbackPacket)==72,"shared packet ABI must match x86 diagnostic and x64 bridge");
inline bool validChargedFeedback(const ChargedFeedbackPacket& p,uint64_t now){
    return p.version==1&&p.writer&&p.bridge&&p.weapon&&p.epoch&&p.active<=1
        &&p.tick&&p.tick<=now&&now-p.tick<150
        &&((p.readySequence!=0)==(p.readyTick!=0))&&((p.heavySerial!=0)==(p.heavyTick!=0))
        &&(!p.readyTick||(p.readyTick>=p.epoch&&p.readyTick<=p.tick))
        &&(!p.heavyTick||(p.heavyTick>=p.epoch&&p.heavyTick<=p.tick));
}
// Called under the existing weapon pose lock. Only the successful shared
// contact/peak commit path may record a heavy serial; proposals are not events.
class ChargedFeedbackWriter {
    ChargedFeedbackPacket packet_{};bool wasReady_{};
public:
    const ChargedFeedbackPacket& packet()const{return packet_;}
    void sample(uint32_t writer,uint32_t bridge,uint32_t generation,uint32_t weapon,
        uint64_t now,bool eligible,bool ready){
        const bool same=packet_.active&&packet_.writer==writer&&packet_.bridge==bridge
            &&packet_.generation==generation&&packet_.weapon==weapon
            &&now>=packet_.tick&&now-packet_.tick<150;
        if(!eligible||!writer||!bridge||!weapon||!now){packet_.active=0;packet_.tick=now;wasReady_=false;return;}
        if(!same){
            packet_={};packet_.writer=writer;packet_.bridge=bridge;packet_.generation=generation;
            packet_.weapon=weapon;packet_.epoch=now;wasReady_=ready;
        }else if(ready&&!wasReady_){++packet_.readySequence;packet_.readyTick=now;}
        packet_.active=1;packet_.tick=now;wasReady_=ready;
    }
    bool committed(uint32_t weapon,uint32_t generation,uint64_t serial,uint64_t now){
        if(!packet_.active||packet_.weapon!=weapon||packet_.generation!=generation||!serial
            ||serial==packet_.heavySerial||now<packet_.tick||now-packet_.tick>=150)return false;
        packet_.heavySerial=serial;packet_.heavyTick=packet_.tick=now;wasReady_=false;return true;
    }
};
struct ChargedPulse {unsigned kind{},milliseconds{};float amplitude{};};
// No queued pulses: loss of gameplay/focus/tracking, a new process or recenter
// establishes a silent baseline. Fresh events after that baseline may play once.
class ChargedFeedbackReceiver {
    ChargedFeedbackPacket previous_{};bool have_{};uint64_t armedAt_{},readyAt_{},heavyAt_{};
public:
    void reset(){*this=ChargedFeedbackReceiver{};}
    ChargedPulse sample(const ChargedFeedbackPacket& p,uint64_t now,bool gameplay,
        uint32_t writer,uint32_t bridge,uint32_t generation){
        if(!gameplay||!validChargedFeedback(p,now)||!p.active||p.writer!=writer
            ||p.bridge!=bridge||p.generation!=generation){reset();return {};}
        const bool same=have_&&previous_.writer==p.writer&&previous_.bridge==p.bridge
            &&previous_.generation==p.generation&&previous_.weapon==p.weapon&&previous_.epoch==p.epoch
            &&now>=previous_.tick&&now-previous_.tick<150;
        if(!same){reset();previous_=p;have_=true;armedAt_=now;return {};}
        const bool heavy=p.heavySerial&&p.heavySerial!=previous_.heavySerial&&p.heavyTick>=armedAt_
            &&now-p.heavyTick<150;
        const bool ready=p.readySequence&&p.readySequence!=previous_.readySequence&&p.readyTick>=armedAt_
            &&now-p.readyTick<150;
        previous_=p;
        // A committed strike wins if both events arrived between bridge polls.
        // It may replace a lighter readiness buzz immediately.
        if(heavy&&(!heavyAt_||now-heavyAt_>=100)){heavyAt_=now;return {2,110,.8f};}
        if(ready&&(!readyAt_||now-readyAt_>=250)&&(!heavyAt_||now-heavyAt_>=150)){
            readyAt_=now;return {1,45,.35f};
        }
        return {};
    }
};
}
