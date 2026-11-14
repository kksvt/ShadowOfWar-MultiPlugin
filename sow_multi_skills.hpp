#pragma once
#include "sow_ring_might.hpp"
#include "sow_multi_skill_data.hpp"
#include <cstdio>
#include <atomic>
namespace sow::multi_skills {
using ActivateFn=bool(__fastcall*)(Address,Address);
using HasFn=bool(__fastcall*)(Address,Address);
using AddFn=void(__fastcall*)(Address,Address,unsigned);
using LookupFn=Address(*)(const char*);
using PlayerFn=Address(*)(Address);
inline Address PlayerInventory(Address inventory) {
    const auto stats=At<Address>(inventory,0x440);
    const auto actor=At<Address>(stats.value,0x28);
    const auto back=At<Address>(actor.value,0x26A8);
    const auto inv=At<Address>(actor.value,0x2348);
    return stats.readable && actor.readable && back.readable && back.value==stats.value &&
        inv.readable && inv.value==inventory && ring_might::IsPlayer(stats.value)?stats.value:0;
}
inline Address Lookup(const char* name){return ring_might::Lookup(0x2700530,name);}
struct API { ActivateFn activate=nullptr;HasFn has=nullptr;AddFn add=nullptr;LookupFn lookup=&Lookup;PlayerFn player=&PlayerInventory; };
inline API native;
inline std::unique_ptr<PLH::x64Detour> hook;
inline std::uint64_t trampoline=0;
inline thread_local bool handling=false;
inline bool diagnostics=true;
inline std::atomic<unsigned> lines{0};
inline void(*onActivated)(const char*)=nullptr;
inline const Spec* Find(const char* name) {
    for(const auto& s:specs)if(std::strcmp(name,s.skill)==0)return &s;
    return nullptr;
}
inline void Log(const char* skill,unsigned retained,const char* event="activated") {
    if(!diagnostics || lines.fetch_add(1)>=128)return;
    char message[512]{};std::snprintf(message,sizeof(message),"[MultiSkills] event=%s selected=%s restoredSiblingEffects=%u\n",event,skill,retained);
    OutputDebugStringA(message);
    if(GetConsoleWindow()){std::fputs(message,stdout);std::fflush(stdout);}
}
struct Keep {Address item=0,unlock=0;};
inline bool __fastcall Activate(Address inventory,Address skillRecord) {
    if(handling || !native.activate || !native.has || !native.add || !native.lookup || !native.player)
        return native.activate?native.activate(inventory,skillRecord):false;
    const auto name=ReadRecord(skillRecord);
    const auto selected=name.nameComplete?Find(name.name.data()):nullptr;
    const auto player=selected?native.player(inventory):0;
    const auto unlock=player?native.lookup(selected->unlock):0;
    if(!unlock || !native.has(inventory,unlock)) {
        if(selected)Log(selected->skill,0,player?"unlock-not-owned-passthrough":"inventory-not-bound-passthrough");
        return native.activate(inventory,skillRecord);
    }
    struct Scope {Scope(){handling=true;}~Scope(){handling=false;}} scope;
    std::array<Keep,12> keep{};std::size_t count=0;
    for(const auto& sibling:specs) {
        if(&sibling==selected || std::strcmp(sibling.parent,selected->parent)!=0)continue;
        const auto owned=native.lookup(sibling.unlock);
        if(!owned || !native.has(inventory,owned))continue;
        for(const auto* itemName:sibling.items) {
            if(!itemName)continue;
            const auto item=native.lookup(itemName);
            if(item && native.has(inventory,item) && count<keep.size())keep[count++]={item,owned};
        }
    }
    // Native purchase/prerequisite/selection behavior runs unchanged first.
    const bool result=native.activate(inventory,skillRecord);
    unsigned restored=0;
    if(result && native.player(inventory)==player && native.has(inventory,unlock)) {
        for(std::size_t i=0;i<count;++i) {
            const auto& k=keep[i];
            if(native.has(inventory,k.unlock) && !native.has(inventory,k.item)) {
                native.add(inventory,k.item,1);
                if(native.has(inventory,k.item))++restored;
            }
        }
        if(onActivated)onActivated(selected->skill);
        Log(selected->skill,restored);
    }
    return result;
}
inline bool Remove() {
    if(hook && !hook->unHook())return false;
    hook.reset();native.activate=nullptr;return true;
}
inline bool Install(bool log=true) {
    if(hook)return false;
    diagnostics=log;lines=0;const auto base=ring_might::Base();
    native={nullptr,reinterpret_cast<HasFn>(base+0x24DF7C),reinterpret_cast<AddFn>(base+0x1C01E0)};
    // Full activation wrapper calls the large native selection routine; unlike
    // the withdrawn v6 leaf-getter detour, its caller sites use the normal ABI.
    const auto bytes=Read<std::array<unsigned char,12>>(base+0x1BC8C6C);
    constexpr std::array<unsigned char,12> expected{0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9,0x48,0x8b,0xca};
    if(!bytes.readable || bytes.value!=expected)return false;
    hook=std::make_unique<PLH::x64Detour>(base+0x1BC8C6C,reinterpret_cast<Address>(&Activate),&trampoline);
    if(!hook->hook()){hook.reset();return false;}
    native.activate=PLH::FnCast(trampoline,static_cast<ActivateFn>(nullptr));
    if(log && GetConsoleWindow())std::puts("[MultiSkills] installed; preserve already-equipped purchased sibling upgrades");
    return true;
}
} // namespace sow::multi_skills
