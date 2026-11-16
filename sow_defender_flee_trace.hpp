#pragma once
#include "sow_defender_repair.hpp"
#include "sow_grunt_aggression.hpp"
#include <intrin.h>
namespace sow::defender_flee_trace {
using RemoveFn=int(__fastcall*)(Address,Address,int,unsigned);
inline std::unique_ptr<PLH::x64Detour> removeHook;
inline std::uint64_t removeTrampoline=0;
inline std::atomic<unsigned long long> sequence{0};
inline ULONGLONG start=0;
inline bool enabled=false;
inline std::mutex logMutex;
struct State {Address actor,member,seed,identity;std::array<int,10> values;std::string overrides;};
inline std::vector<State> states;
inline void Emit(const char* text) {
    std::lock_guard<std::mutex> lock(logMutex);
    char line[2400]{};
    std::snprintf(line,sizeof(line),"[DefenderTrace #%llu +%llums tid=%lu] %s\n",
        sequence.fetch_add(1)+1,GetTickCount64()-start,GetCurrentThreadId(),text);
    OutputDebugStringA(line);if(GetConsoleWindow()){std::fputs(line,stdout);std::fflush(stdout);}
}
inline Address Member(Address actor) {
    if(actor && actor==defender_repair::traceBindingActor)return defender_repair::traceBindingMember;
    const auto snapshot=defender_repair::LoadThreats();
    Entity e;e.actor={actor,true};
    if(!defender_repair::DamageBlocked(e))return 0;
    for(const auto& t:*snapshot)if(t.actor==actor)return t.member;
    return 0;
}
inline void Observe(const char* stage,Address inventory,Address item,int count,Address caller,int result) {
    if(!enabled)return;
    const auto record=ReadRecord(item);
    if(!record.Named("Item_Inspired_Morale_Buff") && !record.Named("Item_Aura_ExtraMorale") && !record.Named("Item_Quirk_Fearless"))return;
    const auto stats=At<Address>(inventory,0x440),owner=At<Address>(stats.value,0x28);
    Address actor=owner.value;
    if(!actor && defender_repair::traceBindingActor &&
       At<Address>(defender_repair::traceBindingActor,0x2348).value==inventory)actor=defender_repair::traceBindingActor;
    const auto member=Member(actor);
    if(!member || At<Address>(actor,0x2348).value!=inventory)return;
    using HasFn=bool(__fastcall*)(Address,Address);
    const auto base=ring_might::Base();
    const int owned=reinterpret_cast<HasFn>(base+0x24DF7C)(inventory,item)?1:0;
    void* frames[12]{};const auto size=CaptureStackBackTrace(1,12,frames,nullptr);
    char stack[320]{};std::size_t used=0;
    for(unsigned i=0;i<size && used+24<sizeof(stack);++i){
        const auto address=reinterpret_cast<Address>(frames[i]);
        if(address<base || address-base>=0x8000000)continue;
        used+=std::snprintf(stack+used,sizeof(stack)-used,"0x%llX,",static_cast<unsigned long long>(address-base));
    }
    char line[1300]{};
    std::snprintf(line,sizeof(line),"inventory=%s actor=%p member=%p item=%s requested=%d result=%d owned=%d callerRVA=0x%llX stackRVA=%s",
        stage,reinterpret_cast<void*>(actor),reinterpret_cast<void*>(member),record.name.data(),count,result,owned,
        static_cast<unsigned long long>(caller>=base && caller-base<0x8000000?caller-base:0),stack);
    Emit(line);
}
inline int __fastcall RemoveItem(Address inventory,Address item,int count,unsigned flags) {
    const auto caller=reinterpret_cast<Address>(_ReturnAddress());
    Observe("remove-before",inventory,item,count,caller,0);
    const auto result=PLH::FnCast(removeTrampoline,static_cast<RemoveFn>(nullptr))(inventory,item,count,flags);
    Observe("remove-after",inventory,item,count,caller,result);return result;
}
inline bool Changed(const State& next) {
    std::lock_guard<std::mutex> lock(logMutex);
    for(auto& old:states)if(old.actor==next.actor){
        const bool changed=old.member!=next.member || old.seed!=next.seed || old.identity!=next.identity || old.values!=next.values || old.overrides!=next.overrides;
        old=next;return changed;
    }
    if(states.size()>=512)states.erase(states.begin()); // Cache bound, never a log cap.
    states.push_back(next);return true;
}
struct ControllerState {
    Field<float> morale;
    Field<unsigned> mode;
    Field<unsigned char> flag;
    std::string overrides="<unavailable>";
};
inline ControllerState ReadController(Address actor) {
    ControllerState state;
    const auto pointer=At<Address>(actor,0x2D08),owner=Read<Address>(pointer.value);
    if(!pointer.readable || !pointer.value || !owner.readable || owner.value!=actor)return state;
    state.morale=At<float>(pointer.value,0x18);state.mode=At<unsigned>(pointer.value,0x1C);state.flag=At<unsigned char>(pointer.value,0x20);
    const auto begin=At<Address>(pointer.value,0x38),end=At<Address>(pointer.value,0x40);
    if(!begin.readable || !end.readable || end.value<begin.value || (end.value-begin.value)%0x18 ||
       (end.value-begin.value)/0x18>64 || (end.value!=begin.value && !begin.value))return state;
    state.overrides="[";
    for(Address at=begin.value;at<end.value;at+=0x18){
        const auto record=ReadRecord(Read<Address>(at).value);
        if(at!=begin.value)state.overrides+=",";
        state.overrides+=record.nameComplete?record.name.data():"<unnamed/unreadable>";
        if(state.overrides.size()>900){state.overrides+="...<truncated>";break;}
    }
    state.overrides+="]";
    const auto b=At<Address>(pointer.value,0x38),e=At<Address>(pointer.value,0x40);
    if(!b.readable || !e.readable || b.value!=begin.value || e.value!=end.value)state.overrides="<changed-during-read>";
    return state;
}
inline void StateChanged(Address actor,const char* event,const std::array<int,6>& items) {
    if(!enabled)return;
    const auto member=Member(actor);if(!member)return;
    const auto traits=defender_repair::Traits(member);
    int inspiring=-1;
    if(traits.valid){inspiring=0;for(std::size_t i=0;i<traits.size;++i)inspiring|=int(ReadRecord(traits.values[i]).Named("Quirk_InspiringPresence"));}
    const auto seed=At<Address>(member,0x168),identity=At<Address>(member,0x170);
    if(!seed.readable || !identity.readable)return;
    const auto controller=ReadController(actor);int moraleBits=-1;
    if(controller.morale.readable)std::memcpy(&moraleBits,&controller.morale.value,4);
    State state{actor,member,seed.value,identity.value,{items[0],items[1],items[2],items[3],items[4],inspiring,
        controller.mode.readable?static_cast<int>(controller.mode.value):-1,
        controller.flag.readable?int(controller.flag.value):-1,moraleBits,items[5]},controller.overrides};
    if(!Changed(state))return;
    char line[1800]{};
    std::snprintf(line,sizeof(line),"state event=%s actor=%p member=%p terrorGeneral=%d terrified=%d noFleeRemoval=%d inspiredMorale=%d moraleAura=%d inspiringTrait=%d nativeMode=%d nativeFlag20=%d moraleRaw=%.3f moraleReadable=%u fearlessItem=%d overrides=%s",
        event,reinterpret_cast<void*>(actor),reinterpret_cast<void*>(member),items[0],items[1],items[2],items[3],items[4],inspiring,state.values[6],state.values[7],
        double(controller.morale.value),unsigned(controller.morale.readable),items[5],controller.overrides.c_str());
    Emit(line);
}
inline bool Install(bool requested=true) {
    if(!requested)return true;
    if(removeHook || grunt_aggression::inventoryObserver)return false;
    const auto base=ring_might::Base();
    static constexpr unsigned char bytes[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x54,0x24,0x10,0x55,0x56,0x57,0x41,0x54,0x41};
    static constexpr unsigned char has[]={0x48,0x83,0xec,0x28,0x48,0x8b,0xc1,0x48,0x85,0xd2,0x74,0x28};
    if(!defender_repair::Signature(base+0x1C0840,bytes) || !defender_repair::Signature(base+0x24DF7C,has))return false;
    start=GetTickCount64();sequence=0;states.clear();
    removeHook=std::make_unique<PLH::x64Detour>(base+0x1C0840,reinterpret_cast<std::uint64_t>(&RemoveItem),&removeTrampoline);
    if(!removeHook->hook()){removeHook.reset();return false;}
    enabled=true;
    if(grunt_aggression::addHook)grunt_aggression::inventoryObserver=&Observe;
    Emit(grunt_aggression::addHook?"read-only inventory grant/removal and state-change trace installed":"state/removal trace installed; grant observer unavailable (aggression hook disabled)");
    return true;
}
inline bool Remove() {
    if(removeHook && !removeHook->unHook())return false;
    if(grunt_aggression::inventoryObserver==&Observe)grunt_aggression::inventoryObserver=nullptr;
    removeHook.reset();enabled=false;states.clear();return true;
}
}
