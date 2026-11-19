#pragma once
#include "sow_grunt_command.hpp"

// Experimental scoped promotion in a transient native selection list. Never
// write actor suspension flags, global budgets or persistent priority entries.
namespace sow::command_target_priority {
namespace gc=grunt_command;
using PolicyFn=Address(__fastcall*)(Address,Address,Address);
inline std::unique_ptr<PLH::x64Detour> hook;
inline std::uint64_t original=0;
inline std::atomic<bool> enabled{false};
inline std::atomic<unsigned long long> evaluated{0},promoted{0},rejected{0};
struct Candidate {std::array<std::uint8_t,32> bytes{};};
static_assert(sizeof(Candidate)==32);
inline Address Pointer(const Candidate& c,std::size_t offset){Address value=0;std::memcpy(&value,c.bytes.data()+offset,8);return value;}
inline gc::Binding Target() {
    if(!enabled.load() || !gc::crowdCommandsActive.load())return {};
    for(const auto& signal:gc::orderSignals){
        const auto actor=signal.actor.load(),stats=signal.stats.load(),target=signal.target.load();
        if(!actor || !stats || !target || signal.destroyed.load())continue;
        const auto bound=At<Address>(actor,0x2EB8);
        const auto flag=At<std::uint8_t>(actor,0x2E90);
        const auto binding=gc::Bind(target);
        if(!gc::Friendly({actor,stats}) || !gc::Enemy(binding) || !bound.readable || bound.value!=binding.stats || !flag.readable || !flag.value)continue;
        const auto unavailable=At<std::uint8_t>(binding.stats,0x4A0),held=At<std::uint8_t>(target,0x2F1B),excluded=At<std::uint8_t>(target,0x2F1E);
        if(!unavailable.readable || !held.readable || !excluded.readable || unavailable.value || held.value || excluded.value)continue;
        if(signal.actor.load()==actor && signal.stats.load()==stats && signal.target.load()==target && !signal.destroyed.load())return binding;
    }
    return {};
}
// RAII also restores the list if a mocked/native callee throws a C++ exception.
struct Promotion {
    Address list=0;unsigned count=0;bool active=false;
    std::array<Candidate,256> saved{};
    Promotion(Address input,Address cut,gc::Binding target,Address manager=ring_might::Base()+0x2A62FF0) {
        if(!target.actor || !target.stats)return;
        const auto size=Read<unsigned>(input);
        if(!size.readable || !size.value || size.value>saved.size())return;
        const Address first=input+8,last=first+32ull*size.value;
        if(cut<=first || cut>last || (cut-first)%32)return; // Keep native cutoff, including zero-slot refusal.
        const auto raw=Read<std::array<std::uint8_t,32>>(first);
        if(!raw.readable)return;
        SIZE_T got=0;
        if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(first),saved.data(),32ull*size.value,&got) || got!=32ull*size.value)return;
        if(!gc::Live(target))return;
        const auto actorSlot=At<unsigned>(target.actor,0x2BF8);
        if(!actorSlot.readable || actorSlot.value>=256)return;
        const Address expectedEntry=manager+0x188+48ull*actorSlot.value;
        const auto slotOwner=Read<Address>(expectedEntry);
        if(!slotOwner.readable || slotOwner.value!=target.actor)return;
        unsigned found=size.value;
        for(unsigned i=0;i<size.value;++i){
            const auto entry=Pointer(saved[i],0);
            if(entry!=expectedEntry)continue;
            if(found!=size.value)return; // Ambiguous duplicate: leave native list untouched.
            found=i;
        }
        if(found==size.value || saved[found].bytes[25])return;
        // The native policy reads state 2 as the full-AI candidate class. Put
        // the owned target first so native quota allocation considers it first.
        // Preserve every other candidate's relative ordering.
        auto chosen=saved[found];chosen.bytes[26]=2;
        list=first;count=size.value;active=true;
        auto* output=reinterpret_cast<Candidate*>(first);
        if(found)std::memmove(output+1,output,32ull*found);
        std::memcpy(output,&chosen,sizeof(chosen));
    }
    ~Promotion(){if(active)std::memcpy(reinterpret_cast<void*>(list),saved.data(),32ull*count);}
};
inline Address __fastcall Detour(Address list,Address cut,Address configuration) {
    const auto target=Target();
    Promotion promotion(list,cut,target);
    if(target.actor){++evaluated;if(promotion.active)++promoted;else ++rejected;}
    return PLH::FnCast(original,static_cast<PolicyFn>(nullptr))(list,cut,configuration);
}
inline bool Remove(){enabled=false;if(hook && !hook->unHook())return false;hook.reset();return true;}
inline bool Install() {
    if(hook || !gc::updateHook)return false;
    const auto site=ring_might::Base()+0xF2FF00;
    const auto bytes=Read<std::array<std::uint8_t,16>>(site);
    if(!bytes.readable || bytes.value!=std::array<std::uint8_t,16>{0x4C,0x89,0x44,0x24,0x18,0x48,0x89,0x54,0x24,0x10,0x48,0x89,0x4C,0x24,0x08,0x53})return false;
    hook=std::make_unique<PLH::x64Detour>(site,reinterpret_cast<Address>(&Detour),&original);
    if(!hook->hook()){hook.reset();return false;}
    evaluated=0;promoted=0;rejected=0;enabled=true;return true;
}
} // namespace sow::command_target_priority
