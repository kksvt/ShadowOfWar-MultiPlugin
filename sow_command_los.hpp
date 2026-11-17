#pragma once
#include "sow_grunt_command.hpp"

namespace sow::command_los {
namespace gc=grunt_command;
using RayFn=bool(__fastcall*)(Address,Address,Address*);
using FilterFn=bool(__fastcall*)(Address,Address);
inline std::unique_ptr<PLH::x64Detour> rayHook,filterHook;
inline std::uint64_t rayTrampoline=0,filterTrampoline=0;
inline Address gameBase=0;
inline bool enabled=false,diagnostics=false;
inline std::atomic<unsigned long long> queries{0},candidates{0},ignored{0},clearQueries{0},blockedQueries{0};
inline std::atomic<ULONGLONG> nextReport{0};
struct Scope {Address source=0,target=0;unsigned ignored=0;};
inline thread_local Scope* current=nullptr;
inline bool IsAvailabilityCaller(Address caller) {return caller==gameBase+0x2E5197;}
inline bool CommandPair(Address source,Address targetStats,gc::Binding& target) {
    if(!enabled || !gc::crowdCommandsActive.load())return false;
    const auto actor=At<Address>(targetStats,0x28);
    if(!actor.readable || !gc::Enemy({actor.value,targetStats}))return false;
    for(const auto& signal:gc::orderSignals) {
        if(signal.actor.load()!=source || signal.destroyed.load() || signal.target.load()!=actor.value)continue;
        if(gc::Friendly({source,signal.stats.load()})){target={actor.value,targetStats};return true;}
    }
    return false;
}
inline bool IgnoreHit(Address hit,Address data,const Scope& scope) {
    if(!hit || hit==scope.source || hit==scope.target || At<Address>(data,0).value!=scope.source)return false;
    // Require the hit pointer itself to be a verified, live commanded grunt.
    // No generic physics-to-entity native call and no guess from a pool index.
    for(const auto& signal:gc::orderSignals) {
        if(signal.actor.load()!=hit || signal.destroyed.load() || signal.target.load()!=scope.target)continue;
        return gc::Friendly({hit,signal.stats.load()});
    }
    return false;
}
inline bool __fastcall FilterDetour(Address hit,Address data) {
    const bool accepted=PLH::FnCast(filterTrampoline,static_cast<FilterFn>(nullptr))(hit,data);
    if(!accepted || !current || !enabled)return accepted;
    ++candidates;
    if(!IgnoreHit(hit,data,*current))return accepted;
    ++ignored;++current->ignored;return false; // Native filter false excludes a ray candidate.
}
inline bool RunQuery(Address source,Address targetStats,Address* blocker,bool tracked,gc::Binding target) {
    Scope scope{source,target.actor};auto* previous=current;
    // A nested unrelated query must not inherit the outer command's exemption.
    current=tracked?&scope:nullptr;
    if(tracked)++queries;
    const bool result=PLH::FnCast(rayTrampoline,static_cast<RayFn>(nullptr))(source,targetStats,blocker);
    current=previous;
    if(tracked){if(result)++clearQueries;else ++blockedQueries;}
    return result;
}
inline bool __fastcall RayDetour(Address source,Address targetStats,Address* blocker) {
    gc::Binding target;
    const bool tracked=IsAvailabilityCaller(reinterpret_cast<Address>(_ReturnAddress())) && CommandPair(source,targetStats,target);
    return RunQuery(source,targetStats,blocker,tracked,target);
}
inline void Report(Address target) {
    if(!diagnostics)return;
    const auto now=GetTickCount64();auto until=nextReport.load();
    if(now<until || !nextReport.compare_exchange_strong(until,now+1000))return;
    char text[320]{};
    std::snprintf(text,sizeof(text),"command-los tick=%llu batch=%llu installed=%d queries=%llu filterCandidates=%llu ignoredFriendlyCandidates=%llu clearQueries=%llu blockedQueries=%llu",
        static_cast<unsigned long long>(now),gc::crowdBatch.load(),enabled,queries.load(),candidates.load(),ignored.load(),clearQueries.load(),blockedQueries.load());
    gc::Log(text,0,target,true);
}
inline bool Remove() {
    bool ok=true;
    for(auto* hook:{&filterHook,&rayHook}) {
        if(*hook && !(*hook)->unHook()){ok=false;continue;}
        hook->reset();
    }
    if(ok)enabled=false;
    return ok;
}
inline bool Install(bool log=false) {
    if(rayHook || filterHook || !gc::updateHook)return false;
    gameBase=ring_might::Base();diagnostics=log;
    const auto ray=Read<std::array<std::uint8_t,10>>(gameBase+0x1A65498);
    const auto filter=Read<std::array<std::uint8_t,12>>(gameBase+0x1A656E0);
    if(!ray.readable || ray.value!=std::array<std::uint8_t,10>{0x48,0x89,0x5C,0x24,0x10,0x48,0x89,0x74,0x24,0x18} ||
       !filter.readable || filter.value!=std::array<std::uint8_t,12>{0x40,0x53,0x48,0x83,0xEC,0x20,0x48,0x8B,0xDA,0x4C,0x8B,0xC1}) {
        gc::Log("command LOS signature mismatch; native visibility unchanged",0,0,true);return false;
    }
    rayHook=std::make_unique<PLH::x64Detour>(gameBase+0x1A65498,reinterpret_cast<Address>(&RayDetour),&rayTrampoline);
    filterHook=std::make_unique<PLH::x64Detour>(gameBase+0x1A656E0,reinterpret_cast<Address>(&FilterDetour),&filterTrampoline);
    if(!rayHook->hook()){rayHook.reset();filterHook.reset();return false;}
    if(!filterHook->hook()){filterHook.reset();Remove();return false;}
    queries=0;candidates=0;ignored=0;clearQueries=0;blockedQueries=0;nextReport=0;enabled=true;
    gc::Log("command LOS trial installed; same-command friendly grunts ignored by visibility filter; walls and other native checks retained",0,0,true);
    return true;
}
} // namespace sow::command_los
