#pragma once
#include "../tracking/action_feedback_channel.hpp"
// Included after player_rig, motion_controls, weapon_control and game_pause.
namespace native_action_haptics {
inline SRWLOCK feedbackLock=SRWLOCK_INIT;
inline amalur::ActionFeedbackWriter writer;
inline amalur::ActionFeedbackChannel channel;
struct Context {uint32_t owner{},bridge{},generation{};uint64_t now{};bool allowed{};};
inline Context context(uint32_t owner){
 Context c{};c.owner=owner;c.now=GetTickCount64();
 __try {
  const auto p=reinterpret_cast<uintptr_t>(player_rig::player.load());
  if(!p||(player_rig::word(p)!=gameBase+0x1359f14&&player_rig::word(p)!=gameBase+0x1359e94)
    ||!owner||player_rig::word(p+0x1ec)!=owner||!player_rig::resolve(owner))return c;
  amalur::MotionInputPacket input;
  AcquireSRWLockExclusive(&motion_controls::lock);
  const bool fresh=motion_controls::channel.open(false)&&motion_controls::channel.read(input);
  ReleaseSRWLockExclusive(&motion_controls::lock);
  if(!fresh)return c;c.bridge=input.session;
  AcquireSRWLockShared(&weapon_control::poseLock);c.generation=weapon_control::generation;ReleaseSRWLockShared(&weapon_control::poseLock);
  c.allowed=motion_controls::nativeAttackContext()&&headTracking.load()&&game_pause::sample(true)==0&&!amalur::playMode.normal();
 }__except(EXCEPTION_EXECUTE_HANDLER){c.allowed=false;}
 return c;
}
inline void publish(){auto p=writer.packet();channel.transfer(p,true);}
inline void update(uint32_t owner){
 const auto c=context(owner);AcquireSRWLockExclusive(&feedbackLock);
 writer.sample(GetCurrentProcessId(),c.bridge,c.generation,c.owner,c.now,c.allowed);publish();ReleaseSRWLockExclusive(&feedbackLock);
}
// Source must verify local native completion and supply a deduplicated identity.
inline void emit(amalur::ActionFeedbackKind kind,uint32_t owner,uint64_t nativeEventIdentity){
 const auto c=context(owner);AcquireSRWLockExclusive(&feedbackLock);
 writer.sample(GetCurrentProcessId(),c.bridge,c.generation,c.owner,c.now,c.allowed);
 const bool sent=writer.confirmed(kind,nativeEventIdentity,c.now);publish();ReleaseSRWLockExclusive(&feedbackLock);
 if(sent)log("VR action haptic tick=%llu kind=%u owner=%08x identity=%llu\n",c.now,unsigned(kind),owner,nativeEventIdentity);
}
}
