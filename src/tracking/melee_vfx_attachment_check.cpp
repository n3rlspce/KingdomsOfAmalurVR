#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "melee_vfx_attachment.hpp"
void ck(bool value,const char* name){if(!value){std::printf("FAIL %s\n",name);std::exit(1);}}
int main(){
 uint32_t aliases[]{0x858053,0xceac76,0x123456};int16_t indices[]{6,3,1};
 ck(trailAttachmentIndex(1520,0,7,3,aliases,indices)==6,"captured right native attachment");
 ck(trailAttachmentIndex(1520,1,7,3,aliases,indices)==3,"captured left native attachment");
 ck(trailAttachmentIndex(5457,1,7,3,aliases,indices)<0,"no left sword alias inference");
 ck(trailAttachmentIndex(1520,2,7,3,aliases,indices)<0,"invalid hand");
 indices[1]=6;ck(trailAttachmentIndex(1520,1,7,3,aliases,indices)<0,"left cannot attach right branch");
 indices[1]=3;aliases[2]=0xceac76;ck(trailAttachmentIndex(1520,1,7,3,aliases,indices)<0,"duplicate alias");
 aliases[2]=0x123456;indices[1]=-1;ck(trailAttachmentIndex(1520,1,7,3,aliases,indices)<0,"negative bone");
 indices[1]=3;ck(trailAttachmentIndex(1520,1,6,3,aliases,indices)<0,"wrong skeleton size");
 ck(trailAttachmentIndex(1520,1,7,1,aliases,indices)<0,"missing alias");
 indices[0]=3;ck(trailAttachmentIndex(5457,0,4,3,aliases,indices)==3,"existing single sword alias");
 std::puts("PASS captured left/right trail attachment and fail-closed mismatches");
}
