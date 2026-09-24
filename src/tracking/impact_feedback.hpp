#pragma once
#include <cstdint>
namespace amalur {
struct ImpactFeedbackPacket {
 uint32_t version{1},writer{},bridge{},generation{},weapon{},owner{},active{},heavyMask{};
 uint64_t epoch{},tick{};uint32_t serial[2]{};uint64_t hitTick[2]{};
};
static_assert(sizeof(ImpactFeedbackPacket)==72,"impact shared ABI");
class ImpactFeedbackWriter {
 ImpactFeedbackPacket p_{};
public:
 const ImpactFeedbackPacket& packet()const{return p_;}
 void sample(uint32_t writer,uint32_t bridge,uint32_t generation,uint32_t weapon,uint32_t owner,uint64_t now,bool allowed){
  const bool identity=p_.writer==writer&&p_.bridge==bridge&&p_.generation==generation&&p_.weapon==weapon&&p_.owner==owner;
  if(!identity){p_={};p_.writer=writer;p_.bridge=bridge;p_.generation=generation;p_.weapon=weapon;p_.owner=owner;}
  const bool continuity=p_.active&&p_.tick<=now&&now-p_.tick<150;
  if(!allowed||!writer||!bridge||!weapon||!owner||!now){p_.active=0;p_.tick=now;return;}
  if(!continuity){p_.epoch=now;p_.hitTick[0]=p_.hitTick[1]=0;p_.heavyMask=0;}
  p_.active=1;p_.tick=now;
 }
 bool confirmed(unsigned hand,uint32_t serial,uint64_t now,bool heavy){
  if(hand>1||!serial||!p_.active||p_.tick>now||now-p_.tick>=150||p_.serial[hand]==serial)return false;
  p_.serial[hand]=serial;p_.hitTick[hand]=p_.tick=now;
  p_.heavyMask=(p_.heavyMask&~(1u<<hand))|(unsigned(heavy)<<hand);return true;
 }
};
struct ImpactPulse {unsigned milliseconds{};float amplitude{};};
struct ImpactPulses {ImpactPulse hand[2]{};};
class ImpactFeedbackReceiver {
 ImpactFeedbackPacket previous_{};bool have_{};uint64_t armedAt_{};
public:
 void reset(){*this={};}
 ImpactPulses sample(const ImpactFeedbackPacket& p,uint64_t now,bool allowed,uint32_t writer,uint32_t bridge,uint32_t generation){
  bool valid=p.version==1&&p.writer==writer&&writer&&p.bridge==bridge&&bridge&&p.generation==generation&&p.owner&&p.weapon
   &&p.active==1&&p.epoch&&p.epoch<=p.tick&&p.tick<=now&&now-p.tick<150&&p.heavyMask<=3;
  for(unsigned h=0;h<2;++h)valid=valid&&(!p.hitTick[h]||(p.serial[h]&&p.hitTick[h]>=p.epoch&&p.hitTick[h]<=p.tick));
  if(!allowed||!valid){reset();return {};}
  const bool same=have_&&previous_.writer==p.writer&&previous_.bridge==p.bridge&&previous_.generation==p.generation
   &&previous_.weapon==p.weapon&&previous_.owner==p.owner&&previous_.epoch==p.epoch&&previous_.tick<=now&&now-previous_.tick<150;
  if(!same){previous_=p;have_=true;armedAt_=now;return {};}
  ImpactPulses out;
  for(unsigned h=0;h<2;++h)if(p.serial[h]!=previous_.serial[h]&&p.hitTick[h]>=armedAt_&&p.hitTick[h]&&now-p.hitTick[h]<150)
   out.hand[h]=(p.heavyMask&(1u<<h))?ImpactPulse{45,.45f}:ImpactPulse{35,.35f};
  previous_=p;return out;
 }
};
}
