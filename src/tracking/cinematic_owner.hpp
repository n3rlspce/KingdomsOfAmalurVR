#pragma once
#include <cstdint>
namespace amalur {
struct CinematicOwner {uintptr_t scene{},core{},owner{};};
// Build 10619381 CAMERA.is_cinematic_camera_active (A419A0). Active native
// camera ownership, not player initialization or a preloaded scene, selects VR.
template<class Read> bool cinematicOwner(uintptr_t base,Read word,CinematicOwner& out){
 const auto g=word(base+0x15fe9c4);if(!g)return false;
 const auto scenes=g+0x15a0;
 if(word(scenes)!=base+0x13483cc)return false;
 if(static_cast<int32_t>(word(g+0x165c))<2&&static_cast<int32_t>(word(g+0x1660))<2)return false;
 const auto windows=g+0x397c;
 if(word(windows)!=base+0x134e60c||word(windows+8)!=1)return false;
 const auto entries=word(windows+4);if(!entries)return false;
 const auto game=word(entries+4);if(!game)return false;
 if(word(game)!=base+0x1328914&&word(game)!=base+0x132ae7c)return false;
 const auto window=word(game+0x40c);
 if(!window||word(window)!=base+0x1326e9c)return false;
 const auto camera=word(window+0x410);
 if(!camera||word(camera)!=base+0x1335d08||word(camera+8)!=1)return false;
 const auto scene=word(scenes+0xd4);
 const auto identity=scene&&word(scene)==base+0x1348754?scene:window;
 if(word(window+0x410)!=camera||word(scenes+0xd4)!=scene)return false;
 out={identity,camera+8,window};return true;
}
}
