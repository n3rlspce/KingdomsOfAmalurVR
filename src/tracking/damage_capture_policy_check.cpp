#include "damage_capture_policy.hpp"
#include <cstdio>
#include <cstdlib>
void check(bool ok){if(!ok)std::exit(1);}
int main(){amalur::DamageCaptureBudget b;unsigned dropped;for(unsigned i=0;i<64;++i)check(b.take(100,dropped));check(!b.take(101,dropped));check(!b.take(1099,dropped));check(b.take(1100,dropped)&&dropped==2);check(b.take(1,dropped));
 check(amalur::damageCaptureRuntime(7,7,4,4,160));check(!amalur::damageCaptureRuntime(7,8,4,4,160));check(!amalur::damageCaptureRuntime(7,7,4,5,160));check(!amalur::damageCaptureRuntime(7,7,0,0,160));check(!amalur::damageCaptureRuntime(7,7,4,4,1000000));puts("PASS: renewable capture budget and owner/index/asset identity guards");}
