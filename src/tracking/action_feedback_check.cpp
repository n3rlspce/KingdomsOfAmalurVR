#include "action_feedback.hpp"
#include <cassert>
#include <cstdio>
using namespace amalur;
int main(){
 ActionFeedbackWriter w;ActionFeedbackReceiver r;
 auto poll=[&](uint64_t now,bool allowed=true){return r.sample(w.packet(),now,allowed,1,2,3);};
 w.sample(1,2,3,4,100,true);assert(!poll(100).hand[0].milliseconds);
 assert(w.confirmed(ActionFeedbackKind::Cast,10,101));auto out=poll(101);assert(out.hand[0].milliseconds==40&&!out.hand[1].milliseconds);
 assert(!w.confirmed(ActionFeedbackKind::Cast,10,102));assert(!poll(102).hand[0].milliseconds);
 assert(w.confirmed(ActionFeedbackKind::Block,10,103));out=poll(103);assert(out.hand[1].milliseconds==50&&!out.hand[0].milliseconds);
 assert(!w.confirmed(ActionFeedbackKind(5),11,104));
 assert(!poll(104,false).hand[1].milliseconds);assert(!poll(105).hand[1].milliseconds);
 assert(w.confirmed(ActionFeedbackKind::Block,11,106));assert(poll(106).hand[1].milliseconds);
 assert(!poll(256).hand[1].milliseconds); // expiry
 w.sample(1,2,3,4,300,true);assert(!poll(300).hand[0].milliseconds);
 assert(!w.confirmed(ActionFeedbackKind::Block,11,301)); // no consumed replay after gap
 assert(w.confirmed(ActionFeedbackKind::Cast,12,302));assert(poll(302).hand[0].milliseconds);
 w.sample(1,2,3,5,303,true);assert(!poll(303).hand[0].milliseconds); // changed owner
 auto bad=w.packet();bad.bridge=9;assert(!r.sample(bad,304,true,1,2,3).hand[0].milliseconds);
 w.sample(1,2,3,5,305,false);assert(!w.confirmed(ActionFeedbackKind::Cast,13,305));
 w.sample(1,2,3,5,306,true);assert(!poll(306).hand[0].milliseconds);
 assert(w.confirmed(ActionFeedbackKind::Cast,13,307));assert(!poll(306).hand[0].milliseconds); // future event
 assert(!poll(307).hand[0].milliseconds); // baseline discards pending event
 assert(!w.confirmed(ActionFeedbackKind::Cast,14,457)); // stale writer
 w.sample(1,2,3,5,500,true);assert(!poll(500).hand[0].milliseconds);
 assert(w.confirmed(ActionFeedbackKind::Arrow,0x8000000000000001ull,501));out=poll(501);assert(out.hand[0].milliseconds==40&&!out.hand[1].milliseconds);
 assert(!w.confirmed(ActionFeedbackKind::Arrow,0x8000000000000001ull,502));
 // Independent kinds can legitimately share the same source token.
 w.sample(1,2,3,5,510,true);
 assert(w.confirmed(ActionFeedbackKind::Cast,77,511));assert(poll(511).hand[0].milliseconds);
 assert(w.confirmed(ActionFeedbackKind::Arrow,77,512));assert(poll(512).hand[0].milliseconds);
 assert(!w.confirmed(ActionFeedbackKind::Cast,77,513));
 assert(w.confirmed(ActionFeedbackKind::Damage,77,514));out=poll(514);assert(out.hand[0].milliseconds&&out.hand[1].milliseconds);
 assert(!w.confirmed(ActionFeedbackKind::Damage,78,515));
 w.sample(1,2,3,5,640,true);poll(640);
 w.sample(1,2,3,5,763,true);poll(763);
 assert(!w.confirmed(ActionFeedbackKind::Damage,79,763)); // 249ms
 assert(!w.confirmed(ActionFeedbackKind::Damage,79,764)); // suppressed event never replayed
 assert(w.confirmed(ActionFeedbackKind::Damage,80,764));out=poll(764);assert(out.hand[0].milliseconds&&out.hand[1].milliseconds);
 assert(w.confirmed(ActionFeedbackKind::Arrow,80,765));assert(poll(765).hand[0].milliseconds);
 w.sample(1,2,3,6,766,true);poll(766); // owner change resets damage budget and tokens
 assert(w.confirmed(ActionFeedbackKind::Damage,80,767));out=poll(767);assert(out.hand[0].milliseconds&&out.hand[1].milliseconds);
 w.sample(1,9,3,6,768,true);
 ActionFeedbackReceiver fresh;fresh.sample(w.packet(),768,true,1,9,3);
 assert(w.confirmed(ActionFeedbackKind::Damage,80,769));out=fresh.sample(w.packet(),769,true,1,9,3);assert(out.hand[0].milliseconds&&out.hand[1].milliseconds);
 puts("PASS action haptic hands, dedup, identities, stale/future data, focus loss and no replay");
}
