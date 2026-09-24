#include "shield_block_feedback.hpp"
#include <cassert>
#include <cstdio>
#include <limits>
int main(){
 using namespace amalur;
 uint32_t result{};int32_t i=4;double d=5;
 assert(blockCounterValue(6,&i,result)&&result==4);
 assert(blockCounterValue(10,&d,result)&&result==5);
 assert(confirmedShieldBlock(4,5,5,true,true));
 assert(confirmedShieldBlock(0,1,1,true,true));
 assert(!confirmedShieldBlock(4,4,4,true,true));
 assert(!confirmedShieldBlock(4,5,4,true,true));
 assert(!confirmedShieldBlock(4,6,6,true,true));
 assert(!confirmedShieldBlock(4,5,5,false,true));
 assert(!confirmedShieldBlock(4,5,5,true,false));
 assert(!confirmedShieldBlock(7,0,0,true,true));
 assert(!blockCounterValue(7,&i,result));i=-1;assert(!blockCounterValue(6,&i,result));
 d=.5;assert(!blockCounterValue(10,&d,result));d=std::numeric_limits<double>::quiet_NaN();assert(!blockCounterValue(10,&d,result));
 puts("PASS confirmed local block increment, first block, unchanged/failed/bulk writes, save reset, and numeric guards");
}
