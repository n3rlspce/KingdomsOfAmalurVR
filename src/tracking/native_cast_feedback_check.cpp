#include "native_cast_feedback.hpp"
#include <cassert>
#include <cstdio>
int main(){
 using namespace amalur;
 NativeCastDefinition magic{1,2434,4,0,0xdac,0,0,0xc803,0x01000104,783,0xef010ecc};
 NativeCastDefinition staff{1,1077,1,0,0,0,0,0x0100c002,0x01000104,1174,0x41950419};
 assert(capturedNativeCast(892,magic));assert(capturedNativeCast(160,staff));assert(capturedNativeCast(161,staff));
 for(auto ordinary:{50u,81u,199u,417u,16u,84u})assert(!capturedNativeCast(ordinary,staff));
 assert(!capturedNativeCast(892,staff));assert(!capturedNativeCast(160,magic));
 auto wrong=magic;wrong.scriptHash^=1;assert(!capturedNativeCast(892,wrong));wrong=magic;wrong.kind=1;assert(!capturedNativeCast(892,wrong));
 wrong=magic;wrong.field1f8=0;assert(!capturedNativeCast(892,wrong));
 NativeCastObservation o;
 assert(!o.created(123,1,1,892,magic,1,false,true));
 assert(!o.created(123,2,1,892,magic,0,false,true));
 assert(!o.created(123,1,1,892,magic,0,true,true));
 assert(!o.created(123,1,1,892,magic,0,false,false));
 assert(!o.created(123,1,1,50,staff,0,false,true));
 const auto first=o.created(123,1,1,892,magic,0,false,true);assert(first);
 assert(!o.created(123,1,1,892,magic,0,false,true));
 // No timer guesses: same address remains consumed until native reset.
 assert(!o.created(123,1,1,160,staff,0,false,true));
 o.retired(456);assert(!o.created(123,1,1,892,magic,0,false,true));
 o.retired(123);const auto next=o.created(123,1,1,892,magic,0,false,true);assert(next>first);
 const auto dual=o.created(456,1,1,160,staff,0,false,true);assert(dual>next);
 o.retired(123);assert(!o.created(456,1,1,160,staff,0,false,true));
 puts("PASS native cast start definitions, failure/foreign/owned rejection, duplicate and reset/address reuse");
}
