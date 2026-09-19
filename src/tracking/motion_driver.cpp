// Bounded desktop test driver for the mod's private XInput channel.
// Run with the XR bridge stopped: buttons(hex) abilities durationMs moveX moveY block.
#include "motion_input.hpp"
#include <cstdlib>
#include <cstdio>
int main(int argc,char** argv){
    if(argc<4){std::puts("Usage: motion_driver BUTTONS_HEX ABILITIES DURATION_MS [MOVE_X MOVE_Y BLOCK]");return 2;}
    amalur::MotionInputPacket p;p.active=1;p.buttons=std::strtoul(argv[1],nullptr,16);p.abilities=std::strtof(argv[2],nullptr);
    unsigned duration=std::strtoul(argv[3],nullptr,10);if(argc>4)p.moveX=std::strtof(argv[4],nullptr);if(argc>5)p.moveY=std::strtof(argv[5],nullptr);if(argc>6)p.block=std::strtof(argv[6],nullptr);
    p.tick=GetTickCount64();if(duration>10000||!amalur::validMotionInput(p,p.tick))return 2;
    amalur::MotionInputChannel channel;if(!channel.open(true))return 3;
    auto until=GetTickCount64()+duration;while(GetTickCount64()<until){channel.publish(p);Sleep(8);}
    channel.publish({});return 0;
}
