#pragma once
#include "hud_settings.hpp"
namespace amalur {
// Separate channel: prototype state must not alter tracking/hand packet ABI.
class MapPanelSettings {
    HudSettingsChannel channel;
public:
    MapPanelSettings(const wchar_t* name=L"Local\\AmalurMapPanelV1",const wchar_t* mutex=L"Local\\AmalurMapPanelMutexV1"):channel(name,mutex){}
    void publish(bool enabled){if(channel.open(true))channel.publish(enabled?1.f:.4f);}
    bool read(){float value=.4f;return channel.open(false)&&channel.read(value)&&value>.5f;}
};
}
