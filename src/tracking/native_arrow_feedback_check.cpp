#include "native_arrow_feedback.hpp"
#include <cassert>
#include <cstdio>
#include <stdexcept>
int main(){
 using namespace amalur;NativeArrowObservation o;NativeArrowIdentity p{10,20,30,true};
 assert(!o.completed(84,p,p));assert(!o.completed(0,p,p));auto q=p;q.owner++;assert(!o.completed(nativeArrowFiredHash,p,q));q=p;q.entity++;assert(!o.completed(nativeArrowFiredHash,p,q));q=p;q.player++;assert(!o.completed(nativeArrowFiredHash,p,q));q=p;q.valid=false;assert(!o.completed(nativeArrowFiredHash,p,q));
 auto a=o.completed(nativeArrowFiredHash,p,p),b=o.completed(nativeArrowFiredHash,p,p);assert(a==0x8000000000000001ULL&&b==a+1);
 unsigned calls=0,after=0,notifies=0;uintptr_t self=0x12345678,arg=0xfedcba98;
 auto result=observeNativeArrowCall([&]{++calls;assert(self==0x12345678&&arg==0xfedcba98);return uintptr_t(0xfedba987);},[&]{++after;return p;},[&](auto i){++notifies;assert(i.owner==30);});
 assert(result==0xfedba987&&calls==1&&after==1&&notifies==1);
 try{observeNativeArrowCall([&]()->uintptr_t{++calls;throw std::runtime_error("native failure");},[&]{++after;return p;},[&](auto){++notifies;});assert(false);}catch(const std::runtime_error&){}
 assert(calls==2&&after==1&&notifies==1);
 puts("PASS exact arrow release hash, stable local owner, distinct shots, call-once/raw return and native exception forwarding");
}
