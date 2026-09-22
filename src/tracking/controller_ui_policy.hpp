#pragma once
#include <cstdint>
namespace amalur {
// Own only the native device-selection value, never buttons or navigation.
// Unknown modes and replaced managers must not be overwritten/restored.
struct ControllerUiPolicy {
    uintptr_t owner{};int saved{};bool leased=false;
    int update(uintptr_t current,int nativeMode,bool active){
        if(current!=owner){owner=current;leased=false;}
        if(!current||nativeMode<0||nativeMode>3||nativeMode==1){leased=false;return nativeMode;}
        if(active){
            if(nativeMode!=0){saved=nativeMode;leased=true;}
            return 0;
        }
        const int result=leased&&nativeMode==0?saved:nativeMode;
        leased=false;return result;
    }
};
}
