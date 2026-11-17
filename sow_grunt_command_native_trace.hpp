#pragma once
#include "sow_grunt_command.hpp"
#include "sow_command_los.hpp"

// Steam 1.21 diagnostic only. These detours observe existing native calls;
// they do not issue combat calls or alter their arguments/results.
namespace sow::grunt_command_native_trace {
namespace gc=grunt_command;
using PickFn=Address(__fastcall*)(Address,Address,Address,bool);
using AllocateFn=void(__fastcall*)(Address,Address,Address,Address);
inline std::unique_ptr<PLH::x64Detour> pickHook,allocateHook;
inline std::uint64_t pickTrampoline=0,allocateTrampoline=0;
inline std::atomic<unsigned long long> pickCalls{0},allocationCalls{0};
inline std::atomic<ULONGLONG> nextPick{0},nextAllocation{0},nextHeartbeat{0};
// Only one custom command batch is active at a time. A picker context is a
// diagnostic correlation, not authority for any game-memory write.
inline std::atomic<Address> lastContext{0},lastTarget{0},lastStats{0};
inline std::atomic<unsigned long long> lastBatch{0};
inline thread_local gc::Binding pickerTarget{};
inline thread_local Address pickerCircle=0;
inline bool Relevant(Address circle,gc::Binding& target) {
    if(!gc::crowdCommandsActive.load())return false;
    const auto stats=At<Address>(circle,0x38);
    const auto actor=At<Address>(stats.value,0x28);
    if(!stats.readable || !actor.readable || !gc::Live({actor.value,stats.value}))return false;
    for(const auto& signal:gc::orderSignals)
        if(signal.actor.load() && !signal.destroyed.load() && signal.target.load()==actor.value) {
            target={actor.value,stats.value};return true;
        }
    return false;
}
inline bool Sample(std::atomic<ULONGLONG>& timer) {
    const auto now=GetTickCount64();auto until=timer.load();
    return now>=until && timer.compare_exchange_strong(until,now+1000);
}
struct RoleSummary {unsigned roles=0,buddyLists=0,buddyCandidates=0;bool complete=false;};
inline RoleSummary ReadRoles(Address context) {
    RoleSummary result;
    const auto begin=At<Address>(context,0x18),end=At<Address>(context,0x20);
    if(!begin.readable || !end.readable || end.value<begin.value ||
       (end.value-begin.value)%0x160 || (end.value-begin.value)/0x160>128)return result;
    result.roles=static_cast<unsigned>((end.value-begin.value)/0x160);
    for(unsigned i=0;i<result.roles;++i) {
        const auto entry=begin.value+Address(i)*0x160;
        const auto role=At<Address>(entry,0);
        if(!role.readable)return result;
        if(!ReadRecord(role.value).Named("Buddy_Commanded"))continue;
        ++result.buddyLists;
        const auto first=At<Address>(entry,8),last=At<Address>(entry,0x10);
        if(!first.readable || !last.readable || last.value<first.value ||
           (last.value-first.value)%0x30 || (last.value-first.value)/0x30>4096)return result;
        result.buddyCandidates+=static_cast<unsigned>((last.value-first.value)/0x30);
    }
    result.complete=true;return result;
}
struct AllocationSummary {unsigned count=0,commanded=0;bool complete=false;};
inline AllocationSummary ReadAllocation(Address stage,Address target) {
    AllocationSummary result;const auto count=At<unsigned>(stage,0x10);
    if(!count.readable || count.value>16)return result;
    result.count=count.value;
    for(unsigned i=0;i<count.value;++i) {
        const auto stats=At<Address>(stage,0x38+Address(i)*0x90);
        if(!stats.readable)return result;
        for(const auto& signal:gc::orderSignals)
            if(signal.actor.load() && !signal.destroyed.load() && signal.target.load()==target &&
               signal.stats.load()==stats.value){++result.commanded;break;}
    }
    result.complete=true;return result;
}
inline Address __fastcall PickDetour(Address context,Address circle,Address third,bool flag) {
    gc::Binding target;const bool relevant=Relevant(circle,target);
    if(relevant) {
        lastContext=0;lastTarget=target.actor;lastStats=target.stats;lastBatch=gc::crowdBatch.load();lastContext=context;
        ++pickCalls;
    }
    const auto previousTarget=pickerTarget;const auto previousCircle=pickerCircle;
    pickerTarget=relevant?target:gc::Binding{};pickerCircle=relevant?circle:0;
    const auto result=PLH::FnCast(pickTrampoline,static_cast<PickFn>(nullptr))(context,circle,third,flag);
    pickerTarget=previousTarget;pickerCircle=previousCircle;
    if(relevant && Sample(nextPick)) {
        const auto roles=ReadRoles(context);
        const auto record=ReadRecord(At<Address>(result,0).value);
        char text[512]{};
        std::snprintf(text,sizeof(text),
            "native-picker tick=%llu batch=%llu calls=%llu context=%p selected=%p play=%s roles=%u buddyLists=%u buddyCandidates=%u roleReadComplete=%d",
            static_cast<unsigned long long>(GetTickCount64()),gc::crowdBatch.load(),pickCalls.load(),reinterpret_cast<void*>(context),
            reinterpret_cast<void*>(result),record.nameComplete?record.name.data():"<none/unavailable>",
            roles.roles,roles.buddyLists,roles.buddyCandidates,roles.complete);
        gc::Log(text,0,target.actor,true);
    }
    return result;
}
inline void __fastcall AllocateDetour(Address stage,Address play,Address context,Address node) {
    // At 425C3C, R8 is the same selection context passed as RCX to 11D618.
    // Only correlate a current command target with its latest observed picker.
    const auto target=lastTarget.load(),stats=lastStats.load();
    const bool relevant=gc::crowdCommandsActive.load() && lastBatch.load()==gc::crowdBatch.load() && context==lastContext.load() &&
        gc::Live({target,stats}) && ReadRecord(play).Named("Buddy_Commanded");
    if(relevant)++allocationCalls;
    PLH::FnCast(allocateTrampoline,static_cast<AllocateFn>(nullptr))(stage,play,context,node);
    if(relevant && Sample(nextAllocation)) {
        const auto roles=ReadRoles(context);const auto assigned=ReadAllocation(stage,target);
        char text[512]{};
        std::snprintf(text,sizeof(text),
            "native-allocation tick=%llu batch=%llu calls=%llu context=%p stage=%p buddyCandidates=%u roleReadComplete=%d assigned=%u commandedAssigned=%u allocationReadComplete=%d",
            static_cast<unsigned long long>(GetTickCount64()),gc::crowdBatch.load(),allocationCalls.load(),reinterpret_cast<void*>(context),
            reinterpret_cast<void*>(stage),roles.buddyCandidates,roles.complete,assigned.count,assigned.commanded,assigned.complete);
        gc::Log(text,0,target,true);
    }
}
// Candidate filter: five arguments, original return ignored by 11D91C.
using CandidateFn=void(__fastcall*)(Address,Address,Address,unsigned,Address);
// Final gate uses XMM3 for distance, then three stack arguments.
using GateFn=bool(__fastcall*)(Address,Address,Address,float,unsigned,float*,Address);
using ModeFn=void(__fastcall*)(Address,Address,std::uint8_t,Address,Address);
inline std::unique_ptr<PLH::x64Detour> candidateHook,gateHook,modeHook;
inline std::uint64_t candidateTrampoline=0,gateTrampoline=0,modeTrampoline=0;
inline std::atomic<unsigned long long> observedCandidateCalls{0},buddyRoleCalls{0},unmatchedCandidateCalls{0};
inline std::atomic<unsigned long long> candidateCalls{0},acceptedCandidates{0},prefilterRejects{0},gateRejects{0};
inline std::atomic<ULONGLONG> nextRejection{0};
inline std::atomic<unsigned long long> otherContextRejects{0},ownContextCalls{0},ownContextRejects{0},ownContextAccepted{0};
enum class OwnerClass {Unreadable,Unrestricted,Own,Other};
struct OwnerContext {Address first=0,second=0;OwnerClass kind=OwnerClass::Unreadable;};
inline OwnerContext ReadOwnerContext(Address raw,Address context) {
    const auto first=At<Address>(raw,0x340),second=At<Address>(raw,0x348);
    if(!first.readable || !second.readable)return {};
    const auto chosen=first.value?first.value:second.value;
    return {first.value,second.value,!chosen?OwnerClass::Unrestricted:(chosen==context?OwnerClass::Own:OwnerClass::Other)};
}
struct CandidateFrame {Address raw=0;bool gateReached=false,gateAllowed=false;
    unsigned population=0;Field<std::uint8_t> f441{},f442{};
    bool inGate=false,memoryCaptured=false,memoryComplete=false;unsigned memoryBlocks=0;
    std::array<int,6> subchecks{-1,-1,-1,-1,-1,-1};
    std::array<Address,6> subcheckInputs{};
    OwnerContext ownership{};Address context=0;};
inline thread_local CandidateFrame* currentCandidate=nullptr;
// 11DA18 / 37A2C4 receive a combat candidate object, not actor stats.
// Native code follows raw+2E8 to the actor and then actor+26A8 to stats.
inline bool MatchSource(Address raw,Address target,gc::Binding& source) {
    const auto owner=At<Address>(raw,0x2E8);
    if(!owner.readable || !owner.value)return false;
    for(const auto& signal:gc::orderSignals) {
        const auto actor=signal.actor.load(),stats=signal.stats.load();
        if(actor!=owner.value || signal.destroyed.load() || signal.target.load()!=target)continue;
        if(gc::Live({actor,stats})){source={actor,stats};return true;}
    }
    return false;
}
inline Field<unsigned> CandidateCount(Address roleList) {
    const auto first=At<Address>(roleList,8),last=At<Address>(roleList,0x10);
    if(!first.readable || !last.readable || last.value<first.value ||
       (last.value-first.value)%0x30 || (last.value-first.value)/0x30>4096)return {};
    return {static_cast<unsigned>((last.value-first.value)/0x30),true};
}
// Pure memory observations of sufficient rejection conditions. They identify
// constraints, not the first executed branch; native subcheck results follow.
struct GateMemory {unsigned blocks=0;bool complete=true;};
inline GateMemory ReadGateMemory(Address raw,Address circle,Address context) {
    GateMemory r;
    const auto actor=At<Address>(raw,0x2E8),a=At<Address>(raw,0x340),b=At<Address>(raw,0x348);
    const auto only=At<Address>(circle,0xC0),target=At<Address>(circle,0x38),actual=At<Address>(raw,0x310);
    const auto state=At<Address>(raw,0x370);const auto ready=At<std::uint8_t>(raw,0x43D);
    if(!actor.readable || !a.readable || !b.readable || !only.readable || !target.readable || !actual.readable || !state.readable || !ready.readable)r.complete=false;
    if(actor.readable && !actor.value)r.blocks|=1;
    if(a.readable && b.readable && (a.value?a.value:b.value) && (a.value?a.value:b.value)!=context)r.blocks|=2;
    if(only.readable && actor.readable && only.value && only.value!=actor.value)r.blocks|=4;
    if(target.readable && actual.readable && target.value!=actual.value)r.blocks|=8;
    if(state.readable && !state.value)r.blocks|=16;
    if(ready.readable && !ready.value)r.blocks|=32;
    const auto owner=At<Address>(raw,0x2C0);
    if(!owner.readable)r.complete=false;
    else if(!owner.value)r.blocks|=64;
    else {
        const auto machine=At<Address>(owner.value,0x2F0);
        const auto active=At<Address>(machine.value,0x58);
        if(!machine.readable || !machine.value || !active.readable)r.complete=false;
        else if(active.value)r.blocks|=128;
    }
    const auto until=At<unsigned>(circle,0x1E0);const auto priority=At<Address>(circle,0x178);
    if(!until.readable || !priority.readable)r.complete=false;
    else if(until.value && priority.value) {
        const auto clockRoot=At<Address>(ring_might::Base(),0x2702598);
        const auto clock=At<Address>(clockRoot.value,8);
        const auto now=At<unsigned>(clock.value,0x18);
        const auto stats=At<Address>(actor.value,0x26A8);
        if(!clockRoot.readable || !clock.readable || !now.readable || !stats.readable)r.complete=false;
        else if(now.value<=until.value && priority.value!=stats.value)r.blocks|=256;
    }
    return r;
}
using Bool1Fn=bool(__fastcall*)(Address);
using Int1Fn=int(__fastcall*)(Address);
using Bool2Fn=bool(__fastcall*)(Address,Address);
inline std::array<std::unique_ptr<PLH::x64Detour>,6> subcheckHooks{};
inline std::array<std::uint64_t,6> subcheckTrampolines{};
inline Address traceBase=0;
inline constexpr std::array<Address,6> subcheckReturnRvas{0x37A45B,0x37A495,0x37A4B3,0x37A4C7,0x37A4DD,0x37A4FE};
inline void RecordSubcheck(unsigned index,int result,Address caller,Address input=0) {
    if(currentCandidate && currentCandidate->inGate && index<6 && caller==traceBase+subcheckReturnRvas[index]) {
        currentCandidate->subchecks[index]=result;currentCandidate->subcheckInputs[index]=input;
    }
}
template<unsigned I>inline bool __fastcall Bool1Detour(Address value) {
    const auto caller=reinterpret_cast<Address>(_ReturnAddress());
    const auto result=PLH::FnCast(subcheckTrampolines[I],static_cast<Bool1Fn>(nullptr))(value);
    RecordSubcheck(I,result,caller,value);return result;
}
inline int __fastcall Int1Detour(Address value) {
    const auto caller=reinterpret_cast<Address>(_ReturnAddress());
    const auto result=PLH::FnCast(subcheckTrampolines[1],static_cast<Int1Fn>(nullptr))(value);
    RecordSubcheck(1,result,caller,value);return result;
}
template<unsigned I>inline bool __fastcall Bool2Detour(Address first,Address second) {
    const auto caller=reinterpret_cast<Address>(_ReturnAddress());
    const auto result=PLH::FnCast(subcheckTrampolines[I],static_cast<Bool2Fn>(nullptr))(first,second);
    RecordSubcheck(I,result,caller,first);return result;
}

inline bool __fastcall GateDetour(Address raw,Address role,Address circle,float distance,
    unsigned population,float* score,Address context) {
    if(currentCandidate && currentCandidate->raw==raw) {
        currentCandidate->population=population;
        currentCandidate->context=context;currentCandidate->ownership=ReadOwnerContext(raw,context);
        currentCandidate->inGate=true;
        if(currentCandidate->ownership.kind!=OwnerClass::Other && GetTickCount64()>=nextRejection.load()) {
            currentCandidate->f441=At<std::uint8_t>(raw,0x441);
            currentCandidate->f442=At<std::uint8_t>(raw,0x442);
            const auto memory=ReadGateMemory(raw,circle,context);
            currentCandidate->memoryCaptured=true;currentCandidate->memoryComplete=memory.complete;
            currentCandidate->memoryBlocks=memory.blocks;
        }
    }
    const bool result=PLH::FnCast(gateTrampoline,static_cast<GateFn>(nullptr))(
        raw,role,circle,distance,population,score,context);
    if(currentCandidate && currentCandidate->raw==raw) {
        currentCandidate->inGate=false;currentCandidate->gateReached=true;currentCandidate->gateAllowed=result;
    }
    return result;
}
inline void __fastcall CandidateDetour(Address roleList,Address circle,Address raw,unsigned population,Address context) {
    const auto target=pickerTarget;gc::Binding source;
    const bool inPicker=target.actor && circle==pickerCircle;
    if(inPicker)++observedCandidateCalls;
    const bool buddyRole=inPicker && ReadRecord(At<Address>(roleList,0).value).Named("Buddy_Commanded");
    if(buddyRole)++buddyRoleCalls;
    const bool relevant=buddyRole && MatchSource(raw,target.actor,source);
    if(buddyRole && !relevant)++unmatchedCandidateCalls;
    CandidateFrame frame{raw};auto* previous=currentCandidate;
    // Suppress attribution of nested unrelated candidates to the outer call.
    currentCandidate=relevant?&frame:nullptr;
    const auto before=relevant?CandidateCount(roleList):Field<unsigned>{};
    if(relevant)++candidateCalls;
    PLH::FnCast(candidateTrampoline,static_cast<CandidateFn>(nullptr))(roleList,circle,raw,population,context);
    currentCandidate=previous;
    if(!relevant)return;
    const auto after=CandidateCount(roleList);
    const bool accepted=before.readable && after.readable && after.value>before.value;
    if(accepted)++acceptedCandidates;
    else if(before.readable && after.readable && !frame.gateReached)++prefilterRejects;
    else if(before.readable && after.readable && !frame.gateAllowed)++gateRejects;
    const bool own=frame.ownership.kind==OwnerClass::Own || frame.ownership.kind==OwnerClass::Unrestricted;
    if(frame.gateReached && own) {
        ++ownContextCalls;if(accepted)++ownContextAccepted;else ++ownContextRejects;
    }
    if(!accepted && frame.gateReached && frame.ownership.kind==OwnerClass::Other) {
        ++otherContextRejects;return; // Expected: a per-unit override excludes other contexts.
    }
    if(!accepted && Sample(nextRejection)) {
        const auto combatTarget=At<Address>(raw,0x310),pathState=At<Address>(raw,0x370);
        const auto ready=At<std::uint8_t>(raw,0x43D),f43e=At<std::uint8_t>(raw,0x43E),f43f=At<std::uint8_t>(raw,0x43F);
        const auto attack=ReadRecord(At<Address>(source.stats,0xE0).value);
        char text[768]{};
        std::snprintf(text,sizeof(text),
            "native-rejection tick=%llu batch=%llu raw=%p outcome=%s vectorRead=%d finalGateReached=%d finalGateAllowed=%d population=%u raw441=%u raw442=%u multiFlagReads=%d/%d earlyMultiBlocked=%d rawTargetMatches=%d rawTargetRead=%d raw370Nonzero=%d raw370Read=%d raw43D=%u raw43E=%u raw43F=%u flagReads=%d/%d/%d distanceM=%.2f attack=%s",
            static_cast<unsigned long long>(GetTickCount64()),gc::crowdBatch.load(),reinterpret_cast<void*>(raw),
            (!before.readable || !after.readable)?"unreadable-vector":(frame.gateReached?(frame.gateAllowed?"after-final-gate":"final-gate"):"prefilter"),
            before.readable && after.readable,frame.gateReached,frame.gateAllowed,frame.population,
            unsigned(frame.f441.value),unsigned(frame.f442.value),frame.f441.readable,frame.f442.readable,
            frame.gateReached && frame.population>1 && frame.f441.readable && !frame.f441.value && frame.f442.readable && !frame.f442.value,
            combatTarget.readable && combatTarget.value==target.stats,combatTarget.readable,
            pathState.value!=0,pathState.readable,unsigned(ready.value),unsigned(f43e.value),unsigned(f43f.value),
            ready.readable,f43e.readable,f43f.readable,std::sqrt(gc::DistanceSquared(source.actor,target.actor))*.01f,
            attack.nameComplete?attack.name.data():"<none/unavailable>");
        gc::Log(text,source.actor,target.actor,true);
        const auto property=ReadRecord(frame.subcheckInputs[0]),stateProperty=ReadRecord(frame.subcheckInputs[2]);
        const auto owner=frame.ownership.first?frame.ownership.first:frame.ownership.second;
        const auto overridePlay=ReadRecord(At<Address>(owner,0xA8).value);
        std::snprintf(text,sizeof(text),"native-gate-detail tick=%llu batch=%llu memoryCaptured=%d memoryComplete=%d memoryBlocks=0x%X subchecks=%d/%d/%d/%d/%d/%d property=%s stateProperty=%s ownerClass=%u context=%p override340=%p override348=%p overridePlay=%s",
            static_cast<unsigned long long>(GetTickCount64()),gc::crowdBatch.load(),frame.memoryCaptured,frame.memoryComplete,frame.memoryBlocks,
            frame.subchecks[0],frame.subchecks[1],frame.subchecks[2],frame.subchecks[3],frame.subchecks[4],frame.subchecks[5],
            property.nameComplete?property.name.data():"<none/unavailable>",stateProperty.nameComplete?stateProperty.name.data():"<none/unavailable>",unsigned(frame.ownership.kind),
            reinterpret_cast<void*>(frame.context),reinterpret_cast<void*>(frame.ownership.first),reinterpret_cast<void*>(frame.ownership.second),
            overridePlay.nameComplete?overridePlay.name.data():"<none/unavailable>");
        gc::Log(text,source.actor,target.actor,true);
    }
}
inline void __fastcall ModeDetour(Address manager,Address entry,std::uint8_t mode,Address player,Address targetStats) {
    const auto caller=reinterpret_cast<Address>(_ReturnAddress());
    const auto actor=At<Address>(entry,0);const auto before=At<std::uint8_t>(entry,0x80);
    Address target=0,sourceStats=0,sourceRaw=0;OwnerContext ownerBefore;Record attack{};
    if(gc::crowdCommandsActive.load() && before.readable && before.value==2 && mode!=2)
        for(const auto& signal:gc::orderSignals)
            if(actor.readable && signal.actor.load()==actor.value && !signal.destroyed.load() && signal.target.load()) {
                target=signal.target.load();sourceStats=signal.stats.load();attack=ReadRecord(At<Address>(sourceStats,0xE0).value);
                sourceRaw=At<Address>(sourceStats,0x2E0).value;
                if(At<Address>(sourceRaw,0x2E8).value==actor.value)ownerBefore=ReadOwnerContext(sourceRaw,0);
                else sourceRaw=0;
                break;
            }
    PLH::FnCast(modeTrampoline,static_cast<ModeFn>(nullptr))(manager,entry,mode,player,targetStats);
    if(target) {
        const bool ownerVerified=sourceRaw && gc::Live({actor.value,sourceStats}) && At<Address>(sourceStats,0x2E0).value==sourceRaw;
        const auto ownerAfter=ownerVerified?ReadOwnerContext(sourceRaw,0):OwnerContext{};
        char text[640]{};
        std::snprintf(text,sizeof(text),"native-mode-exit tick=%llu batch=%llu from=2 to=%u callerRVA=0x%llX attackBefore=%s ownerVerified=%d overrideBefore=%p overrideAfter=%p",
            static_cast<unsigned long long>(GetTickCount64()),gc::crowdBatch.load(),unsigned(mode),
            static_cast<unsigned long long>(caller-ring_might::Base()),attack.nameComplete?attack.name.data():"<none/unavailable>",ownerVerified,
            reinterpret_cast<void*>(ownerBefore.first?ownerBefore.first:ownerBefore.second),
            reinterpret_cast<void*>(ownerAfter.first?ownerAfter.first:ownerAfter.second));
        gc::Log(text,actor.value,target,true);
    }
}

inline void Heartbeat(Address,Address circle,Address) {
    gc::Binding target;if(!Relevant(circle,target) || !Sample(nextHeartbeat))return;
    unsigned issued=0,close=0,bound=0,flags=0,attacks=0;
    for(const auto& signal:gc::orderSignals) {
        const auto actor=signal.actor.load(),stats=signal.stats.load();
        if(!actor || signal.destroyed.load() || signal.target.load()!=target.actor || !gc::Live({actor,stats}))continue;
        ++issued;
        if(gc::DistanceSquared(actor,target.actor)<=400.f*400.f)++close;
        const auto binding=At<Address>(actor,0x2EB8),attack=At<Address>(stats,0xE0);
        const auto flag=At<std::uint8_t>(actor,0x2E90);
        if(binding.readable && binding.value==target.stats)++bound;
        if(flag.readable && flag.value)++flags;
        if(attack.readable && attack.value)++attacks;
    }
    char text[640]{};
    std::snprintf(text,sizeof(text),"native-heartbeat tick=%llu batch=%llu pickerCalls=%llu buddyAllocationCalls=%llu issued=%u within4m=%u boundMatch=%u commandFlags=%u attackRecords=%u candidateCalls=%llu accepted=%llu prefilterRejects=%llu finalGateRejects=%llu observedCandidates=%llu buddyRoleCandidates=%llu unmatchedCandidates=%llu otherContextRejects=%llu ownContextCalls=%llu ownContextAccepted=%llu ownContextRejected=%llu",
        static_cast<unsigned long long>(GetTickCount64()),gc::crowdBatch.load(),pickCalls.load(),allocationCalls.load(),issued,close,bound,flags,attacks,candidateCalls.load(),acceptedCandidates.load(),prefilterRejects.load(),gateRejects.load(),observedCandidateCalls.load(),buddyRoleCalls.load(),unmatchedCandidateCalls.load(),
        otherContextRejects.load(),ownContextCalls.load(),ownContextAccepted.load(),ownContextRejects.load());
    gc::Log(text,0,target.actor,true);
    command_los::Report(target.actor);
}
inline bool Remove() {
    gc::traceNativeCommandEnds=false;
    if(grunt_aggression::playObserver==&Heartbeat)grunt_aggression::playObserver=nullptr;
    bool ok=true;
    for(auto& hook:subcheckHooks) {
        if(hook && !hook->unHook()){ok=false;continue;}
        hook.reset();
    }
    for(auto* hook:{&modeHook,&gateHook,&candidateHook,&allocateHook,&pickHook}) {
        if(*hook && !(*hook)->unHook()){ok=false;continue;}
        hook->reset();
    }
    return ok;
}
inline bool Install() {
    if(pickHook || allocateHook || candidateHook || gateHook || modeHook || !gc::updateHook)return false;
    for(const auto& hook:subcheckHooks)if(hook)return false;
    const auto base=ring_might::Base();traceBase=base;
    constexpr std::array<Address,6> helperRvas{0x37A68C,0x2F37E0,0x378F18,0x2FC004,0x444CB4,0x37A6F8};
    constexpr std::array<std::array<std::uint8_t,8>,6> signatures{{
        {0x48,0x83,0xEC,0x28,0x4C,0x8B,0xC1,0xE8},
        {0x48,0x8B,0x91,0x60,0x02,0,0,0x33},
        {0x48,0x83,0xEC,0x28,0x4C,0x8B,0xC1,0xE8},
        {0x48,0x83,0xEC,0x28,0xE8,0x23,0,0},
        {0x40,0x53,0x48,0x81,0xEC,0xA0,0,0},
        {0x48,0x89,0x5C,0x24,0x10,0x57,0x48,0x83}}};
    for(std::size_t i=0;i<helperRvas.size();++i) {
        const auto bytes=Read<std::array<std::uint8_t,8>>(base+helperRvas[i]);
        if(!bytes.readable || bytes.value!=signatures[i]) {
            gc::Log("native trace subcheck signature mismatch; command unchanged",0,0,true);return false;
        }
    }
    const auto picker=Read<std::array<std::uint8_t,7>>(base+0x11D618);
    const auto allocator=Read<std::array<std::uint8_t,5>>(base+0x425C3C);
    if(!picker.readable || picker.value!=std::array<std::uint8_t,7>{0x48,0x8B,0xC4,0x48,0x89,0x58,0x08} ||
       !allocator.readable || allocator.value!=std::array<std::uint8_t,5>{0x48,0x89,0x5C,0x24,0x18}) {
        gc::Log("native trace signature mismatch; gameplay command unchanged",0,0,true);return false;
    }
    const auto candidate=Read<std::array<std::uint8_t,7>>(base+0x11DA18);
    const auto gate=Read<std::array<std::uint8_t,7>>(base+0x37A2C4);
    const auto mode=Read<std::array<std::uint8_t,5>>(base+0x1AA4B90);
    if(!candidate.readable || candidate.value!=std::array<std::uint8_t,7>{0x48,0x8B,0xC4,0x48,0x89,0x58,0x10} ||
       !gate.readable || gate.value!=std::array<std::uint8_t,7>{0x48,0x8B,0xC4,0x48,0x89,0x58,0x18} ||
       !mode.readable || mode.value!=std::array<std::uint8_t,5>{0x48,0x89,0x5C,0x24,0x08}) {
        gc::Log("native trace eligibility signature mismatch; command unchanged",0,0,true);return false;
    }
    pickHook=std::make_unique<PLH::x64Detour>(base+0x11D618,reinterpret_cast<Address>(&PickDetour),&pickTrampoline);
    allocateHook=std::make_unique<PLH::x64Detour>(base+0x425C3C,reinterpret_cast<Address>(&AllocateDetour),&allocateTrampoline);
    if(!pickHook->hook()){pickHook.reset();allocateHook.reset();return false;}
    if(!allocateHook->hook()){allocateHook.reset();Remove();return false;}
    candidateHook=std::make_unique<PLH::x64Detour>(base+0x11DA18,reinterpret_cast<Address>(&CandidateDetour),&candidateTrampoline);
    gateHook=std::make_unique<PLH::x64Detour>(base+0x37A2C4,reinterpret_cast<Address>(&GateDetour),&gateTrampoline);
    modeHook=std::make_unique<PLH::x64Detour>(base+0x1AA4B90,reinterpret_cast<Address>(&ModeDetour),&modeTrampoline);
    if(!candidateHook->hook()){candidateHook.reset();gateHook.reset();modeHook.reset();Remove();return false;}
    if(!gateHook->hook()){gateHook.reset();modeHook.reset();Remove();return false;}
    if(!modeHook->hook()){modeHook.reset();Remove();return false;}
    const std::array<Address,6> helperDetours{
        reinterpret_cast<Address>(&Bool1Detour<0>),reinterpret_cast<Address>(&Int1Detour),
        reinterpret_cast<Address>(&Bool1Detour<2>),reinterpret_cast<Address>(&Bool1Detour<3>),
        reinterpret_cast<Address>(&Bool2Detour<4>),reinterpret_cast<Address>(&Bool2Detour<5>)};
    for(std::size_t i=0;i<helperRvas.size();++i) {
        subcheckHooks[i]=std::make_unique<PLH::x64Detour>(base+helperRvas[i],helperDetours[i],&subcheckTrampolines[i]);
        if(!subcheckHooks[i]->hook()){subcheckHooks[i].reset();Remove();return false;}
    }
    otherContextRejects=0;ownContextCalls=0;ownContextRejects=0;ownContextAccepted=0;
    observedCandidateCalls=0;buddyRoleCalls=0;unmatchedCandidateCalls=0;
    candidateCalls=0;acceptedCandidates=0;prefilterRejects=0;gateRejects=0;nextRejection=0;
    pickCalls=0;allocationCalls=0;nextPick=0;nextAllocation=0;nextHeartbeat=0;lastContext=0;
    gc::traceNativeCommandEnds=true;
    if(grunt_aggression::updateHook)grunt_aggression::playObserver=&Heartbeat;
    gc::Log("native trace v6 installed; own-context rejection trace; expected override exclusions counted separately",0,0,true);
    return true;
}
} // namespace sow::grunt_command_native_trace
