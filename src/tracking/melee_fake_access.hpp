#pragma once
#include "melee_creation.hpp"
#include <cstdio>
#include <cstdlib>
#include <map>

inline void check(bool ok,const char* message){if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
struct FakeAccess {
    bool allowed=true,failCreate=false,reuseRuntime=false,expiredObservation=false;
    unsigned mutations{},reads{};
    uint32_t claimedKey{};
    std::map<uintptr_t,uint32_t> memory;
    static constexpr uintptr_t part=0x1000,keys=0x2000,records=0x3000,pool=0x4000,table=0x5000,runtime=0x6000;
    FakeAccess(){
        memory={{part+0x18,7},{part+0x1c,18},{part+0x20,1},{part+0x24,keys},
            {part+0x28,1},{part+0x34,records},{part+0x38,1},{keys,10},
            {records,11},{records+4,0},{records+12,0},{pool+4,table},{pool+8,2}};
    }
    bool enabled()const{return allowed;}
    bool claim(uintptr_t p,uint32_t key){check(p==part,"claim correct component");claimedKey=key;return true;}
    bool owns(uintptr_t p,uint32_t key)const{return p==part&&key==claimedKey&&key!=0;}
    bool observed(uintptr_t address)const{return address==runtime&&!expiredObservation;}
    uint64_t keySerial(uintptr_t p,uint32_t key)const{return owns(p,key)?1:0;}
    uint64_t observedSerial(uintptr_t address)const{return observed(address)?1:0;}
    uint32_t read(uintptr_t address){++reads;check(memory.count(address)!=0,"read only verified native fields");return memory.at(address);}
    void write(uintptr_t address,uint32_t value){++mutations;memory[address]=value;}
    void reserve(uintptr_t p,uint32_t key){
        ++mutations;check(p==part,"reserve uses component");
        // Simulate native vector growth. The previous record address is invalid.
        memory[part+0x34]=records+0x100;
        memory[records+0x100]=11;memory[records+0x104]=0;memory[records+0x10c]=0;
        memory.erase(records);memory.erase(records+4);memory.erase(records+12);
        memory[keys+4]=key;memory[part+0x28]=memory[part+0x38]=2;
        memory[records+0x118]=0xffffffffu;memory[records+0x11c]=0;memory[records+0x124]=0;
    }
    void bind(uintptr_t p,uint32_t key,uint32_t index){
        ++mutations;check(p==part&&memory[keys+4]==key,"bind key exists");memory[records+0x118]=index;
    }
    void erase(uintptr_t p,uint32_t key){
        ++mutations;check(p==part&&memory[keys+4]==key,"erase correct key");
        check(memory[records+0x118]==0xffffffffu,"erase cannot free a bound runtime");
        memory[part+0x28]=memory[part+0x38]=1;
        claimedKey=0;
    }
    uint32_t create(uintptr_t p,uint32_t asset,uint32_t owner,uint32_t target){
        ++mutations;check(p==pool&&asset==199&&owner==7&&target==0,"native creation arguments preserved");
        if(failCreate)return 0;
        memory[table+4]=runtime;memory[runtime+4]=asset;memory[runtime+0x1c]=1;
        memory[runtime+0x20]=1;memory[runtime+0x24]=reuseRuntime?8:owner;
        return 1;
    }
};
