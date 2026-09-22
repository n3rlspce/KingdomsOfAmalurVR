#pragma once
#include <cstdint>

namespace amalur {
// Native layout/call policy for the observed single-runtime basic dagger case.
// Access supplies read/write and the four verified game calls. Native Access
// must require the update thread and installed lifetime observers before enabled
// can return true. Returned indices are candidates, NOT lifetime tokens.
// These primitives are not the transactional MeleeContextScope Backend: after
// a native-call verification failure, that backend must quarantine/roll back
// the candidate using observed lifetime identity before attempting more work.
template<class Access>
class MeleeCreation {
public:
    explicit MeleeCreation(Access& access):a_(access){}
    struct Record {uintptr_t address{};bool tableValid{};};
    struct Candidate {uint32_t index{};uintptr_t address{};bool valid{};};
    Record record(uintptr_t part,uint32_t key)const{
        auto count=a_.read(part+0x28),records=a_.read(part+0x34);
        if(count>4096||a_.read(part+0x38)!=count)return {};
        auto keys=a_.read(part+0x24);
        if(count&&(!keys||!records))return {};
        uintptr_t found=0;
        for(uint32_t i=0;i<count;++i)if(a_.read(keys+i*4)==key){
            if(found)return {}; // Duplicate keys cannot establish ownership.
            found=records+i*24;
        }
        return {found,true};
    }
    bool empty(uintptr_t address)const{
        return address&&a_.read(address)==0xffffffffu&&a_.read(address+4)==0
            &&a_.read(address+12)==0;
    }
    bool reserve(uintptr_t part,uint32_t owner,uint32_t key){
        if(!a_.enabled()||!key||!component(part,owner))return false;
        auto before=record(part,key);
        if(!before.tableValid||before.address||a_.read(part+0x28)>=4096)return false;
        a_.reserve(part,key);
        // b837e0 may grow both vectors: never retain a record pointer across it.
        auto after=record(part,key);
        return after.tableValid&&empty(after.address)&&a_.claim(part,key);
    }
    bool bind(uintptr_t part,uint32_t owner,uint32_t key,uint32_t index){
        if(!a_.enabled()||!index||!component(part,owner)||!a_.owns(part,key))return false;
        auto before=record(part,key);
        if(!before.tableValid||!empty(before.address))return false;
        a_.bind(part,key,index);
        auto after=record(part,key);
        return after.tableValid&&after.address&&a_.read(after.address)==index
            &&a_.read(after.address+4)==0&&a_.read(after.address+12)==0;
    }
    bool unbind(uintptr_t part,uint32_t owner,uint32_t key,uint32_t index){
        if(!a_.enabled()||!index||!component(part,owner)||!a_.owns(part,key))return false;
        auto r=record(part,key);
        if(!r.tableValid||!r.address||a_.read(r.address)!=index
            ||a_.read(r.address+4)!=0||a_.read(r.address+12)!=0)return false;
        a_.write(r.address,0xffffffffu);
        return true;
    }
    bool erase(uintptr_t part,uint32_t owner,uint32_t key){
        if(!a_.enabled()||!component(part,owner)||!a_.owns(part,key))return false;
        auto r=record(part,key);
        if(!r.tableValid||!empty(r.address))return false;
        // b80020 frees runtimes too: allow only an already-empty owned record.
        a_.erase(part,key);
        auto after=record(part,key);
        return after.tableValid&&!after.address;
    }
    Candidate create(uintptr_t pool,uint32_t owner,uint32_t asset,uint32_t target){
        if(!a_.enabled()||!pool||!owner||asset<2)return {};
        auto index=a_.create(pool,asset,owner,target);
        // beb400 returns ZERO on initialization failure, despite one native
        // caller checking only for a negative result. Slot zero is reserved.
        if(!index)return {};
        auto count=a_.read(pool+8),table=a_.read(pool+4);
        if(index>=count||count>1048576||!table)return {index,0,false};
        auto runtime=a_.read(table+index*4);
        if(!runtime||!(a_.read(runtime+0x1c)&1)||a_.read(runtime+0x20)!=index
            ||a_.read(runtime+0x24)!=owner||a_.read(runtime+4)!=asset)return {index,runtime,false};
        return {index,runtime,a_.observed(runtime)};
    }
private:
    bool component(uintptr_t part,uint32_t owner)const{
        return part&&owner&&a_.read(part+0x18)==owner&&a_.read(part+0x1c)==18
            &&(a_.read(part+0x20)&1);
    }
    Access& a_;
};
}
