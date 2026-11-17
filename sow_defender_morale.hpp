#pragma once
#include "sow_defender_flee_trace.hpp"
#include "sow_defender_effects.hpp"
#include <limits>
namespace sow::defender_morale {
using LookupFn=Address(*)(const char*);
using HasFn=bool(__fastcall*)(Address,Address);
using RemoveFn=int(__fastcall*)(Address,Address,int,unsigned);
struct API {LookupFn lookup=nullptr;HasFn has=nullptr;RemoveFn remove=nullptr;};
inline API native;
inline bool enabled=false;
inline bool clearMorale=true,clearMercenaryEffects=false;
inline thread_local bool cleaning=false;
inline Address Lookup(const char* name){return ring_might::Lookup(0x2700530,name);}
inline bool Registered(Address actor,Address member) {
    Entity source;source.actor={actor,true};
    if(!defender_repair::DamageBlocked(source))return false;
    const auto snapshot=defender_repair::LoadThreats();
    for(const auto& t:*snapshot)if(t.actor==actor && t.member==member)return true;
    return false;
}
inline void Report(Address actor,Address member,const char* item,int before,int after,int removed) {
    char text[512]{};
    std::snprintf(text,sizeof(text),"[DefenderMorale] actor=%p member=%p item=%s ownedBefore=%d ownedAfter=%d removedCount=%d",
        reinterpret_cast<void*>(actor),reinterpret_cast<void*>(member),item,before,after,removed);
    if(defender_flee_trace::enabled)defender_flee_trace::Emit(text);
    else defender_repair::Log(text,member,"",true);
}
inline bool Cleanup(Address actor,Address member) {
    if(!enabled || !defender_repair::config.enabled || cleaning || !native.lookup || !native.has || !native.remove ||
       !Registered(actor,member))return false;
    const auto inventory=At<Address>(actor,0x2348);
    if(!inventory.readable || !inventory.value)return false;
    struct Scope{Scope(){cleaning=true;}~Scope(){cleaning=false;}} scope;
    if(clearMorale)for(const auto* name:{"Item_Quirk_Fearless","Item_Inspired_Morale_Buff","Item_Aura_ExtraMorale"}){
        const auto item=native.lookup(name);
        if(!item){Report(actor,member,name,-1,-1,0);return false;}
        const bool before=native.has(inventory.value,item);
        if(!before){Report(actor,member,name,0,0,0);continue;}
        // Native 1C0840 clamps requested count to current count (7FD964).
        // Remove all ordinary copies with one native call, never by pointer edits.
        const auto removed=native.remove(inventory.value,item,(std::numeric_limits<int>::max)(),0);
        const bool after=native.has(inventory.value,item);
        Report(actor,member,name,1,int(after),removed);
        if(after || removed<=0)return false;
    }
    unsigned removedItems=0,shared=0,unknown=0,failed=0;
    // A live body can inherit an immunity parent/child from class or preset
    // construction after the stored trait was removed. Strip those only for
    // registered flagged actors under the force-Awe weakening policy.
    if(clearMercenaryEffects && defender_repair::config.forceTerrorPlayer){
        const auto removeItem=[&](const char* name){
            if(!name)return true;if(!Registered(actor,member))return false;
            const auto item=native.lookup(name);if(!item){++unknown;Report(actor,member,name,-1,-1,0);return false;}
            if(!native.has(inventory.value,item))return true;
            const auto removed=native.remove(inventory.value,item,(std::numeric_limits<int>::max)(),0);
            const bool after=native.has(inventory.value,item);Report(actor,member,name,1,int(after),removed);
            if(after || removed<=0){++failed;return false;}++removedItems;return true;
        };
        for(const auto& effect:defender_vulnerability_policy::effects){
            if(!removeItem(effect.parent) || !removeItem(effect.leaf))return false;
            // Mortal parents may themselves select a resistant child while
            // weaknesses are unknown. Preserve that native info-state behavior.

        }
    }

    if(clearMercenaryEffects){
        // Re-evaluate class/title permissions after binding: removed stored
        // traits can still have native inventory effects. Allowed title/class
        // effects are retained even on a defender flagged for another reason.
        const auto ctx=defender_repair::ReadClassContext(member);
        static constexpr const char* invalidItems[][2]={
            {"Quirk_NoLastChance","Item_Quirk_NoLastChance"},
            {"Quirk_LifeSteal","Item_Quirk_Lifesteal"},
            {"Quirk_SecondWind","SecondWind"},
            {"Quirk_DyingCommand","DyingCommand"},
            {"Quirk_Summoner_Gang","Summoner_Gang"}};
        for(const auto& pair:invalidItems){
            Record trait;trait.nameComplete=true;
            std::snprintf(trait.name.data(),trait.name.size(),"%s",pair[0]);
            if(!defender_repair::OffClassTrait(trait,ctx))continue;
            if(!Registered(actor,member))return false;
            const auto item=native.lookup(pair[1]);
            if(!item){++unknown;Report(actor,member,pair[1],-1,-1,0);continue;}
            if(!native.has(inventory.value,item))continue;
            const auto removed=native.remove(inventory.value,item,(std::numeric_limits<int>::max)(),0);
            const bool after=native.has(inventory.value,item);
            Report(actor,member,pair[1],1,int(after),removed);
            if(after || removed<=0)++failed;else ++removedItems;
        }
    }
    if(clearMercenaryEffects)for(const auto& effect:effectItems){
        // These three are governed by the explicit, separately tested morale policy.
        if(std::strcmp(effect.name,"Item_Quirk_Fearless")==0 ||
           std::strcmp(effect.name,"Item_Inspired_Morale_Buff")==0 ||
           std::strcmp(effect.name,"Item_Aura_ExtraMorale")==0)continue;
        if(!Registered(actor,member))return false;
        const auto item=native.lookup(effect.name);
        if(!item){++unknown;Report(actor,member,effect.name,-1,-1,0);continue;}
        if(!native.has(inventory.value,item))continue;
        if(!effect.mercenaryOnly){
            ++shared;defender_repair::Log("shared trait item retained; provenance unverified",member,effect.name,true);continue;
        }
        const auto removed=native.remove(inventory.value,item,(std::numeric_limits<int>::max)(),0);
        const bool after=native.has(inventory.value,item);
        Report(actor,member,effect.name,1,int(after),removed);
        if(after || removed<=0)++failed;else ++removedItems;
    }
    if(clearMercenaryEffects){
        char summary[256]{};
        std::snprintf(summary,sizeof(summary),"effect cleanup removedItems=%u sharedRetained=%u lookupUnknown=%u failed=%u",removedItems,shared,unknown,failed);
        defender_repair::Log(summary,member,"",true);
    }
    return failed==0 && unknown==0;
}
inline void Bound(Address actor,Address member){Cleanup(actor,member);}
inline bool Install(bool requested=true,bool mercenaryEffects=false) {
    if(!requested && !mercenaryEffects)return true;
    if(enabled || defender_repair::onFlaggedBound)return false;
    const auto base=ring_might::Base();
    static constexpr unsigned char remove[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x54,0x24,0x10,0x55,0x56,0x57,0x41,0x54,0x41};
    static constexpr unsigned char has[]={0x48,0x83,0xec,0x28,0x48,0x8b,0xc1,0x48,0x85,0xd2,0x74,0x28};
    static constexpr unsigned char hash[]={0x0f,0xb6,0x01,0x84,0xc0,0x74,0x27};
    static constexpr unsigned char find[]={0x45,0x33,0xc9,0x4c,0x8b,0xd9,0x41,0x83,0xe8,0x01};
    if(!defender_repair::Signature(base+0x1C0840,remove) && !defender_flee_trace::removeHook)return false;
    if(!defender_repair::Signature(base+0x24DF7C,has) || !defender_repair::Signature(base+0x423E70,hash) ||
       !defender_repair::Signature(base+0x278540,find))return false;
    native={&Lookup,reinterpret_cast<HasFn>(base+0x24DF7C),reinterpret_cast<RemoveFn>(base+0x1C0840)};
    clearMorale=requested;clearMercenaryEffects=mercenaryEffects;
    enabled=true;defender_repair::onFlaggedBound=&Bound;
    defender_repair::Log("post-binding morale-item cleanup enabled; registered flagged actors only",0,"",true);return true;
}
inline bool Remove() {
    if(defender_repair::onFlaggedBound==&Bound)defender_repair::onFlaggedBound=nullptr;
    enabled=false;clearMercenaryEffects=false;native={};return true;
}
}
