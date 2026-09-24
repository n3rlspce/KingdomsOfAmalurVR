#pragma once
#include <cstdint>
#include <cmath>
#include <cstring>
namespace amalur {
inline constexpr uint32_t attacksBlockedHash=0x00d4e726;
// Native Variant getter6A1D70 proves integer type6 and double type10.
inline bool blockCounterValue(uint32_t type,const void* value,uint32_t& out){
 if(!value)return false;
 double number{};
 if(type==6){int32_t n;std::memcpy(&n,value,4);number=n;}
 else if(type==10)std::memcpy(&number,value,8);
 else return false;
 if(!std::isfinite(number)||number<0||number>2147483646.0||std::floor(number)!=number)return false;
 out=uint32_t(number);return true;
}
inline bool confirmedShieldBlock(uint32_t before,uint32_t requested,uint32_t after,bool local,bool scriptSetter){
 return local&&scriptSetter&&before<2147483646u&&requested==before+1&&after==requested;
}
}
