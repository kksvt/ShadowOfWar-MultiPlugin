#pragma once
#include "sow_command_perception_trace.hpp"

// Observe the exact native planner methods. No AI calls, I/O, allocation or
// blocking locks in detours. Reuse the perception trace's stable source/epoch.
namespace sow::command_planner_trace {
namespace pt=command_perception_trace;
namespace gc=grunt_command;
using BeginFn=Address(__fastcall*)(Address,Address);
using CircleUpdateFn=Address(__fastcall*)(Address,std::uint8_t);
using EndFn=Address(__fastcall*)(Address);
using AttackUpdateFn=Address(__fastcall*)(Address,std::uint8_t,Address);
using PermissionFn=bool(__fastcall*)(Address,Address);
inline std::array<std::unique_ptr<PLH::x64Detour>,6> hooks;
inline std::array<std::uint64_t,6> originals{};
inline std::atomic<bool> enabled{false};
inline std::atomic<unsigned long long> dropped{0};
inline std::mutex mutex;
enum class Phase:unsigned {CircleBegin,CircleUpdate,CircleEnd,AttackBegin,AttackUpdate,Count};
inline const char* Name(Phase phase) {
    switch(phase){
    case Phase::CircleBegin:return "circle-begin";
    case Phase::CircleUpdate:return "circle-update";
    case Phase::CircleEnd:return "circle-end";
    case Phase::AttackBegin:return "attack-begin";
    case Phase::AttackUpdate:return "attack-update";
    default:return "unknown";
    }
}
struct Event {Phase phase=Phase::CircleBegin;Address result=0;ULONGLONG tick=0;bool started=false;};
struct Counters {
    std::array<unsigned long long,static_cast<unsigned>(Phase::Count)> calls{},permissionPass{},permissionFail{};
    unsigned long long circleAccepted=0,attackAccepted=0,beginStarted=0,startedEdges=0;
    std::array<unsigned long long,4> circleResults{},attackResults{};
    Address lastCircleResult=~Address(0),lastAttackResult=~Address(0);
    std::uint32_t circleDeadline=0;bool circleDeadlineReadable=false;
    std::array<Event,8> events{};unsigned long long eventCount=0;
};
struct Snapshot {pt::Ticket ticket{};Counters counters{};};
inline Snapshot focus;
struct Context {pt::Ticket ticket{};Phase phase=Phase::CircleBegin;};
inline thread_local Context context;
struct Scope {
    Context previous;
    Scope(pt::Ticket ticket,Phase phase):previous(context){context={ticket,phase};}
    ~Scope(){context=previous;}
};
inline pt::Ticket StepTicket(Address step) {
    if(!enabled.load() || !gc::crowdCommandsActive.load() || !step)return {};
    // Native callers own this step and the observed methods dereference +8.
    Address actor=0;std::memcpy(&actor,reinterpret_cast<const void*>(step+8),sizeof(actor));
    return pt::Source(actor);
}
template<class F> inline void Record(pt::Ticket ticket,F change) {
    if(!ticket || !enabled.load())return;
    std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);
    if(!lock.owns_lock()){++dropped;return;}
    if(ticket.epoch!=pt::generation.load() || ticket.source!=pt::sourceHint.load() || ticket.target!=pt::targetHint.load())return;
    if(focus.ticket.epoch!=ticket.epoch || focus.ticket.source!=ticket.source || focus.ticket.target!=ticket.target)focus={ticket,{}};
    change(focus.counters);
}
inline void EventAt(Counters& c,Phase phase,Address result,bool started) {
    c.events[c.eventCount%c.events.size()]={phase,result,GetTickCount64(),started};++c.eventCount;
}
inline void Result(pt::Ticket ticket,Phase phase,Address result,bool started=false) {
    command_planner_trace::Record(ticket,[&](Counters& c){
        ++c.calls[static_cast<unsigned>(phase)];
        bool event=true;
        if(phase==Phase::CircleBegin)c.circleAccepted+=result==0;
        if(phase==Phase::AttackBegin){c.attackAccepted+=result==0;c.beginStarted+=started;}
        if(phase==Phase::CircleUpdate){
            ++c.circleResults[result<3?static_cast<unsigned>(result):3];
            event=result!=c.lastCircleResult;c.lastCircleResult=result;
        }
        if(phase==Phase::AttackUpdate){
            ++c.attackResults[result<3?static_cast<unsigned>(result):3];
            event=result!=c.lastAttackResult || started;c.lastAttackResult=result;
            c.startedEdges+=started;
        }
        if(event)EventAt(c,phase,result,started);
    });
}
inline Address __fastcall CircleBegin(Address step,Address planner) {
    const auto ticket=StepTicket(step);Scope scope(ticket,Phase::CircleBegin);
    const auto result=PLH::FnCast(originals[0],static_cast<BeginFn>(nullptr))(step,planner);
    Result(ticket,Phase::CircleBegin,result);return result;
}
inline Address __fastcall CircleUpdate(Address step,std::uint8_t flag) {
    const auto ticket=StepTicket(step);Scope scope(ticket,Phase::CircleUpdate);
    const auto result=PLH::FnCast(originals[1],static_cast<CircleUpdateFn>(nullptr))(step,flag);
    if(ticket){const auto deadline=At<std::uint32_t>(step,0x178);command_planner_trace::Record(ticket,[&](Counters& c){c.circleDeadline=deadline.value;c.circleDeadlineReadable=deadline.readable;});}
    Result(ticket,Phase::CircleUpdate,result);return result;
}
inline Address __fastcall CircleEnd(Address step) {
    const auto ticket=StepTicket(step);Scope scope(ticket,Phase::CircleEnd);
    const auto result=PLH::FnCast(originals[2],static_cast<EndFn>(nullptr))(step);
    Result(ticket,Phase::CircleEnd,result);return result;
}
inline Address __fastcall AttackBegin(Address step,Address planner) {
    const auto ticket=StepTicket(step);Scope scope(ticket,Phase::AttackBegin);
    const auto result=PLH::FnCast(originals[3],static_cast<BeginFn>(nullptr))(step,planner);
    const auto started=ticket?At<std::uint8_t>(step,0x23):Field<std::uint8_t>{};
    Result(ticket,Phase::AttackBegin,result,result==0 && started.readable && started.value);return result;
}
inline Address __fastcall AttackUpdate(Address step,std::uint8_t flag,Address planner) {
    const auto ticket=StepTicket(step);Scope scope(ticket,Phase::AttackUpdate);
    const auto before=ticket?At<std::uint8_t>(step,0x23):Field<std::uint8_t>{};
    const auto result=PLH::FnCast(originals[4],static_cast<AttackUpdateFn>(nullptr))(step,flag,planner);
    const auto after=ticket?At<std::uint8_t>(step,0x23):Field<std::uint8_t>{};
    Result(ticket,Phase::AttackUpdate,result,before.readable && after.readable && !before.value && after.value);
    return result;
}
inline bool __fastcall Permission(Address source,Address targetStats) {
    // Only the native permission evaluations nested in our observed planner
    // method are counted. Do not call this predicate ourselves or force it.
    const auto current=context;
    const auto result=PLH::FnCast(originals[5],static_cast<PermissionFn>(nullptr))(source,targetStats);
    if(current.ticket && source==current.ticket.source){
        // Target can have changed within the original method. Require the
        // native argument to resolve back to our tracked actor.
        const auto actor=At<Address>(targetStats,0x28);
        if(actor.readable && actor.value==current.ticket.target)
            command_planner_trace::Record(current.ticket,[&](Counters& c){auto& bucket=result?c.permissionPass:c.permissionFail;++bucket[static_cast<unsigned>(current.phase)];});
    }
    return result;
}
inline bool Sample(const pt::Snapshot& perception,Snapshot& out) {
    if(!enabled.load())return false;
    std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);if(!lock.owns_lock())return false;
    const pt::Ticket ticket{perception.source,perception.target,perception.epoch};
    if(focus.ticket.epoch!=ticket.epoch || focus.ticket.source!=ticket.source || focus.ticket.target!=ticket.target)focus={ticket,{}};
    out=focus;return bool(ticket);
}
inline bool AnyHook(){for(const auto& hook:hooks)if(hook)return true;return false;}
inline bool Remove() {
    enabled=false;bool ok=true;
    for(auto& hook:hooks){if(hook && !hook->unHook())ok=false;else hook.reset();}
    return ok; // Retain trampolines/storage for any detour that could not detach.
}
inline bool Install() {
    if(AnyHook() || !pt::enabled.load())return false;
    const auto base=ring_might::Base();
    const std::array<Address,6> rvas{0x493DF4,0x4443E4,0x72FDF4,0x445930,0x11E780,0x445880};
    const std::array<std::array<std::uint8_t,16>,6> signatures{{
        {0x48,0x8B,0xC4,0x48,0x89,0x58,0x08,0x48,0x89,0x68,0x10,0x48,0x89,0x70,0x18,0x48},
        {0x48,0x89,0x5C,0x24,0x10,0x48,0x89,0x6C,0x24,0x18,0x56,0x57,0x41,0x56,0x48,0x83},
        {0x40,0x53,0x48,0x83,0xEC,0x20,0x48,0x8B,0xD9,0x48,0x83,0xC1,0x20,0xE8,0x32,0x00},
        {0x48,0x89,0x5C,0x24,0x10,0x48,0x89,0x6C,0x24,0x18,0x48,0x89,0x74,0x24,0x20,0x57},
        {0x48,0x89,0x5C,0x24,0x10,0x48,0x89,0x6C,0x24,0x18,0x56,0x57,0x41,0x54,0x41,0x56},
        {0x48,0x89,0x5C,0x24,0x08,0x57,0x48,0x83,0xEC,0x20,0x80,0x3D,0x0B,0xFC,0x5E,0x02}
    }};
    for(std::size_t i=0;i<rvas.size();++i){const auto bytes=Read<std::array<std::uint8_t,16>>(base+rvas[i]);if(!bytes.readable || bytes.value!=signatures[i])return false;}
    const std::array<Address,6> detours{reinterpret_cast<Address>(&CircleBegin),reinterpret_cast<Address>(&CircleUpdate),reinterpret_cast<Address>(&CircleEnd),reinterpret_cast<Address>(&AttackBegin),reinterpret_cast<Address>(&AttackUpdate),reinterpret_cast<Address>(&Permission)};
    for(std::size_t i=0;i<rvas.size();++i){
        hooks[i]=std::make_unique<PLH::x64Detour>(base+rvas[i],detours[i],&originals[i]);
        if(!hooks[i]->hook()){hooks[i].reset();Remove();return false;}
    }
    {std::lock_guard<std::mutex> lock(mutex);focus={};dropped=0;}
    enabled=true;return true;
}
} // namespace sow::command_planner_trace
