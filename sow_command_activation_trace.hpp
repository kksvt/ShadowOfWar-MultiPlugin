#pragma once
#include "sow_grunt_command.hpp"

// Observation only: no I/O, allocation, waits or extra engine calls in detours.
namespace sow::command_activation_trace {
namespace gc=grunt_command;
using RequestFn=Address(__fastcall*)(Address,Address,int,std::uint8_t,float);
using CallbackFn=Address(__fastcall*)(Address,Address);
using FirstResolveFn=Address(__fastcall*)(unsigned);
using SecondResolveFn=Address(__fastcall*)(Address,unsigned,std::uint8_t);
using ActivateFn=Address(__fastcall*)(Address);
inline std::array<std::unique_ptr<PLH::x64Detour>,6> hooks;
inline std::array<std::uint64_t,6> originals{};
inline std::atomic<bool> enabled{false};
inline std::atomic<unsigned long long> dropped{0};
inline std::mutex mutex;
inline constexpr unsigned invalidHandle=0xFFFFFFFFu;
struct Ticket {
    Address actor=0,stats=0;unsigned first=invalidHandle,second=invalidHandle;
    unsigned long long generation=0,batch=0;
    explicit operator bool()const{return actor && stats && generation;}
};
struct State {
    int inactive=-1,pending=-1,guard=-1,suspended=-1,level=-1;
    bool operator==(const State& b)const{return inactive==b.inactive && pending==b.pending && guard==b.guard && suspended==b.suspended && level==b.level;}
    bool operator!=(const State& b)const{return !(*this==b);}
};
enum class Phase:unsigned {Watch,RequestIn,RequestOut,FirstIn,FirstOut,FirstHit,FirstMiss,FirstOther,
    SecondIn,SecondOut,SecondHit,SecondMiss,SecondOther,ActivateIn,ActivateOut,StateChange,PendingWait,Count};
inline const char* Name(Phase p){
    static constexpr const char* names[]={"watch","request-in","request-out","first-in","first-out","first-hit","first-miss","first-other",
        "second-in","second-out","second-hit","second-miss","second-other","activate-in","activate-out","state","pending-wait"};
    const auto i=static_cast<unsigned>(p);return i<static_cast<unsigned>(Phase::Count)?names[i]:"unknown";
}
struct Event {unsigned long long sequence=0;Phase phase=Phase::Watch;State state{};Address value=0;};
struct Watch {
    Ticket ticket{};ULONGLONG expires=0,pendingSince=0;
    State state{};bool stateSeen=false,pendingWarned=false;
    std::array<unsigned long long,static_cast<unsigned>(Phase::Count)> counts{};
    std::array<Event,8> events{};
    unsigned long long sequence=0,reported=0;
};
inline std::array<Watch,8> watches;
inline unsigned long long generation=0;
inline ULONGLONG nextPoll=0,nextFlush=0;
struct Context {Ticket ticket{};unsigned stage=0;};
inline thread_local Context context;
struct Scope {Context previous;Scope(Ticket t,unsigned stage):previous(context){context={t,stage};}~Scope(){context=previous;}};
inline bool Identity(const Ticket& t){
    if(!t || !gc::Live({t.actor,t.stats}))return false;
    const auto handle=At<unsigned>(t.actor,0x2BFC);return handle.readable && handle.value==t.second;
}
inline State ReadState(const Ticket& t){
    State s;if(!Identity(t))return s;
    const auto inactive=At<std::uint8_t>(t.actor,0x2F19),pending=At<std::uint8_t>(t.actor,0x2F1B),guard=At<std::uint8_t>(t.actor,0x2E72);
    if(inactive.readable)s.inactive=inactive.value;
    if(pending.readable)s.pending=pending.value;
    if(guard.readable)s.guard=guard.value;
    const auto slot=At<unsigned>(t.actor,0x2BF8);
    if(slot.readable && slot.value<256){
        const auto entry=ring_might::Base()+0x2A62FF0+0x188+48ull*slot.value;
        const auto owner=Read<Address>(entry);
        if(owner.readable && owner.value==t.actor){
            const auto suspended=At<std::uint8_t>(entry,36);const auto level=At<unsigned>(entry,12);
            if(suspended.readable)s.suspended=suspended.value;
            if(level.readable && level.value<3)s.level=static_cast<int>(level.value);
        }
    }
    return s;
}
inline void Add(Watch& w,Phase phase,State state,Address value=0){
    ++w.counts[static_cast<unsigned>(phase)];const auto seq=++w.sequence;
    w.events[(seq-1)%w.events.size()]={seq,phase,state,value};
}
inline void Record(Ticket t,Phase phase,State state={},Address value=0){
    if(!t || !enabled.load())return;
    std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);
    if(!lock.owns_lock()){++dropped;return;}
    for(auto& w:watches)if(w.ticket.generation==t.generation && w.ticket.actor==t.actor && w.ticket.stats==t.stats){Add(w,phase,state,value);return;}
}
inline Ticket Find(Address actor,unsigned handle,unsigned stage){
    if(!enabled.load())return {};
    std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);
    if(!lock.owns_lock()){++dropped;return {};}
    Ticket result{};const auto now=GetTickCount64();
    for(const auto& w:watches){
        if(!w.ticket || now>w.expires)continue;
        const bool match=stage?(handle!=invalidHandle && (stage==1?w.ticket.first:w.ticket.second)==handle):w.ticket.actor==actor;
        if(!match)continue;
        if(result)return {}; // Ambiguous handle: leave correlation unresolved.
        result=w.ticket;
    }
    return result;
}
inline bool Owned(Address target){
    if(!gc::crowdCommandsActive.load())return false;
    const auto binding=gc::Bind(target);if(!gc::Enemy(binding))return false;
    for(const auto& signal:gc::orderSignals){
        if(signal.target.load()!=target || signal.destroyed.load())continue;
        const gc::Binding source{signal.actor.load(),signal.stats.load()};
        if(!gc::Friendly(source))continue;
        const auto bound=At<Address>(source.actor,0x2EB8);const auto command=At<std::uint8_t>(source.actor,0x2E90);
        if(bound.readable && bound.value==binding.stats && command.readable && command.value &&
           signal.target.load()==target && signal.actor.load()==source.actor && signal.stats.load()==source.stats && !signal.destroyed.load())return true;
    }
    return false;
}
inline Ticket Track(Address actor){
    if(!enabled.load() || !Owned(actor))return {};
    const auto binding=gc::Bind(actor);const auto second=At<unsigned>(actor,0x2BFC);
    const auto component=At<Address>(actor,0x5A0);const auto first=At<unsigned>(component.value,0x2BFC);
    if(!gc::Live(binding) || !second.readable || second.value==invalidHandle)return {};
    const auto now=GetTickCount64();const auto batch=gc::crowdBatch.load();
    const auto state=ReadState({binding.actor,binding.stats,first.readable?first.value:invalidHandle,second.value,1,batch});
    std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);
    if(!lock.owns_lock()){++dropped;return {};}
    Watch* vacant=nullptr;Watch* oldest=&watches[0];
    for(auto& w:watches){
        if(w.ticket.actor==actor && w.ticket.stats==binding.stats && w.ticket.second==second.value && now<=w.expires){w.expires=now+60000;return w.ticket;}
        if((!w.ticket || now>w.expires) && !vacant)vacant=&w;
        if(w.expires<oldest->expires)oldest=&w;
    }
    auto& w=*(vacant?vacant:oldest);if(!vacant)++dropped;
    w={};w.ticket={actor,binding.stats,first.readable?first.value:invalidHandle,second.value,++generation,batch};
    w.expires=now+60000;w.state=state;w.stateSeen=true;Add(w,Phase::Watch,state);return w.ticket;
}
inline Ticket ForActor(Address actor){auto t=Find(actor,invalidHandle,0);if(t && Identity(t))return t;return Track(actor);}
inline Address __fastcall Request(Address actor,Address saved,int mode,std::uint8_t flag,float blend){
    const auto t=ForActor(actor);if(t)Record(t,Phase::RequestIn,ReadState(t));
    const auto result=PLH::FnCast(originals[0],static_cast<RequestFn>(nullptr))(actor,saved,mode,flag,blend);
    if(t)Record(t,Phase::RequestOut,ReadState(t));return result;
}
inline Address __fastcall First(Address manager,Address payload){
    Ticket t{};if(enabled.load()){const auto id=Read<unsigned>(payload);if(id.readable)t=Find(0,id.value,1);}
    Scope scope(t,1);if(t)Record(t,Phase::FirstIn,ReadState(t));
    const auto result=PLH::FnCast(originals[1],static_cast<CallbackFn>(nullptr))(manager,payload);
    if(t)Record(t,Phase::FirstOut,ReadState(t));return result;
}
inline Address __fastcall FirstResolve(unsigned id){
    const auto result=PLH::FnCast(originals[2],static_cast<FirstResolveFn>(nullptr))(id);const auto c=context;
    if(c.stage==1 && c.ticket && id==c.ticket.first){
        // The owner entity is a separate object. Its +98 native handle
        // corresponds to actor+2BFC, as tested by the 161EB0 lookup.
        const auto owner=result?At<Address>(result,0x28):Field<Address>{};
        const auto idOfOwner=At<unsigned>(owner.value,0x98);
        Record(c.ticket,!result?Phase::FirstMiss:idOfOwner.readable && idOfOwner.value==c.ticket.second?Phase::FirstHit:Phase::FirstOther,ReadState(c.ticket),result);
    }
    return result;
}
inline Address __fastcall Second(Address manager,Address handle){
    const auto t=Find(0,static_cast<unsigned>(handle),2);Scope scope(t,2);if(t)Record(t,Phase::SecondIn,ReadState(t));
    const auto result=PLH::FnCast(originals[3],static_cast<CallbackFn>(nullptr))(manager,handle);
    if(t)Record(t,Phase::SecondOut,ReadState(t));return result;
}
inline Address __fastcall SecondResolve(Address manager,unsigned id,std::uint8_t flag){
    const auto result=PLH::FnCast(originals[4],static_cast<SecondResolveFn>(nullptr))(manager,id,flag);const auto c=context;
    if(c.stage==2 && c.ticket && id==c.ticket.second)Record(c.ticket,!result?Phase::SecondMiss:result==c.ticket.actor?Phase::SecondHit:Phase::SecondOther,ReadState(c.ticket),result);
    return result;
}
inline Address __fastcall Activate(Address actor){
    const auto t=ForActor(actor);if(t)Record(t,Phase::ActivateIn,ReadState(t));
    const auto result=PLH::FnCast(originals[5],static_cast<ActivateFn>(nullptr))(actor);
    if(t)Record(t,Phase::ActivateOut,ReadState(t));return result;
}
inline void Report(const char* message){
    gc::Log(message,0,0,true);
    if(!gc::consoleOutput && GetConsoleWindow()){
        std::lock_guard<std::mutex> lock(gc::logMutex);std::fprintf(stdout,"[GruntCommand] %s\n",message);std::fflush(stdout);
    }
}
inline void Tick(ULONGLONG now){
    if(!enabled.load() || now<nextPoll)return;
    nextPoll=now+100;
    if(gc::crowdCommandsActive.load()){
        std::array<Address,8> seen{};std::size_t count=0;
        for(const auto& signal:gc::orderSignals){
            const auto actor=signal.target.load();if(!actor || signal.destroyed.load())continue;
            if(std::find(seen.begin(),seen.begin()+count,actor)!=seen.begin()+count)continue;
            if(count==seen.size())break;
            seen[count++]=actor;if(Track(actor))break;
        }
    }
    std::array<Ticket,8> tickets{};
    {std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);if(!lock.owns_lock())return;
     for(std::size_t i=0;i<watches.size();++i)if(watches[i].ticket && now<=watches[i].expires)tickets[i]=watches[i].ticket;}
    for(const auto& t:tickets){
        if(!t)continue;
        const auto state=ReadState(t);std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);
        if(!lock.owns_lock()){++dropped;continue;}
        for(auto& w:watches)if(w.ticket.generation==t.generation){
            if(!w.stateSeen || state!=w.state){w.state=state;w.stateSeen=true;Add(w,Phase::StateChange,state);}
            if(state.pending==1){
                if(!w.pendingSince)w.pendingSince=now;
                if(!w.pendingWarned && now>=w.pendingSince && now-w.pendingSince>=2000){w.pendingWarned=true;Add(w,Phase::PendingWait,state,now-w.pendingSince);}
            }else{w.pendingSince=0;w.pendingWarned=false;}
        }
    }
    if(now<nextFlush)return;
    nextFlush=now+1000;std::array<Watch,8> output{};
    {std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);if(!lock.owns_lock())return;
     for(std::size_t i=0;i<watches.size();++i)if(watches[i].ticket && watches[i].sequence!=watches[i].reported){output[i]=watches[i];watches[i].reported=watches[i].sequence;}}
    for(const auto& w:output){
        if(!w.ticket)continue;
        char trail[384]{};std::size_t used=0;bool trailTruncated=false;
        const auto begin=w.sequence>w.events.size()?w.sequence-w.events.size()+1:1;
        for(auto seq=begin;seq<=w.sequence;++seq){
            const auto& e=w.events[(seq-1)%w.events.size()];if(e.sequence!=seq)continue;
            const auto n=std::snprintf(trail+used,sizeof(trail)-used,"%s%llu:%s(%d/%d/%d,0x%llX)",used?">":"",seq,Name(e.phase),e.state.inactive,e.state.pending,e.state.guard,static_cast<unsigned long long>(e.value));
            if(n<0 || static_cast<std::size_t>(n)>=sizeof(trail)-used){trailTruncated=true;break;}
            used+=static_cast<std::size_t>(n);
        }
        const auto n=[&](Phase p){return w.counts[static_cast<unsigned>(p)];};char message[896]{};
        std::snprintf(message,sizeof(message),"activation trace v2 target=%p stats=%p epoch=%llu batch=%llu handles=%08X/%08X state=%d/%d/%d selection=%d/%d request=%llu/%llu first=%llu/%llu hit/miss/other=%llu/%llu/%llu second=%llu/%llu hit/miss/other=%llu/%llu/%llu activate=%llu/%llu pendingWait=%llu dropped=%llu historyTruncated=%u events=%s",
            reinterpret_cast<void*>(w.ticket.actor),reinterpret_cast<void*>(w.ticket.stats),w.ticket.generation,w.ticket.batch,w.ticket.first,w.ticket.second,
            w.state.inactive,w.state.pending,w.state.guard,w.state.level,w.state.suspended,n(Phase::RequestIn),n(Phase::RequestOut),n(Phase::FirstIn),n(Phase::FirstOut),n(Phase::FirstHit),n(Phase::FirstMiss),n(Phase::FirstOther),n(Phase::SecondIn),n(Phase::SecondOut),n(Phase::SecondHit),n(Phase::SecondMiss),n(Phase::SecondOther),n(Phase::ActivateIn),n(Phase::ActivateOut),n(Phase::PendingWait),dropped.load(),w.sequence>w.reported+8 || trailTruncated?1u:0u,trail);
        Report(message);
    }
}
inline bool AnyHook(){for(const auto& h:hooks)if(h)return true;return false;}
inline bool Remove(){
    enabled=false;if(gc::activationAuditTick==&Tick)gc::activationAuditTick=nullptr;
    bool ok=true;for(auto& h:hooks){if(h && !h->unHook())ok=false;else h.reset();}return ok;
}
inline bool Install(){
    if(AnyHook() || !gc::updateHook)return false;
    const auto base=ring_might::Base();const std::array<Address,6> rvas{0x3F5704,0x4171B4,0x2FDA00,0x3F5930,0x161E78,0x3F5954};
    const std::array<std::array<std::uint8_t,16>,6> signatures{{
        {0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xEC,0x40,0x41},
        {0x40,0x53,0x48,0x83,0xEC,0x20,0x8B,0x0A,0x48,0x8B,0xDA,0xE8,0x3C,0x68,0xEE,0xFF},
        {0x48,0x83,0xEC,0x28,0x83,0xF9,0xFF,0x75,0x04,0x33,0xC0,0xEB,0x4D,0x48,0x8B,0x05},
        {0x48,0x83,0xEC,0x28,0x45,0x33,0xC0,0xE8,0x3C,0xC5,0xD6,0xFF,0x48,0x85,0xC0,0x74},
        {0x48,0x83,0xEC,0x28,0x83,0xFA,0xFF,0x74,0x26,0x48,0x8B,0x0D,0x20,0xDC,0x59,0x02},
        {0x40,0x53,0x48,0x83,0xEC,0x20,0x48,0x8B,0xD9,0x48,0x81,0xC1,0x72,0x2E,0x00,0x00}
    }};
    for(std::size_t i=0;i<rvas.size();++i){const auto bytes=Read<std::array<std::uint8_t,16>>(base+rvas[i]);if(!bytes.readable || bytes.value!=signatures[i])return false;}
    const std::array<Address,6> detours{reinterpret_cast<Address>(&Request),reinterpret_cast<Address>(&First),reinterpret_cast<Address>(&FirstResolve),reinterpret_cast<Address>(&Second),reinterpret_cast<Address>(&SecondResolve),reinterpret_cast<Address>(&Activate)};
    for(std::size_t i=0;i<rvas.size();++i){hooks[i]=std::make_unique<PLH::x64Detour>(base+rvas[i],detours[i],&originals[i]);if(!hooks[i]->hook()){hooks[i].reset();Remove();return false;}}
    {std::lock_guard<std::mutex> lock(mutex);watches={};generation=0;dropped=0;nextPoll=0;nextFlush=0;}
    enabled=true;gc::activationAuditTick=&Tick;return true;
}
} // namespace sow::command_activation_trace
