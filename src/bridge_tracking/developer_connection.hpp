#pragma once
#include <cstdint>

namespace amalur {
// Only the passive Connect request may retry automatically, never game actions.
class DeveloperConnection {
    uint32_t process_{};
    uint64_t nextAttempt_{};
public:
    bool due(uint32_t process,bool ready,bool busy,bool connected,uint64_t now){
        if(!process)return false;
        if(process!=process_){process_=process;nextAttempt_=0;}
        if(!ready||busy||connected||now<nextAttempt_)return false;
        nextAttempt_=now+5000;
        return true;
    }
};
}
