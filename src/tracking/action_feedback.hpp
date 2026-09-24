#pragma once
#include <cstdint>
namespace amalur {
enum class ActionFeedbackKind : uint32_t { Block=1, Cast=2, Arrow=3, Damage=4 };
struct ActionFeedbackPacket {
 uint32_t version{1},writer{},bridge{},generation{},owner{},active{};
 uint64_t epoch{},tick{},event[2]{},eventTick[2]{};
};
static_assert(sizeof(ActionFeedbackPacket)==72,"action feedback ABI");
inline unsigned actionFeedbackHand(ActionFeedbackKind kind){return kind==ActionFeedbackKind::Block?1u:0u;}
class ActionFeedbackWriter {
 ActionFeedbackPacket p_{};
 uint64_t lastIdentity_[4]{},serial_{},lastDamage_{};
public:
 const ActionFeedbackPacket& packet()const{return p_;}
 void sample(uint32_t writer,uint32_t bridge,uint32_t generation,uint32_t owner,uint64_t now,bool allowed){
  const bool same=p_.writer==writer&&p_.bridge==bridge&&p_.generation==generation&&p_.owner==owner;
  if(!same){for(auto& identity:lastIdentity_)identity=0;serial_=lastDamage_=0;p_={};p_.writer=writer;p_.bridge=bridge;p_.generation=generation;p_.owner=owner;}
  const bool continuous=p_.active&&p_.tick<=now&&now-p_.tick<150;
  if(!allowed||!writer||!bridge||!owner||!now){p_.active=0;p_.tick=now;return;}
  if(!continuous){p_.epoch=now;p_.eventTick[0]=p_.eventTick[1]=0;}
  p_.active=1;p_.tick=now;
 }
 bool confirmed(ActionFeedbackKind kind,uint64_t identity,uint64_t now){
  const auto slot=static_cast<unsigned>(kind);
  if(slot<1||slot>4||!identity||!p_.active||now<p_.tick||now-p_.tick>=150)return false;
  if(lastIdentity_[slot-1]==identity)return false;
  // Tokens are synchronously ordered by each source detector. Dedup covers the
  // current token per kind, not arbitrary historical reordering; detectors own
  // their event lifetime. No numeric ordering is assumed (native stats reset).
  // Consume each source token independently before rate limiting. A throttled
  // damage event must never reappear once its cooldown expires.
  lastIdentity_[slot-1]=identity;
  if(kind==ActionFeedbackKind::Damage){
   if(lastDamage_&&(now<lastDamage_||now-lastDamage_<250))return false;
   lastDamage_=now;
  }
  // Transport serial is independent of source token: equal cast/arrow/damage
  // identities on the same hand remain distinct changes for the receiver.
  if(!++serial_)++serial_;
  const unsigned first=kind==ActionFeedbackKind::Damage?0:actionFeedbackHand(kind);
  const unsigned last=kind==ActionFeedbackKind::Damage?1:first;
  for(unsigned hand=first;hand<=last;++hand){p_.event[hand]=serial_;p_.eventTick[hand]=now;}
  p_.tick=now;return true;
 }
};
struct ActionFeedbackPulse {unsigned milliseconds{};float amplitude{};};
struct ActionFeedbackPulses {ActionFeedbackPulse hand[2]{};};
class ActionFeedbackReceiver {
 ActionFeedbackPacket previous_{};bool have_{};uint64_t armedAt_{};
public:
 void reset(){*this={};}
 ActionFeedbackPulses sample(const ActionFeedbackPacket& p,uint64_t now,bool allowed,uint32_t writer,uint32_t bridge,uint32_t generation){
  bool valid=p.version==1&&p.writer==writer&&writer&&p.bridge==bridge&&bridge&&p.generation==generation&&p.owner&&p.active==1
   &&p.epoch&&p.epoch<=p.tick&&p.tick<=now&&now-p.tick<150;
  for(unsigned h=0;h<2;++h)valid=valid&&(!p.eventTick[h]||(p.event[h]&&p.eventTick[h]>=p.epoch&&p.eventTick[h]<=p.tick));
  if(!allowed||!valid){reset();return {};}
  const bool same=have_&&previous_.writer==p.writer&&previous_.bridge==p.bridge&&previous_.generation==p.generation
   &&previous_.owner==p.owner&&previous_.epoch==p.epoch&&previous_.tick<=now&&now-previous_.tick<150;
  if(!same){previous_=p;have_=true;armedAt_=now;return {};}
  ActionFeedbackPulses out;
  for(unsigned h=0;h<2;++h)if(p.event[h]!=previous_.event[h]&&p.eventTick[h]>=armedAt_&&p.eventTick[h]&&now-p.eventTick[h]<150)
   out.hand[h]=h?ActionFeedbackPulse{50,.5f}:ActionFeedbackPulse{40,.3f};
  previous_=p;return out;
 }
};
}
