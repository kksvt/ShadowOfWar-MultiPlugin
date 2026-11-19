#pragma once
#include "sow_grunt_command.hpp"
#include "sow_command_perception_gate.hpp"

// Optional observation at native boundaries. Originals always run unchanged.
// No logging, allocation or blocking locks in detours. One tracked pair at a time.
namespace sow::command_perception_trace {
namespace gc=grunt_command;
using DispatchFn=Address(__fastcall*)(Address,Address,Address);
using PrepareFn=unsigned(__fastcall*)(Address,Address,Address,Address);
using DeliverFn=Address(__fastcall*)(Address,Address,Address,Address);
using ClassifyFn=Address(__fastcall*)(Address,Address,Address,Address,Address,Address,Address);
using RefreshFn=void(__fastcall*)(Address,Address,Address,Address,std::uint8_t);
using GateFn=char(__fastcall*)(Address,Address);
inline std::array<std::unique_ptr<PLH::x64Detour>,6> hooks;
inline std::array<std::uint64_t,6> originals{};
inline std::atomic<bool> enabled{false};
inline std::atomic<Address> sourceHint{0},targetHint{0};
inline std::atomic<unsigned long long> generation{0},dropped{0};
inline std::atomic<ULONGLONG> nextGateDetail{0};
inline Address relationMatrixPointer=0;
inline std::mutex mutex;
struct Counters {
    unsigned long long dispatch=0,queued=0,queueUnknown=0,prep2=0,prep3=0,prepOther=0;
    unsigned long long delivered=0,classify=0,transitionRejected=0,classUnreadable=0,refreshCalls=0;
    unsigned long long sourceDispatch=0,sourceQueued=0,sourcePrepare=0,sourceDelivered=0,sourceClassify=0,sourceRefresh=0;
    unsigned long long gateSource=0,gateSourcePass=0,gatePair=0,gatePairPass=0,gatePairFail=0;
    Address lastPrimary=0,lastSecondary=0;int lastKind=-1;
    command_perception_gate::Snapshot gateDetail{};bool detailReady=false;ULONGLONG detailTick=0;
    int oldState=-1,newState=-1;float progress=-1;
};
struct Snapshot {
    Address source=0,stats=0,target=0;unsigned long long batch=0,epoch=0;
    ULONGLONG started=0;Counters counters{};
};
inline Snapshot focus;
struct Ticket {Address source=0,target=0;unsigned long long epoch=0;explicit operator bool()const{return source!=0;}};
inline Ticket Pair(Address source,Address target) {
    if(!enabled.load() || !gc::crowdCommandsActive.load())return {};
    const auto windowGeneration=generation.load();
    if(!source || !target || sourceHint.load()!=source || targetHint.load()!=target || generation.load()!=windowGeneration)return {};
    return {source,target,windowGeneration};
}
inline Ticket Source(Address source){return Pair(source,targetHint.load());}
inline Address NativeOwner(Address record) {
    // These entry points receive an engine-owned record whose header the native
    // caller/callee already dereferences. Read only that fixed header directly:
    // do not issue ReadProcessMemory for every unrelated actor's hot gate call.
    Address owner=0;if(record)std::memcpy(&owner,reinterpret_cast<const void*>(record),sizeof(owner));return owner;
}
inline Ticket ListenerSource(Address listener) {
    if(!enabled.load() || !gc::crowdCommandsActive.load())return {};
    // 123BF4 / 123B34: listener+0 is source actor. Stimulus target selection
    // follows its kind (4403B4): +48 for kind 1, +70 for kind 2, never stats.
    const auto source=NativeOwner(listener);
    if(!source || source!=sourceHint.load())return {};
    return Source(source);
}
inline Ticket Listener(Address listener,Address stimulus) {
    const auto ticket=ListenerSource(listener);if(!ticket)return {};
    const auto target=command_perception_gate::ReadTarget(stimulus);
    return target.readable && target.effective==ticket.target?ticket:Ticket{};
}
template<class F> inline void Record(Ticket ticket,F change) {
    if(!ticket)return;
    std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);
    if(!lock.owns_lock()){++dropped;return;}
    if(enabled.load() && focus.epoch==ticket.epoch && focus.source==ticket.source && focus.target==ticket.target)
        change(focus.counters);
}
inline Field<unsigned> Queued(Address listener) {
    const auto bounds=At<std::array<Address,2>>(listener,0x80);
    if(!bounds.readable || bounds.value[1]<bounds.value[0] ||
       (bounds.value[1]-bounds.value[0])%312 || (bounds.value[1]-bounds.value[0])/312>4096)return {};
    return {static_cast<unsigned>((bounds.value[1]-bounds.value[0])/312),true};
}
inline Address __fastcall DispatchDetour(Address listener,Address stimulus,Address range) {
    const auto ticket=ListenerSource(listener);
    const auto target=ticket?command_perception_gate::ReadTarget(stimulus):command_perception_gate::Target{};
    const bool matched=ticket && target.readable && target.effective==ticket.target;
    Field<unsigned> before;
    if(ticket){
        Record(ticket,[&](Counters& c){++c.sourceDispatch;if(matched)++c.dispatch;c.lastPrimary=target.primary;c.lastSecondary=target.secondary;c.lastKind=target.kind;});
        before=Queued(listener);
    }
    const auto result=PLH::FnCast(originals[0],static_cast<DispatchFn>(nullptr))(listener,stimulus,range);
    if(ticket){
        const auto after=Queued(listener);
        Record(ticket,[&](Counters& c){if(before.readable && after.readable && after.value>=before.value){c.sourceQueued+=after.value-before.value;if(matched)c.queued+=after.value-before.value;}else if(matched)++c.queueUnknown;});
    }
    return result;
}
inline unsigned __fastcall PrepareDetour(Address sensor,Address target,Address stimulus,Address output) {
    Ticket ticket;
    if(enabled.load() && gc::crowdCommandsActive.load()){
        const auto source=At<Address>(sensor,8);if(source.readable)ticket=Source(source.value);
    }
    const auto result=PLH::FnCast(originals[1],static_cast<PrepareFn>(nullptr))(sensor,target,stimulus,output);
    Record(ticket,[&](Counters& c){++c.sourcePrepare;if(target==ticket.target){if(result==2)++c.prep2;else if(result==3)++c.prep3;else ++c.prepOther;}});
    return result;
}
inline Address __fastcall DeliverDetour(Address listener,Address stimulus,Address range,Address output) {
    const auto ticket=ListenerSource(listener);
    const auto target=ticket?command_perception_gate::ReadTarget(stimulus):command_perception_gate::Target{};
    Record(ticket,[&](Counters& c){++c.sourceDelivered;if(target.readable && target.effective==ticket.target)++c.delivered;});
    return PLH::FnCast(originals[2],static_cast<DeliverFn>(nullptr))(listener,stimulus,range,output);
}
inline bool RejectsTransition(unsigned oldState,unsigned newState) {
    return (oldState==4 && newState!=4) || (oldState!=0 && newState==0);
}
inline Address __fastcall ClassifyDetour(Address source,Address target,Address stimulus,Address point,Address oldOut,Address newOut,Address progressOut) {
    const auto ticket=Source(source);
    const auto result=PLH::FnCast(originals[3],static_cast<ClassifyFn>(nullptr))(source,target,stimulus,point,oldOut,newOut,progressOut);
    if(ticket){
        const auto oldState=Read<unsigned>(oldOut),newState=Read<unsigned>(newOut);const auto progress=Read<float>(progressOut);
        Record(ticket,[&](Counters& c){
            ++c.sourceClassify;if(target!=ticket.target)return;
            ++c.classify;
            if(oldState.readable && newState.readable){
                c.oldState=static_cast<int>(oldState.value);c.newState=static_cast<int>(newState.value);
                if(RejectsTransition(oldState.value,newState.value))++c.transitionRejected;
            }else ++c.classUnreadable;
            c.progress=progress.readable && std::isfinite(progress.value)?progress.value:-1.f;
        });
    }
    return result;
}
inline void __fastcall RefreshDetour(Address manager,Address source,Address target,Address duration,std::uint8_t refresh) {
    const auto ticket=Source(source);
    PLH::FnCast(originals[4],static_cast<RefreshFn>(nullptr))(manager,source,target,duration,refresh);
    Record(ticket,[&](Counters& c){++c.sourceRefresh;if(target==ticket.target)++c.refreshCalls;});
}
inline char __fastcall GateDetour(Address metadata,Address envelope) {
    Ticket ticket;
    if(enabled.load() && gc::crowdCommandsActive.load()){
        const auto source=NativeOwner(metadata);if(source==sourceHint.load())ticket=Source(source);
    }
    command_perception_gate::Target target;
    if(ticket){const auto stimulus=At<Address>(envelope,0x90);if(stimulus.readable)target=command_perception_gate::ReadTarget(stimulus.value);}
    const auto result=PLH::FnCast(originals[5],static_cast<GateFn>(nullptr))(metadata,envelope);
    const bool matched=ticket && target.readable && target.effective==ticket.target;
    Record(ticket,[&](Counters& c){
        ++c.gateSource;if(result)++c.gateSourcePass;
        if(matched){++c.gatePair;if(result)++c.gatePairPass;else ++c.gatePairFail;}
        c.lastKind=target.kind;c.lastPrimary=target.primary;c.lastSecondary=target.secondary;
    });
    if(matched && !result){
        const auto now=GetTickCount64();auto deadline=nextGateDetail.load();
        if(now>=deadline && nextGateDetail.compare_exchange_strong(deadline,now+1000)){
            const auto matrix=Read<Address>(relationMatrixPointer);
            const auto detail=command_perception_gate::Read(metadata,envelope,matrix.value);
            Record(ticket,[&](Counters& c){c.gateDetail=detail;c.detailReady=true;c.detailTick=now;});
        }
    }
    return result;
}
inline bool StillTracked(const Snapshot& current) {
    if(current.batch!=gc::crowdBatch.load())return false;
    for(const auto& signal:gc::orderSignals)
        if(signal.actor.load()==current.source && signal.stats.load()==current.stats && signal.target.load()==current.target && !signal.destroyed.load())return true;
    return false;
}
inline bool Sample(gc::Binding candidate,gc::Binding target,ULONGLONG now,Snapshot& out) {
    if(!enabled.load())return false;
    std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);if(!lock.owns_lock())return false;
    if(!focus.source || !StillTracked(focus)){
        sourceHint=0;targetHint=0;
        focus={};focus.epoch=++generation;focus.batch=gc::crowdBatch.load();focus.started=now;
        nextGateDetail=0;
        if(candidate.actor && candidate.stats && target.actor){focus.source=candidate.actor;focus.stats=candidate.stats;focus.target=target.actor;}
        sourceHint=focus.source;targetHint=focus.target;
    }
    out=focus;return focus.source!=0;
}
inline bool AnyHook(){for(const auto& hook:hooks)if(hook)return true;return false;}
inline bool Remove() {
    enabled=false;sourceHint=0;targetHint=0;
    bool ok=true;
    for(auto& hook:hooks){if(hook && !hook->unHook())ok=false;else hook.reset();}
    // Keep trampoline storage when removal fails: the detour still forwards.
    return ok;
}
inline bool Install() {
    if(AnyHook() || !gc::updateHook)return false;
    const auto base=ring_might::Base();
    const std::array<Address,6> rvas{0x123BF4,0x4404B0,0x123B34,0x64C668,0x43F4E4,0x1225D8};
    const std::array<std::array<std::uint8_t,16>,6> signatures{{
        {0x48,0x89,0x5C,0x24,0x10,0x48,0x89,0x74,0x24,0x20,0x55,0x57,0x41,0x54,0x41,0x56},
        {0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x6C,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57},
        {0x48,0x8B,0xC4,0x48,0x89,0x58,0x08,0x48,0x89,0x68,0x10,0x48,0x89,0x70,0x18,0x48},
        {0x48,0x8B,0xC4,0x48,0x89,0x58,0x10,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56},
        {0x40,0x53,0x48,0x83,0xEC,0x20,0x48,0x8B,0x0D,0x57,0x07,0x2C,0x02,0x4D,0x8B,0xD9},
        {0x48,0x89,0x5C,0x24,0x08,0x57,0x48,0x83,0xEC,0x20,0x8B,0x41,0x30,0x48,0x8B,0xDA}
    }};
    for(std::size_t i=0;i<rvas.size();++i){const auto bytes=Read<std::array<std::uint8_t,16>>(base+rvas[i]);if(!bytes.readable || bytes.value!=signatures[i])return false;}
    const std::array<Address,6> detours{reinterpret_cast<Address>(&DispatchDetour),reinterpret_cast<Address>(&PrepareDetour),reinterpret_cast<Address>(&DeliverDetour),reinterpret_cast<Address>(&ClassifyDetour),reinterpret_cast<Address>(&RefreshDetour),reinterpret_cast<Address>(&GateDetour)};
    for(std::size_t i=0;i<rvas.size();++i){
        hooks[i]=std::make_unique<PLH::x64Detour>(base+rvas[i],detours[i],&originals[i]);
        if(!hooks[i]->hook()){
            // A failed hook was not installed; rollback only installed entries.
            hooks[i].reset();Remove();return false;
        }
    }
    {std::lock_guard<std::mutex> lock(mutex);focus={};++generation;dropped=0;sourceHint=0;targetHint=0;}
    relationMatrixPointer=base+0x2A868C0;nextGateDetail=0;enabled=true;return true;
}
} // namespace sow::command_perception_trace
