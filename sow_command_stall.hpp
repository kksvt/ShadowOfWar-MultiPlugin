#pragma once
#ifdef SOW_ENABLE_COMMAND_RAGE_RECOVERY
#include "sow_grunt_command.hpp"
namespace sow::command_stall {
namespace gc=grunt_command;
using ApplyFn=void(__fastcall*)(Address,Address,Address,Address);
inline ApplyFn apply=nullptr;
inline Address buff=0;
inline bool enabled=false,enrage=true;
inline unsigned stallDelayMs=3000;
inline unsigned releasedCount=0,rageCount=0,reportedReleased=0,reportedRage=0;
inline ULONGLONG nextReport=0,nextResolve=0;
struct Timer {
    Address actor=0,target=0;unsigned long long batch=0;ULONGLONG issued=0,since=0,last=0;unsigned progress=0;
    bool armed=false;
    bool Step(ULONGLONG now,float distanceSquared,bool idle,unsigned current) noexcept {
        if(!idle || !std::isfinite(distanceSquared) || distanceSquared<0 || distanceSquared>=200.f*200.f ||
           (last && (now<last || now-last>500)) || current!=progress) {armed=false;since=0;}
        progress=current;last=now;
        if(!idle || !std::isfinite(distanceSquared) || distanceSquared<0 || distanceSquared>=200.f*200.f)return false;
        if(!armed && distanceSquared<150.f*150.f){armed=true;since=now;}
        return armed && now>=since && now-since>=stallDelayMs;
    }
};
inline std::array<Timer,gc::maxOrders> timers{};
using LinkFn=Address(__fastcall*)(Address,Address,unsigned,const Address*);
inline const char* rageFailure="not attempted";
inline Address AffixBuff(Address record,Address schema,LinkFn get) {
    // Native schema fields are 72 bytes; getter descriptors begin at +32.
    // Affix Buff is field 9: 9*72+32=0x2A8 (Proc field 22 is +0x650).
    const auto hash=At<std::uint32_t>(schema,0x2A8);
    const auto kind=At<unsigned>(schema,0x2AC);
    if(!record || !get || !hash.readable || hash.value!=0x5D94FA03 || !kind.readable || kind.value!=10)return 0;
    const Address fallback=0;return get(record,schema+0x2A8,0,&fallback);
}
inline Address ResolveBuff() {
    const auto donor=ring_might::Lookup(0x2700488,"5Cape_Base_Chameleon");
    if(!donor){rageFailure="donor unavailable";return 0;}
    const auto schema=At<Address>(ring_might::Base(),0x264CF60);
    const auto seed=AffixBuff(donor,schema.value,reinterpret_cast<LinkFn>(ring_might::Base()+0x115AE24));
    rageFailure="donor Buff link/schema failed";
    if(!ReadRecord(seed).Named("Equip_5Cape_Chameleon"))return 0;
    rageFailure="buff table unavailable";
    const auto table=At<Address>(seed,0x18);
    const auto count=At<unsigned>(table.value,0x28);const auto entries=At<Address>(table.value,0x38);
    if(!table.readable || !table.value || !count.readable || !count.value || count.value>65536 || !entries.readable || !entries.value)return 0;
    using Hash=std::uint32_t(__fastcall*)(const char*,unsigned,unsigned);
    using Find=Address(__fastcall*)(Address,const std::uint32_t*,unsigned);
    const auto base=ring_might::Base();const auto hash=reinterpret_cast<Hash>(base+0x423E70)("AI_Rage_NonCatalyst",0,0);
    const auto result=reinterpret_cast<Find>(base+0x278540)(entries.value,&hash,count.value);
    const auto owner=At<Address>(result,0x18);
    const bool valid=owner.readable && owner.value==table.value && ReadRecord(result).Named("AI_Rage_NonCatalyst");
    rageFailure=valid?"ready":"exact rage record missing";return valid?result:0;
}
struct BuffScan {bool complete=false,anyRage=false;Address slot=0,instance=0;};
inline BuffScan Scan(Address stats,Address wanted=0) {
    BuffScan out;const auto begin=At<Address>(stats,0x1A0),end=At<Address>(stats,0x1A8);
    if(!begin.readable || !end.readable || end.value<begin.value || (end.value-begin.value)%8 || (end.value-begin.value)/8>512 || (!begin.value && end.value))return out;
    for(auto at=begin.value;at<end.value;at+=8){
        const auto instance=Read<Address>(at);if(!instance.readable)return out;if(!instance.value)continue;
        const auto record=At<Address>(instance.value,0x20);if(!record.readable)return out;
        const auto name=ReadRecord(record.value);
        if(!name.nameComplete)return out;
        if(name.Named("AI_Rage") || name.Named("AI_Rage_NonCatalyst") || name.Named("AI_Rage_LowHealth") || name.Named("AI_Rage_War"))out.anyRage=true;
        if(wanted && instance.value==wanted && record.value==buff){out.slot=at;out.instance=instance.value;}
        if(!wanted && record.value==buff){out.slot=at;out.instance=instance.value;}
    }
    out.complete=true;return out;
}
inline void Report(const char* text) {
    gc::Log(text,0,0,true);
    if(!gc::consoleOutput && GetConsoleWindow()){
        std::lock_guard<std::mutex> lock(gc::logMutex);
        std::fprintf(stdout,"[GruntCommand] %s\n",text);std::fflush(stdout);
    }
}
inline void Tick(ULONGLONG now) {
    if(enabled && enrage && apply && !buff && now>=nextResolve){
        nextResolve=now+2000;buff=ResolveBuff();
        if(buff)Report("native rage resolved; stalled-order enrage enabled");
    }
    if(enabled && now>=nextReport && (releasedCount!=reportedReleased || rageCount!=reportedRage)){
        char text[160]{};std::snprintf(text,sizeof(text),"stall recovery totals: released=%u nativeRage=%u",releasedCount,rageCount);
        Report(text);reportedReleased=releasedCount;reportedRage=rageCount;nextReport=now+1000;
    }
}
inline void Rage(const gc::Binding& unit,ULONGLONG now) {
    (void)now;
    if(!enrage || !apply || !buff || !gc::Friendly(unit))return;
    const auto before=Scan(unit.stats);if(!before.complete || before.anyRage)return;
    // Native lifetime: Trait_RageDuration_NonCatalyst returns 5 seconds for
    // IsNotFactionMember, otherwise 10, with an EndlessRage multiplier.
    // Do not remove by a plugin timer: native refreshes may extend this effect.
    apply(unit.stats,buff,0,0);
    if(!gc::Friendly(unit))return;
    const auto after=Scan(unit.stats);if(!after.complete || !after.instance)return;
    ++rageCount;gc::Log("stall fallback: native rage present; native duration",unit.actor);
}
inline bool Observe(std::size_t slot,ULONGLONG now,Address player) {
    if(!enabled)return false;
    auto& a=gc::orders[slot];auto& timer=timers[slot];
    if(timer.actor!=a.unit.actor || timer.target!=a.target.actor || timer.issued!=a.issuedAt || timer.batch!=gc::crowdBatch.load()){timer={};timer.actor=a.unit.actor;timer.target=a.target.actor;timer.issued=a.issuedAt;timer.batch=gc::crowdBatch.load();}
    if(timer.last && now>=timer.last && now-timer.last<100)return false;
    // Borrowed native buddies are never altered by this fallback.
    if(!a.ownsItem || !gc::Friendly(a.unit) || !gc::Enemy(a.target) || !gc::native.state || !gc::native.blocked){timer.armed=false;return false;}
    const auto entry=gc::native.entry(a.unit.actor);
    const auto buddySetup=ReadRecord(At<Address>(entry,8).value);
    const auto state=At<std::uint8_t>(entry,0x80);
    const auto bound=At<Address>(a.unit.actor,0x2EB8);
    const auto attack=At<Address>(a.unit.stats,0xE0);
    const auto stateRecord=gc::native.state(a.unit.actor);
    const auto blocked=At<std::uint8_t>(stateRecord,0x66);
    const bool idle=buddySetup.Named("Bodyguard_DefaultOrc") && state.readable && state.value==2 &&
        bound.readable && bound.value==a.target.stats && attack.readable && !attack.value && blocked.readable && !blocked.value && !gc::CommandBlocked(a.unit.actor);
    if(!timer.Step(now,gc::DistanceSquared(a.unit.actor,a.target.actor),idle,gc::orderSignals[slot].progress.load()))return false;
    const auto unit=a.unit,target=a.target;timer={};
    // Native transition removes command-mode items/playbook; entry may relocate.
    gc::native.mode(gc::native.manager(),entry,1,player,0);
    const auto fresh=gc::native.entry(unit.actor);const auto after=At<std::uint8_t>(fresh,0x80);
    if(!fresh || !after.readable || after.value==2){gc::Log("stall fallback: native release unconfirmed",unit.actor,target.actor);return true;}
    gc::ReleaseUnit(slot);++releasedCount;gc::Log("stall fallback: configured idle delay reached; command released",unit.actor,target.actor);
    Rage(unit,now);return true;
}
inline void Shutdown(){enabled=false;timers={};}
inline bool Install(bool release=true,bool rage=true,unsigned delayMs=3000) {
    if(!gc::updateHook)return false;
    stallDelayMs=delayMs;enabled=release;enrage=rage;buff=0;apply=nullptr;
    timers={};releasedCount=0;rageCount=0;reportedReleased=0;reportedRage=0;nextReport=0;nextResolve=0;
    const auto base=ring_might::Base();
    const auto signature=Read<std::array<std::uint8_t,9>>(base+0x36A35C);
    rageFailure=rage?"application signature mismatch":"disabled by configuration";
    if(rage && signature.readable && signature.value==std::array<std::uint8_t,9>{0x48,0x85,0xD2,0x0F,0x84,0x23,0x0A,0,0}){
        apply=reinterpret_cast<ApplyFn>(base+0x36A35C);buff=ResolveBuff();
    }
    if(rage && !buff){char text[180]{};std::snprintf(text,sizeof(text),"rage initialization unavailable: %s",rageFailure);Report(text);}
    gc::stallObserver=&Observe;gc::stallTick=&Tick;gc::stallShutdown=&Shutdown;
    char text[240]{};
    std::snprintf(text,sizeof(text),"stall fallback installed: 1.5m enter / 2m exit, idleMs=%u, %s",stallDelayMs,
        buff?"release + native rage":"release only (rage disabled or unavailable)");
    Report(text);
    return true;
}
} // namespace sow::command_stall
#endif // SOW_ENABLE_COMMAND_RAGE_RECOVERY

