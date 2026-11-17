#pragma once
#include "sow_grunt_command.hpp"
namespace sow::command_target_cooldown {
namespace gc=grunt_command;
using PredicateFn=char(__fastcall*)(Address,Address);
using CooldownFn=Address(__fastcall*)(Address);
inline std::unique_ptr<PLH::x64Detour> hook;
inline std::uint64_t trampoline=0;
inline CooldownFn cooldown=nullptr;
inline std::atomic<unsigned long long> bypassed{0};
inline std::atomic<ULONGLONG> nextLog{0};
inline std::atomic<unsigned long long> predicateCalls{0},exactCalls{0},nativePasses{0},nativeDenied{0};
enum class Scope : unsigned {Allowed,Inactive,Predicate,API,Context,Untracked,Entity,Command,Registry,Link,Count};
inline std::array<std::atomic<unsigned long long>,static_cast<unsigned>(Scope::Count)> rejected{};
inline bool diagnostics=true;
inline void Report(const char* text) {
    gc::Log(text,0,0,true);
    if(!gc::consoleOutput && GetConsoleWindow()){
        std::lock_guard<std::mutex> lock(gc::logMutex);
        std::fprintf(stdout,"[GruntCommand] %s\n",text);std::fflush(stdout);
    }
}
struct PredicatePair {gc::Binding source{},target{};bool readable=false;};
inline PredicatePair ReadPair(Address query) {
    PredicatePair pair;
    // 30D1A4 constructs predicateContext+0 = attack-selection context,
    // +8 = temporary target descriptor, +10 = descriptor+28 target stats.
    // 2DA938 constructs selectionContext+0 = source stats. Query+8 is NOT stats.
    const auto selection=At<Address>(query,0),descriptor=At<Address>(query,8),targetStats=At<Address>(query,0x10);
    if(!selection.readable || !selection.value || !descriptor.readable || !descriptor.value || !targetStats.readable || !targetStats.value)return pair;
    const auto sourceStats=Read<Address>(selection.value),descriptorTarget=At<Address>(descriptor.value,0x28);
    if(!sourceStats.readable || !sourceStats.value || !descriptorTarget.readable || descriptorTarget.value!=targetStats.value)return pair;
    const auto sourceActor=At<Address>(sourceStats.value,0x28),targetActor=At<Address>(targetStats.value,0x28);
    if(!sourceActor.readable || !sourceActor.value || !targetActor.readable || !targetActor.value || sourceActor.value==targetActor.value)return pair;
    pair.source={sourceActor.value,sourceStats.value};pair.target={targetActor.value,targetStats.value};pair.readable=true;return pair;
}
inline Scope Evaluate(Address predicate,Address query) {
    if(!gc::crowdCommandsActive.load())return Scope::Inactive;
    if(!cooldown || !gc::native.entry)return Scope::API;
    if(!ReadRecord(predicate).Named("AI_Buddy_Command_Cooldown"))return Scope::Predicate;
    const auto pair=ReadPair(query);if(!pair.readable)return Scope::Context;
    for(const auto& signal:gc::orderSignals){
        if(signal.actor.load()!=pair.source.actor || signal.stats.load()!=pair.source.stats ||
           signal.target.load()!=pair.target.actor || signal.destroyed.load())continue;
        if(!gc::Friendly(pair.source) || !gc::Enemy(pair.target))return Scope::Entity;
        const auto bound=At<Address>(pair.source.actor,0x2EB8);
        const auto command=At<std::uint8_t>(pair.source.actor,0x2E90);
        if(!bound.readable || bound.value!=pair.target.stats || !command.readable || !command.value)return Scope::Command;
        const auto entry=gc::native.entry(pair.source.actor);
        const auto state=At<std::uint8_t>(entry,0x80);const auto owner=At<Address>(entry,0);
        if(!state.readable || state.value!=2 || !owner.readable || owner.value!=pair.source.actor ||
           !ReadRecord(At<Address>(entry,8).value).Named("Bodyguard_DefaultOrc"))return Scope::Registry;
        if(!ReadRecord(cooldown(predicate)).Named("AI_Buddy_Command_Cooldown"))return Scope::Link;
        return !signal.destroyed.load() && signal.actor.load()==pair.source.actor &&
            signal.stats.load()==pair.source.stats && signal.target.load()==pair.target.actor?Scope::Allowed:Scope::Untracked;
    }
    return Scope::Untracked;
}
inline bool ShouldBypass(Address predicate,Address query){return Evaluate(predicate,query)==Scope::Allowed;}
inline char __fastcall PredicateDetour(Address predicate,Address query) {
    const auto result=PLH::FnCast(trampoline,static_cast<PredicateFn>(nullptr))(predicate,query);
    if(!gc::crowdCommandsActive.load())return result;
    ++predicateCalls;
    if(!ReadRecord(predicate).Named("AI_Buddy_Command_Cooldown"))return result;
    ++exactCalls;
    if(result){++nativePasses;return result;}
    ++nativeDenied;const auto scope=Evaluate(predicate,query);
    if(scope!=Scope::Allowed){++rejected[static_cast<unsigned>(scope)];return result;}
    ++bypassed;return 1;
}
struct PlannerSample {
    gc::Binding source{},target{};float distanceSquared=INFINITY;
    Address ai=0,raw=0;unsigned kind=0,pending=0;
    int targetMatch=-1,visible=-1,attackable=-1,permission=-1,requestMatch=-1;
    bool complete=false;Record attack{};
};
inline PlannerSample ReadPlanner(gc::Binding source,gc::Binding target) {
    PlannerSample out;out.source=source;out.target=target;
    if(!gc::Friendly(source) || !gc::Enemy(target))return out;
    out.distanceSquared=gc::DistanceSquared(source.actor,target.actor);
    const auto ai=At<Address>(source.actor,0x2CE8),raw=At<Address>(source.stats,0x2E0);
    const auto rawActor=At<Address>(raw.value,0x2E8);
    if(!ai.readable || !ai.value || !raw.readable || !raw.value || !rawActor.readable || rawActor.value!=source.actor)return out;
    out.ai=ai.value;out.raw=raw.value;
    const auto selected=At<Address>(ai.value,0x28),request=At<Address>(ai.value,0x700);
    const auto visible=At<std::uint8_t>(ai.value,0xB8),attackable=At<std::uint8_t>(ai.value,0xB9),permission=At<std::uint8_t>(ai.value,0x710);
    const auto kind=At<unsigned>(ai.value,0x98),pending=At<unsigned>(raw.value,0x388);const auto attack=At<Address>(source.stats,0xE0);
    out.complete=selected.readable && request.readable && visible.readable && attackable.readable && permission.readable && kind.readable && pending.readable && attack.readable;
    if(!out.complete)return out;
    out.targetMatch=selected.value==target.stats;out.requestMatch=request.value==target.stats;
    out.visible=visible.value;out.attackable=attackable.value;out.permission=permission.value;out.kind=kind.value;out.pending=pending.value;
    out.attack=ReadRecord(attack.value);return out;
}
inline void AuditTick(ULONGLONG now) {
    if(!diagnostics || !gc::crowdCommandsActive.load())return;
    auto deadline=nextLog.load();if(now<deadline || !nextLog.compare_exchange_strong(deadline,now+1000))return;
    unsigned ordered=0,nearCount=0,attempted=0,complete=0,noPermission=0,notAttackable=0;
    gc::Binding nearestSource{},nearestTarget{};float nearestDistance=INFINITY;
    for(const auto& signal:gc::orderSignals){
        const auto actor=signal.actor.load(),stats=signal.stats.load(),target=signal.target.load();
        if(!actor || !stats || !target || signal.destroyed.load())continue;
        const auto targetStats=At<Address>(target,0x26A8);
        if(!targetStats.readable || !gc::Live({actor,stats}) || !gc::Live({target,targetStats.value}))continue;
        ++ordered;const auto distance=gc::DistanceSquared(actor,target);
        if(!std::isfinite(distance) || distance<0)continue;
        if(distance<nearestDistance){nearestDistance=distance;nearestSource={actor,stats};nearestTarget={target,targetStats.value};}
        if(distance<200.f*200.f){
            ++nearCount;
            if(attempted<8){
                ++attempted;const auto sample=ReadPlanner({actor,stats},{target,targetStats.value});
                if(sample.complete){++complete;if(!sample.permission)++noPermission;if(!sample.attackable)++notAttackable;}
            }
        }
    }
    const auto nearest=ReadPlanner(nearestSource,nearestTarget);
    const auto count=[](Scope scope){return rejected[static_cast<unsigned>(scope)].load();};
    char text[640]{};
    std::snprintf(text,sizeof(text),"cooldown audit batch=%llu totalPredicate=%llu exactCalls=%llu pass=%llu deny=%llu bypass=%llu badContext=%llu untracked=%llu entity=%llu command=%llu registry=%llu link=%llu ordered=%u near2m=%u nearSampled=%u nearRead=%u nearNoPermission=%u nearNotAttackable=%u",
        gc::crowdBatch.load(),predicateCalls.load(),exactCalls.load(),nativePasses.load(),nativeDenied.load(),bypassed.load(),count(Scope::Context),count(Scope::Untracked),count(Scope::Entity),count(Scope::Command),count(Scope::Registry),count(Scope::Link),ordered,nearCount,attempted,complete,noPermission,notAttackable);
    Report(text);
    if(nearest.complete){
        std::snprintf(text,sizeof(text),"planner sample source=%p target=%p distanceM=%.2f selectedMatch=%d kind=0x%X visible=%d attackable=%d permission=%d requestedMatch=%d pending=%u attack=%s",
            reinterpret_cast<void*>(nearest.source.actor),reinterpret_cast<void*>(nearest.target.actor),std::sqrt(nearest.distanceSquared)*.01f,nearest.targetMatch,nearest.kind,nearest.visible,nearest.attackable,nearest.permission,nearest.requestMatch,nearest.pending,nearest.attack.nameComplete?nearest.attack.name.data():"<none/unavailable>");
        Report(text);
    }
}
inline bool Install(bool log=true) {
    if(hook || !gc::updateHook)return false;
    const auto base=ring_might::Base();
    const auto signature=Read<std::array<std::uint8_t,14>>(base+0xED7420);
    const auto getter=Read<std::array<std::uint8_t,8>>(base+0xEC0040);
    if(!signature.readable || signature.value!=std::array<std::uint8_t,14>{0x40,0x53,0x48,0x83,0xEC,0x20,0x48,0x83,0x7A,0x08,0,0x48,0x8B,0xDA} ||
       !getter.readable || getter.value!=std::array<std::uint8_t,8>{0x48,0x89,0x6C,0x24,0x20,0x57,0x48,0x8B}){
        Report("command target cooldown signature mismatch; native behavior retained");return false;
    }
    cooldown=reinterpret_cast<CooldownFn>(base+0xEC0040);bypassed=0;nextLog=0;predicateCalls=0;exactCalls=0;nativePasses=0;nativeDenied=0;diagnostics=log;
    for(auto& count:rejected)count=0;
    hook=std::make_unique<PLH::x64Detour>(base+0xED7420,reinterpret_cast<Address>(&PredicateDetour),&trampoline);
    if(!hook->hook()){hook.reset();cooldown=nullptr;Report("command target cooldown hook failed");return false;}
    if(diagnostics)gc::orderAuditTick=&AuditTick;
    Report(diagnostics?"command target cooldown v2 installed; planner audit enabled":"command target cooldown v2 installed");return true;
}
inline bool Remove(){if(hook && !hook->unHook())return false;hook.reset();cooldown=nullptr;if(gc::orderAuditTick==&AuditTick)gc::orderAuditTick=nullptr;return true;}
} // namespace sow::command_target_cooldown

