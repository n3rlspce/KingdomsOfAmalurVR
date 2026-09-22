#pragma once
#include <windows.h>
#include <mutex>
#ifndef AMALUR_ATTACK_PANEL_MAPPING
#define AMALUR_ATTACK_PANEL_MAPPING L"Local\\AmalurAttackDetectionPanelHiddenV1"
#endif
namespace amalur {
// Independent visibility switch; never changes combat or collision settings.
// Preserve the bridge's wire format: zero visible, one hidden. The diagnostic
// starts each game session hidden once; subsequent opens never undo the user's
// developer-panel choice. No bridge rebuild or mapping-name change required.
class AttackPanelSettings {
    HANDLE mapping{};volatile LONG* hidden{};
public:
    AttackPanelSettings()=default;
    AttackPanelSettings(const AttackPanelSettings&)=delete;
    AttackPanelSettings& operator=(const AttackPanelSettings&)=delete;
    ~AttackPanelSettings(){if(hidden)UnmapViewOfFile(const_cast<LONG*>(hidden));if(mapping)CloseHandle(mapping);}
    bool open(){
        if(hidden)return true;
        mapping=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(LONG),AMALUR_ATTACK_PANEL_MAPPING);
        if(mapping)hidden=static_cast<volatile LONG*>(MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(LONG)));
        if(!hidden&&mapping){CloseHandle(mapping);mapping=nullptr;}
        if(hidden){static std::once_flag startup;std::call_once(startup,[&]{InterlockedExchange(hidden,1);});}
        return hidden!=nullptr;
    }
    bool enabled(){return open()&&InterlockedCompareExchange(hidden,0,0)==0;}
    void toggle(){if(open()){LONG old=InterlockedCompareExchange(hidden,0,0);for(;;){LONG seen=InterlockedCompareExchange(hidden,old?0:1,old);if(seen==old)return;old=seen;}}}
};
}
