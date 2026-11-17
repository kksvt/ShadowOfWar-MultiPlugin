#pragma once
#include "sow_grunt_command.hpp"
namespace sow::close_command_attack {
namespace gc=grunt_command;
using GateFn=bool(__fastcall*)(Address,Address,Address,float,unsigned,float*,Address);
inline std::unique_ptr<PLH::x64Detour> hook;
inline std::uint64_t trampoline=0;
inline bool diagnostics=false;
inline std::atomic<unsigned long long> forced{0};
inline std::atomic<ULONGLONG> nextLog{0};
inline bool WithinRange(float squared) noexcept {return std::isfinite(squared) && squared>=0 && squared<200.f*200.f;}
inline bool __fastcall ShouldForce(Address raw,Address circle,Address context,Address role) noexcept {
    if(!gc::crowdCommandsActive.load() || !ReadRecord(role).Named("Buddy_Commanded"))return false;
    const auto first=At<Address>(raw,0x340),second=At<Address>(raw,0x348);
    if(!first.readable || !second.readable)return false;
    const auto owner=first.value?first.value:second.value;
    if(owner && owner!=context)return false;
    const auto actor=At<Address>(raw,0x2E8),targetStats=At<Address>(circle,0x38);
    if(!actor.readable || !targetStats.readable)return false;
    const auto targetActor=At<Address>(targetStats.value,0x28);
    if(!targetActor.readable || !gc::Enemy({targetActor.value,targetStats.value}))return false;
    if(!WithinRange(gc::DistanceSquared(actor.value,targetActor.value)))return false;
    bool match=false;
    for(const auto& signal:gc::orderSignals)
        if(signal.actor.load()==actor.value && !signal.destroyed.load() && signal.target.load()==targetActor.value &&
           gc::Friendly({actor.value,signal.stats.load()})){match=true;break;}
    if(!match)return false;
    ++forced;
    if(diagnostics) {
        const auto now=GetTickCount64();auto until=nextLog.load();
        if(now>=until && nextLog.compare_exchange_strong(until,now+1000)) {
            char text[160]{};
            std::snprintf(text,sizeof(text),"full eligibility override range<2m count=%llu",forced.load());
            gc::Log(text,actor.value,targetActor.value,true);
        }
    }
    return true;
}
inline bool __fastcall GateDetour(Address raw,Address role,Address circle,float distance,unsigned population,float* score,Address context) {
    if(score && ShouldForce(raw,circle,context,role)) {
        *score=1.f; // Finite positive candidate score; no stale/uninitialized output.
        return true;
    }
    return PLH::FnCast(trampoline,static_cast<GateFn>(nullptr))(raw,role,circle,distance,population,score,context);
}
inline bool Install(bool log=false) {
    if(hook || !gc::updateHook)return false;
    diagnostics=log;forced=0;nextLog=0;
    const auto base=ring_might::Base();
    const auto signature=Read<std::array<std::uint8_t,8>>(base+0x37A2C4);
    if(!signature.readable || signature.value!=std::array<std::uint8_t,8>{0x48,0x8B,0xC4,0x48,0x89,0x58,0x18,0x48}) {
        gc::Log("full close eligibility signature mismatch; native behavior retained",0,0,true);return false;
    }
    hook=std::make_unique<PLH::x64Detour>(base+0x37A2C4,reinterpret_cast<Address>(&GateDetour),&trampoline);
    if(!hook->hook()){hook.reset();return false;}
    gc::Log("full close eligibility override installed; ordered grunts at distance<2m; native gate bypassed",0,0,true);
    return true;
}
inline bool Remove(){if(hook && !hook->unHook())return false;hook.reset();return true;}
} // namespace sow::close_command_attack
