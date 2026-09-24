#include "native_attack_input.hpp"
#include <cassert>
int main(){
 amalur::NativeAttackInput p;
 assert(!p.held(100,true));
 p.observe(true,true,0x4000,0,100);assert(p.held(100,true));assert(p.held(199,true));assert(!p.held(200,true));assert(!p.held(99,true));
 assert(!p.held(101,false)); // focus/menu loss even without another pad poll
 p.observe(true,true,0x8000,30,200);assert(p.held(200,true)); // secondary and exact native RT threshold
 p.observe(true,true,0xc000,31,201);assert(!p.held(201,true)); // native spell layer
 p.observe(true,true,0xc000,0,202);assert(p.held(202,true));
 p.observe(true,true,0,0,203);assert(!p.held(203,true)); // no post-release ownership
 p.observe(true,true,0x4000,0,204);p.observe(false,true,0xffff,0,205);assert(!p.held(205,true)); // disconnected garbage ignored
 p.observe(true,true,0x8000,0,206);p.observe(true,false,0x8000,0,207);assert(!p.held(207,true));
 p.observe(true,true,0x1000,0,208);assert(!p.held(208,true)); // A is not weapon input
 p.observe(true,true,0x4000,0,209);assert(p.held(209,true)); // native pad fallback needs no XR presence
}
