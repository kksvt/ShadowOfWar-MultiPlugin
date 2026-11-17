#pragma once
#include "sow_entities.hpp"
#include "sow_defender_slots.hpp"
#include "sow_defender_vulnerability_policy.hpp"
#include "sow_defender_ability_policy.hpp"
#include "sow_defender_provenance.hpp"
#include "sow_defender_actor_policy.hpp"
#include "sow_defender_blacklist.hpp"
#include "sow_defender_trait_policy.hpp"
#include "sow_defender_class_policy.hpp"
#include "sow_defender_structure_policy.hpp"
#include <polyhook2/Detour/x64Detour.hpp>
#include <polyhook2/Misc.hpp>
#include <memory>
#include <cstdio>
#include <vector>
#include <string>
#include <mutex>
#include <atomic>
#include <utility>

namespace sow::defender_repair {
// Experimental: enable before opening defender cards. Disabled installs no hook.
// Public feature settings. Imported-roster activation and repair policy are automatic.
struct Config { bool enabled=false; bool diagnostics=true; bool forceTerrorPlayer=true; };
using RemoveFn=bool(__fastcall*)(Address,Address);
using LookupFn=Address(*)(Address,const char*);
using CapacityFn=bool(*)(Address,Address,const std::vector<Address>&);
inline RemoveFn addTrait=nullptr;
inline LookupFn lookupTrait=nullptr;
inline CapacityFn hasCapacity=nullptr;
inline char capacityReport[160]{};
struct Mark {Address member,key,seed,identity;};
inline std::vector<Mark> marked;
struct Threat {Address actor,member,key,seed,identity;};
using ThreatSnapshot=std::shared_ptr<const std::vector<Threat>>;
#if defined(__cpp_lib_atomic_shared_ptr) && __cpp_lib_atomic_shared_ptr >= 201711L
inline std::atomic<ThreatSnapshot> threats{std::make_shared<const std::vector<Threat>>()};
inline ThreatSnapshot LoadThreats(){return threats.load();}
inline void StoreThreats(ThreatSnapshot value){threats.store(std::move(value));}
#else
inline ThreatSnapshot threats=std::make_shared<const std::vector<Threat>>();
inline ThreatSnapshot LoadThreats(){return std::atomic_load(&threats);}
inline void StoreThreats(ThreatSnapshot value){std::atomic_store(&threats,std::move(value));}
#endif
// Preserve the formatter's machine return value, even though inspected callers
// do not consume it. Hook the full routine, never its interior CE AOB site.
using FormatFn=Address(__fastcall*)(Address,Address,bool);
inline Config config;
inline RemoveFn removeTrait=nullptr;
inline Address image=0;
inline std::unique_ptr<PLH::x64Detour> hook;
inline std::unique_ptr<PLH::x64Detour> spawnHook;
inline std::uint64_t spawnTrampoline=0;
using SpawnFn=Address(__fastcall*)(Address,Address,bool);
inline std::string armedFaction;
inline std::mutex stateMutex;
inline thread_local Address traceBindingActor=0,traceBindingMember=0;
inline void(*onFlaggedBound)(Address,Address)=nullptr;
inline void(*beforeRosterRepair)(Address)=nullptr;
inline void(*onFormattedMember)(Address)=nullptr; // Read-only export observer; independent of console diagnostics.
inline void(*beforeImportedRepair)(Address)=nullptr; // Called with stateMutex held.
inline std::uint64_t trampoline=0;
inline thread_local bool handling=false;
inline ULONGLONG lastBatch=0;
inline Address lastBatchKey=0;
struct DiagnosticStamp {Address member;std::array<Address,3> identity;std::string event,trait;ULONGLONG time;};
inline std::mutex diagnosticMutex;
inline std::vector<DiagnosticStamp> diagnosticStamps;
inline bool ShouldLog(Address member,const char* event,const char* trait,ULONGLONG now){
    const auto token=At<std::array<Address,3>>(member,0x168);
    std::lock_guard<std::mutex> lock(diagnosticMutex);
    for(auto& stamp:diagnosticStamps)if(stamp.member==member && stamp.identity==token.value && stamp.event==event && stamp.trait==trait){
        if(now>=stamp.time && now-stamp.time<30000)return false;stamp.time=now;return true;
    }
    if(diagnosticStamps.size()>=2048)diagnosticStamps.erase(diagnosticStamps.begin());
    diagnosticStamps.push_back({member,token.value,event,trait,now});return true;
}
inline void Log(const char* event,Address member,const char* trait="",bool /*summary*/=false) {
    if(!config.diagnostics || !ShouldLog(member,event,trait,GetTickCount64()))return;
    char out[512]{};
    std::snprintf(out,sizeof(out),"[DefenderRepair] %s member=%p trait=%s\n",event,reinterpret_cast<void*>(member),trait);
    OutputDebugStringA(out);
    if(GetConsoleWindow()){std::fputs(out,stdout);std::fflush(stdout);}
}
template<std::size_t N> inline bool Matches(const Record& r,const char* const (&names)[N]) {
    for(const auto* n:names)if(n && r.Named(n))return true;
    return false;
}
inline bool Blacklisted(const Record& r) {return Matches(r,blacklistedTraits);}
inline bool InvalidTribe(const Record& r){
    // A corrupt/unnamed record is unknown. Every other named tribe must be in
    // the exact stock allowlist; a missing pointer is handled by Normalize.
    return r.nameComplete && r.name[0] && !Matches(r,allowedDefenderTribes);
}
struct List {std::array<Address,512> values{};std::size_t size=0;bool valid=false;};
inline unsigned EpicCount(const List& list);
inline List Traits(Address member,Address listOffset=0x198) {
    List list;
    const auto begin=At<Address>(member,listOffset),end=At<Address>(member,listOffset+8);
    if(!begin.readable || !end.readable || end.value<begin.value || (end.value-begin.value)%8)return list;
    list.size=(end.value-begin.value)/8;
    if(list.size>list.values.size() || (list.size && !begin.value))return list;
    for(std::size_t i=0;i<list.size;++i){
        const auto p=Read<Address>(begin.value+i*8);
        if(!p.readable || !p.value || !ReadRecord(p.value).nameComplete)return list;
        list.values[i]=p.value;
    }
    const auto b=At<Address>(member,listOffset),e=At<Address>(member,listOffset+8);
    list.valid=b.readable && e.readable && b.value==begin.value && e.value==end.value;
    return list;
}
inline List PolicyTraits(Address member){
    auto list=Traits(member);if(!list.valid || !defender_slots::erase)return list;
    const auto slots=defender_slots::ReadSlots(member);if(!slots.valid){list.valid=false;return list;}
    for(std::size_t i=0;i<slots.size;++i)for(const auto trait:{slots.slots[i].trait}){
        if(!trait)continue;if(!ReadRecord(trait).nameComplete){list.valid=false;return list;}
        bool present=false;for(std::size_t j=0;j<list.size;++j)present|=list.values[j]==trait;
        if(!present){if(list.size==list.values.size()){list.valid=false;return list;}list.values[list.size++]=trait;}
    }
    return list;
}
inline const char* ScopeReason(Address member) {
    // Imported, unspawned roster scope is independent of character taxonomy.
    static constexpr const char* factions[]={"InvasionFaction_Island","InvasionFaction_MtnPass",
        "InvasionFaction_SnowMt","InvasionFaction_desert","InvasionFaction_Volcano","InvasionFaction_MinasIthil"};
    const auto actor=At<Address>(member,0x48),key=At<Address>(member,0x178);
    const auto globals=At<Address>(image,offset::globalsRva),manager=At<Address>(globals.value,0x6C68);
    const auto owner=At<Address>(manager.value,0xA8),tag=At<Address>(key.value,0x18);
    const auto row=At<Address>(member,0x268);
    if(!actor.readable)return "actor-link-unreadable";
    if(actor.value)return "actor-already-spawned";
    if(!key.readable || !key.value)return "faction-unavailable";
    if(!owner.readable || !owner.value || !tag.readable)return "owner-unavailable";
    if(owner.value!=tag.value)return "faction-owner-mismatch";
    if(!Matches(ReadRecord(key.value),factions))return "not-allowed-invasion-faction";
    if(!row.readable || !ReadRecord(row.value).nameComplete)return "row-unreadable-or-unnamed";
    return nullptr;
}
inline const char* SkipReason(Address member) {
    if(const auto* reason=ScopeReason(member))return reason;
    // Shared stock class table; normalization runs separately before stored
    // cleanup. This predicate itself never converts a model.
    const auto cls=At<Address>(member,0x278);const auto record=ReadRecord(cls.value);
    if(!cls.readable || !record.nameComplete)return "class-unreadable-or-unnamed";
    return StandardClass(record)?nullptr:"unsupported-class";
}
inline bool Eligible(Address member) {return SkipReason(member)==nullptr;}
struct StructuralField {const char* field;Record record;bool readable;bool incompatible;bool nullRecord;};
inline std::array<StructuralField,7> InspectStructure(Address member) {
    std::array<StructuralField,7> fields{};
    const auto storage=At<Address>(member,0x2D8);
    const Address offsets[]={0x278,0x188,0x318,0x20,0x28,0x30,0x328};
    const char* names[]={"class","ai","role","visualBase","visualBody","visualHead","tribe"};
    for(std::size_t i=0;i<fields.size();++i){
        const bool memberField=i<3 || i==6;
        const bool baseKnown=memberField || (storage.readable && storage.value);
        const auto ptr=At<Address>(memberField?member:storage.value,offsets[i]);
        auto& f=fields[i];f.field=names[i];f.record=ReadRecord(ptr.value);
        f.nullRecord=baseKnown && ptr.readable && !ptr.value;
        f.readable=baseKnown && ptr.readable && ptr.value && f.record.nameComplete;
        if(!f.readable)continue;
        if(i==0)f.incompatible=!StandardClass(f.record);
        else if(i==1){const auto model=ReadRecord(At<Address>(member,0x278).value);const auto pair=StandardClass(model);
            f.incompatible=pair?!f.record.Named(pair->ai):!StandardAI(f.record);}
        else if(i==2)f.incompatible=Matches(f.record,structuralRoles);
        else if(i==6)f.incompatible=InvalidTribe(f.record);
        else f.incompatible=Matches(f.record,structuralVisuals);
    }
    return fields;
}
inline std::atomic<std::uint64_t> memberDumpSequence{0};
inline void WriteDiagnostic(const char* text) {
    OutputDebugStringA(text);
    if(GetConsoleWindow()){std::fputs(text,stdout);std::fflush(stdout);}
}
inline void LogMemberList(Address member,const char* kind,const List& list,std::uint64_t snapshot) {
    char line[512]{};
    if(!list.valid){
        std::snprintf(line,sizeof(line),"[DefenderTraits] snapshot=%llu member=%p kind=%s status=incomplete-or-invalid; entries unavailable\n",
            static_cast<unsigned long long>(snapshot),reinterpret_cast<void*>(member),kind);
        WriteDiagnostic(line);return;
    }
    std::snprintf(line,sizeof(line),"[DefenderTraits] snapshot=%llu member=%p kind=%s count=%zu status=complete\n",
        static_cast<unsigned long long>(snapshot),reinterpret_cast<void*>(member),kind,list.size);
    WriteDiagnostic(line);
    for(std::size_t i=0;i<list.size;++i){
        const auto record=ReadRecord(list.values[i]);
        std::snprintf(line,sizeof(line),"[DefenderTraits] snapshot=%llu member=%p kind=%s index=%zu record=%p name=%s nameComplete=%u\n",
            static_cast<unsigned long long>(snapshot),reinterpret_cast<void*>(member),kind,i,
            reinterpret_cast<void*>(list.values[i]),record.nameComplete?record.name.data():"<unreadable-or-changed>",unsigned(record.nameComplete));
        WriteDiagnostic(line);
    }
}
inline void LogMember(Address member,const char* reason,bool transition=false) {
    if(!config.diagnostics)return;
    // Full traits/markers/structure live in JSONL, not synchronous console loops.
    if(!transition){Log(reason,member);return;}
    if(!ShouldLog(member,reason,"member-summary",GetTickCount64()))return;
    const auto snapshot=memberDumpSequence.fetch_add(1,std::memory_order_relaxed)+1;
    const auto cls=ReadRecord(At<Address>(member,0x278).value);
    const auto role=ReadRecord(At<Address>(member,0x318).value);
    const auto row=ReadRecord(At<Address>(member,0x268).value);
    const auto actor=At<Address>(member,0x48);
    const auto hash=At<unsigned>(member,0x170),level=At<unsigned>(member,0x4B8);
    const auto list=Traits(member);int blocked=-1;
    if(list.valid){blocked=0;for(std::size_t i=0;i<list.size;++i)if(Blacklisted(ReadRecord(list.values[i])))++blocked;}
    int awe=-1,fearless=-1;
    if(list.valid){awe=fearless=0;for(std::size_t i=0;i<list.size;++i){
        const auto record=ReadRecord(list.values[i]);awe|=int(record.Named("Terror_Player"));fearless|=int(record.Named("Quirk_Fearless"));}}
    char out[1200]{};
    std::snprintf(out,sizeof(out),
        "[DefenderRepair] member-detail reason=%s member=%p actor=%p actorReadable=%u nameHash=0x%08X hashReadable=%u levelRaw=%u levelReadable=%u class=%s role=%s row=%s blacklisted=%d epicCount=%u awe=%d fearless=%d snapshot=%llu\n",
        reason,reinterpret_cast<void*>(member),reinterpret_cast<void*>(actor.value),unsigned(actor.readable),
        hash.value,unsigned(hash.readable),level.value,unsigned(level.readable),
        cls.nameComplete?cls.name.data():"<unknown>",role.nameComplete?role.name.data():"<unknown>",
        row.nameComplete?row.name.data():"<unknown>",blocked,EpicCount(list),awe,fearless,static_cast<unsigned long long>(snapshot));
    WriteDiagnostic(out);

}

inline Address LookupTrait(Address exemplar,const char* name) {
    // Exact category owner from a verified trait; same hashed table lookup as
    // the working ring feature. Never use record addresses from a previous run.
    const auto owner=At<Address>(exemplar,0x18),entries=At<Address>(owner.value,0x38);
    const auto count=At<unsigned>(owner.value,0x28);
    if(!owner.readable || !owner.value || !entries.readable || !entries.value ||
       !count.readable || !count.value || count.value>16384)return 0;
    using Hash=unsigned(__fastcall*)(const char*,Address,unsigned);
    using Find=Address(__fastcall*)(Address,const unsigned*,unsigned);
    const auto hash=reinterpret_cast<Hash>(image+0x423E70)(name,0,0);
    const auto result=reinterpret_cast<Find>(image+0x278540)(entries.value,&hash,count.value);
    const auto tag=At<Address>(result,0x18);
    return tag.readable && tag.value==owner.value && ReadRecord(result).Named(name)?result:0;
}
inline bool Capacity(Address member,Address added,const std::vector<Address>& removed) {
    std::snprintf(capacityReport,sizeof(capacityReport),"list/record unavailable");
    const auto list=Traits(member);if(!list.valid || !added)return false;
    unsigned total=0,counted=0;
    using Counted=bool(__fastcall*)(Address);
    const auto counts=reinterpret_cast<Counted>(image+0x1BBC01C);
    for(std::size_t i=0;i<list.size;++i){
        if(list.values[i]==added)return true;
        bool excluded=false;for(auto p:removed)excluded|=p==list.values[i];
        if(!excluded){++total;if(counts(list.values[i]))++counted;}
    }
    std::snprintf(capacityReport,sizeof(capacityReport),"planned total=%u; maximum before addition=63",total);
    if(total>=64)return false;
    if(counts(added))++counted;
    const auto schema=At<Address>(image,0x266B6F0);
    const auto mode=At<int>(schema.value,0x660);
    const auto key=At<Address>(member,0x178);
    if(!schema.readable || !mode.readable || !key.readable || !key.value)return false;
    using IntAttr=unsigned(__fastcall*)(Address,Address,unsigned,const unsigned*);
    const unsigned fallback=10;
    unsigned limit=fallback;
    std::snprintf(capacityReport,sizeof(capacityReport),"mode=%d; optimized attribute read unavailable",mode.value);
    if(mode.value>=2){
        // Native 3550F0/355104 -> 278758 fast attribute-index path.
        const auto index=At<unsigned char>(schema.value,0x670);
        const auto attrs=At<Address>(key.value,8),values=At<Address>(key.value,0x10);
        const auto attrCount=Read<unsigned char>(key.value);
        if(!index.readable || !attrs.readable || !attrs.value || !values.readable || !values.value ||
           !attrCount.readable || index.value>=attrCount.value)return false;
        const auto descriptor=attrs.value+static_cast<Address>(index.value)*16;
        const auto kind=At<unsigned char>(descriptor,0xA);
        const auto size=At<unsigned short>(descriptor,0xC),start=At<unsigned short>(descriptor,0xE);
        if(!kind.readable || (kind.value&0x3F)!=3 || !size.readable || !start.readable)return false;
        if(size.value){const auto value=Read<unsigned>(values.value+static_cast<Address>(start.value)*8);
            if(!value.readable)return false;limit=value.value;}
    }else limit=reinterpret_cast<IntAttr>(image+0x1169594)(key.value,schema.value+0x650,0,&fallback);
    std::snprintf(capacityReport,sizeof(capacityReport),"mode=%d plannedTotal=%u counted=%u subsetLimit=%u",mode.value,total+1,counted,limit);
    return limit<=512 && counted<=limit;
}
inline bool Has(const List& list,Address trait) {
    for(std::size_t i=0;i<list.size && list.valid;++i)if(list.values[i]==trait)return true;
    return false;
}
// Native active deactivation followed by persistent picker removal. Tests may
// leave slot helpers unset to exercise the legacy active-only operation.
inline bool RemoveStoredTrait(Address member,Address trait){
    if(!removeTrait || !Eligible(member))return false;
    if(defender_slots::erase && (!defender_slots::ReadSlots(member).valid || !defender_slots::Editable(member)))return false;
    const auto before=Traits(member);if(!before.valid)return false;
    if(Has(before,trait) && !removeTrait(member,trait))return false;
    const auto after=Traits(member);if(!after.valid || Has(after,trait))return false;
    return !defender_slots::erase || defender_slots::EraseTraitSlots(member,trait);
}
inline bool ReconcileBaseSlot(Address member){
    if(!Eligible(member) || !lookupTrait || !defender_slots::add || !defender_slots::update || !defender_slots::Editable(member))return false;
    auto slots=defender_slots::ReadSlots(member);const auto* name=defender_slots::BasePicker(ReadRecord(At<Address>(member,0x278).value));
    if(!slots.valid || !slots.size || !name)return false;
    Address exemplar=0;for(std::size_t i=0;i<slots.size;++i)if(ReadRecord(slots.slots[i].picker).nameComplete){exemplar=slots.slots[i].picker;break;}
    const auto wanted=lookupTrait(exemplar,name);if(!ReadRecord(wanted).Named(name))return false;
    bool found=false;for(std::size_t i=0;i<slots.size;++i)found|=slots.slots[i].picker==wanted;
    // Add first: a failed native admission must not strip the old class bonus.
    if(!found){
        if(slots.size>=512 || !defender_slots::add(member,wanted))return false;
        slots=defender_slots::ReadSlots(member);if(!slots.valid)return false;
        found=false;for(std::size_t i=0;i<slots.size;++i)found|=slots.slots[i].picker==wanted;
        if(!found)return false;
    }
    bool kept=false;
    for(std::size_t i=slots.size;i>0;--i){const auto& slot=slots.slots[i-1];
        if(!defender_slots::IsBasePicker(ReadRecord(slot.picker)))continue;
        if(slot.picker==wanted && !kept){kept=true;continue;}
        const auto traits=Traits(member);if(!traits.valid)return false;
        if(slot.trait && Has(traits,slot.trait) && (!removeTrait || !removeTrait(member,slot.trait)))return false;
        if(!defender_slots::Erase(member,static_cast<unsigned>(i-1)))return false;
    }
    if(!Eligible(member))return false;
    defender_slots::update(member,0);
    const auto after=defender_slots::ReadSlots(member);unsigned bases=0;bool correct=false;
    for(std::size_t i=0;after.valid && i<after.size;++i)if(defender_slots::IsBasePicker(ReadRecord(after.slots[i].picker))){
        ++bases;correct=after.slots[i].picker==wanted && after.slots[i].trait && Has(Traits(member),after.slots[i].trait);}
    return after.valid && bases==1 && correct;
}
inline bool PreserveResolvedSlot(Address member,Address awe,Address picker,std::int16_t option){
    if(!defender_slots::push)return true;
    if(!Eligible(member) || !defender_slots::Editable(member) || !ReadRecord(picker).nameComplete)return false;
    auto slots=defender_slots::ReadSlots(member);if(!slots.valid)return false;
    // A zero gate word makes the native resolver retain the cached trait for
    // every rebuild reason. This cache is deliberately not a save-file claim.
    bool kept=false;
    for(std::size_t i=slots.size;i>0;--i){const auto& slot=slots.slots[i-1];if(slot.picker!=picker)continue;
        if(!kept && slot.trait==awe && slot.flags==0 && slot.option==option && !slot.unlocked){kept=true;continue;}
        if(slot.trait && slot.trait!=awe){const auto active=Traits(member);
            if(!active.valid || (Has(active,slot.trait) && (!removeTrait || !removeTrait(member,slot.trait))))return false;}
        if(!defender_slots::Erase(member,static_cast<unsigned>(i-1)))return false;
    }
    if(kept)return true;
    const auto before=defender_slots::ReadSlots(member);if(!before.valid || before.size>=512)return false;
    const defender_slots::Slot forced{picker,awe,0,0,option,0};
    if(!defender_slots::push(member+0x1E0,&forced))return false;
    const auto after=defender_slots::ReadSlots(member);
    return after.valid && after.size==before.size+1 && after.slots[after.size-1].picker==picker &&
        after.slots[after.size-1].trait==awe && after.slots[after.size-1].flags==0;
}
inline bool PreserveTraitSlot(Address member,Address awe,const char* pickerName,std::int16_t option){
    if(!defender_slots::push)return true;
    if(!lookupTrait || !Eligible(member) || !defender_slots::Editable(member))return false;
    auto slots=defender_slots::ReadSlots(member);if(!slots.valid || !slots.size)return false;
    Address exemplar=0;for(std::size_t i=0;i<slots.size;++i)
        if(ReadRecord(slots.slots[i].picker).nameComplete){exemplar=slots.slots[i].picker;break;}
    if(!exemplar)return false;
    const auto picker=lookupTrait(exemplar,pickerName);if(!ReadRecord(picker).Named(pickerName))return false;
    return PreserveResolvedSlot(member,awe,picker,option);
}

inline bool PreserveAweSlot(Address member,Address awe){return PreserveTraitSlot(member,awe,"Picker_Terror_Player",2);}
inline unsigned EpicCount(const List& list) {
    unsigned count=0;
    for(std::size_t i=0;i<list.size && list.valid;++i){
        bool duplicate=false;for(std::size_t j=0;j<i;++j)duplicate|=list.values[j]==list.values[i];
        if(!duplicate && Matches(ReadRecord(list.values[i]),epicTraits))++count;
    }
    return count;
}
inline void ApplyAwe(Address member,Address exemplar) {
    if(!config.forceTerrorPlayer || !addTrait || !lookupTrait || !hasCapacity || !Eligible(member))return;
    const auto awe=lookupTrait(exemplar,"Terror_Player");
    if(!awe){Log("awe lookup failed",member,"",true);return;}
    const auto list=Traits(member);if(!list.valid)return;
    const bool already=Has(list,awe);
    std::vector<Address> plan;
    for(std::size_t i=0;i<list.size;++i)if(ReadRecord(list.values[i]).Named("Quirk_Fearless"))plan.push_back(list.values[i]);
    bool fits=already || hasCapacity(member,awe,plan);
    if(!fits){
        static constexpr const char* preferred[]={"ET_GreatStrength","Quirk_BonusHealth","ET_GreatToughness",
            "Quirk_Deadly","Quirk_AttacksFaster","Quirk_LifeSteal","Quirk_FastRunner","Quirk_GetsUpQuickly"};
        const auto consider=[&](Address candidate){
            if(fits || candidate==awe)return;
            for(auto p:plan)if(p==candidate)return;
            plan.push_back(candidate);fits=hasCapacity(member,awe,plan);
        };
        for(const auto* name:preferred)for(std::size_t i=0;i<list.size;++i)
            if(ReadRecord(list.values[i]).Named(name))consider(list.values[i]);
        for(std::size_t i=0;i<list.size;++i)
            if(Matches(ReadRecord(list.values[i]),replaceableCombatTraits))consider(list.values[i]);
    }
    if(!fits){char reason[256]{};std::snprintf(reason,sizeof(reason),"awe blocked; no complete plan; %s",capacityReport);Log(reason,member,"Terror_Player",true);return;}
    // Plan entirely before any sacrifices; no partial pruning merely to probe a cap.
    std::vector<Address> removed;
    for(auto p:plan){
        if(!Eligible(member))return;
        const bool ok=RemoveStoredTrait(member,p);const auto now=Traits(member);
        if(!ok || !now.valid || Has(now,p)){Log("planned replacement removal unconfirmed; stopped",member,ReadRecord(p).name.data(),true);return;}
        removed.push_back(p);Log("replaced trait for awe",member,ReadRecord(p).name.data(),true);
    }
    if(already){if(!PreserveAweSlot(member,awe))Log("awe active; rebuild cache unconfirmed",member,"Terror_Player",true);return;}
    const auto before=Traits(member);if(!before.valid || !Eligible(member))return;
    const bool added=addTrait(member,awe);const auto after=Traits(member);
    if(added && after.valid && Has(after,awe)){
        Log(PreserveAweSlot(member,awe)?"awe rebuild cache verified":"awe active; rebuild cache unconfirmed",member,"Terror_Player",true);
        bool retained=true;for(std::size_t i=0;i<before.size;++i)retained&=Has(after,before.values[i]);
        Log(retained?"awe added-native; other traits retained":"awe added; unexpected trait loss",member,"Terror_Player",true);
        if(!retained){config.enabled=false;Log("repair disabled after unexpected trait loss",member,"",true);}
    }else{
        Log("awe addition unconfirmed",member,"Terror_Player",true);
        // Best-effort restoration, preferring Fearless (the first plan entry).
        for(auto p:removed){
            const auto now=Traits(member);
            if(!now.valid || Has(now,awe) || !Eligible(member))break;
            if(!hasCapacity(member,p,{})){Log("replacement restore blocked by native limits",member,ReadRecord(p).name.data(),true);continue;}
            const bool ok=addTrait(member,p);
            Log(ok && Has(Traits(member),p)?"replacement restored after failed addition":"replacement restore unconfirmed",member,ReadRecord(p).name.data(),true);
        }
    }
}

struct ClassContext {
    const char* specialization=nullptr;
    Record role,preset;
    bool roleKnown=false,presetKnown=false;
};
inline ClassContext ReadClassContext(Address member) {
    ClassContext ctx;
    const auto markers=Traits(member,0x1B0);
    if(!markers.valid)return ctx;
    static constexpr const char* specializations[]={"Marker_Specialization_Destroyer","Marker_Specialization_Tracker",
        "Marker_Specialization_Assassin","Marker_Specialization_BeastMaster","Marker_Specialization_Commander",
        "Marker_Specialization_CombatMaster","Marker_Specialization_Marksman","Marker_Specialization_Tank",
        "Marker_Specialization_Berserker","Marker_Specialization_Trickster"};
    unsigned count=0;
    for(const auto* name:specializations){bool present=false;
        for(std::size_t i=0;i<markers.size;++i)present|=ReadRecord(markers.values[i]).Named(name);
        if(present){ctx.specialization=name;++count;}
    }
    if(count!=1)ctx.specialization=nullptr;
    const auto role=At<Address>(member,0x318),preset=At<Address>(member,0x190);
    ctx.role=ReadRecord(role.value);ctx.preset=ReadRecord(preset.value);
    // A missing role cannot disprove a title exception. A readable null preset
    // means no preset; a non-null unreadable record is unknown, never invalid.
    ctx.roleKnown=role.readable && role.value && ctx.role.nameComplete;
    ctx.presetKnown=preset.readable && (!preset.value || ctx.preset.nameComplete);
    return ctx;
}
inline defender_ability_policy::NecromancyState ReadNecromancy(Address member,const List& traits){
    defender_ability_policy::NecromancyState state;state.traitsKnown=traits.valid;
    if(traits.valid)for(std::size_t i=0;i<traits.size;++i){const auto r=ReadRecord(traits.values[i]);
        state.raiseDead|=r.Named("Raise_Dead");state.selfResurrection|=r.Named("Necromancer_SelfRes");state.curseWeapon|=r.Named("Curse_Weapon");}
    const auto markers=Traits(member,0x1B0);state.markersKnown=markers.valid;
    if(markers.valid)for(std::size_t i=0;i<markers.size;++i){const auto r=ReadRecord(markers.values[i]);
        state.necromancerMarker|=r.Named("Marker_IsNecromancer");state.curseMarker|=r.Named("Marker_StatusEffect_Curse");}
    return state;
}
inline bool InvalidClassTrait(const Record& trait,const ClassContext& ctx) {
    if(defender_ability_policy::InvalidReinforcements(trait,ctx.specialization,ctx.preset,ctx.presetKnown))return true;
    if(!ctx.specialization || !ctx.presetKnown)return false;
    if(trait.Named("Quirk_NoLastChance"))return ctx.roleKnown &&
        std::strcmp(ctx.specialization,"Marker_Specialization_Assassin")!=0 &&
        !Matches(ctx.role,Quirk_NoLastChance_roles) && !Matches(ctx.preset,Quirk_NoLastChance_presets);
    if(trait.Named("Quirk_LifeSteal"))return ctx.roleKnown &&
        std::strcmp(ctx.specialization,"Marker_Specialization_Berserker")!=0 &&
        !Matches(ctx.role,Quirk_LifeSteal_roles) && !Matches(ctx.preset,Quirk_LifeSteal_presets);
    return false;
}
inline bool UnresolvedSlayerFrost(const Record& trait,const ClassContext& ctx) {
    return ctx.specialization && ctx.presetKnown && trait.Named("Immune_Frost") &&
        std::strcmp(ctx.specialization,"Marker_Specialization_CombatMaster")==0 &&
        !Matches(ctx.preset,Immune_Frost_presets);
}
inline Address ClassMismatch(Address member,const List& traits) {
    if(!traits.valid)return 0;
    const auto ctx=ReadClassContext(member);
    for(std::size_t i=0;i<traits.size;++i)if(InvalidClassTrait(ReadRecord(traits.values[i]),ctx))return traits.values[i];
    return 0;
}
inline bool OffClassTrait(const Record& trait,const ClassContext& ctx) {
    if(!ctx.specialization)return false;
    if(trait.Named("Quirk_DyingCommand"))return std::strcmp(ctx.specialization,"Marker_Specialization_Commander")!=0;
    if(trait.Named("Quirk_SecondWind"))return std::strcmp(ctx.specialization,"Marker_Specialization_Tank")!=0;
    return InvalidClassTrait(trait,ctx);
}
inline void CleanOffClassTraits(Address member) {
    if(!Eligible(member))return;
    const auto traits=PolicyTraits(member);const auto ctx=ReadClassContext(member);
    if(!traits.valid || !ctx.specialization){Log("off-class cleanup skipped; incomplete or ambiguous context",member,"",true);return;}
    for(std::size_t i=0;i<traits.size;++i){
        const auto record=ReadRecord(traits.values[i]);
        if(!OffClassTrait(record,ctx))continue;
        if(!Eligible(member))return;
        const bool removed=RemoveStoredTrait(member,traits.values[i]);const auto after=Traits(member);
        Log(removed && after.valid && !Has(after,traits.values[i])?"removed off-class trait on flagged defender":"off-class removal unconfirmed",
            member,record.name.data(),true);
        if(!removed || !after.valid || Has(after,traits.values[i]))return;
    }
}
inline void CleanInspiringPresence(Address member) {
    if(!config.forceTerrorPlayer || !Eligible(member))return;
    const auto list=PolicyTraits(member);if(!list.valid)return;
    for(std::size_t i=0;i<list.size;++i){
        if(!ReadRecord(list.values[i]).Named("Quirk_InspiringPresence"))continue;
        const bool removed=RemoveStoredTrait(member,list.values[i]);
        const auto after=Traits(member);
        Log(removed && after.valid && !Has(after,list.values[i])?
            "removed inspiring presence on flagged defender":"inspiring presence removal unconfirmed",
            member,"Quirk_InspiringPresence",true);
        return;
    }
}
// Retry conversion requests after a successful immunity removal but failed
// activation/cache insertion. All state is per-session and identity checked.
struct VulnerabilityIntent {Mark token;unsigned wanted=0;std::array<Address,8> pickers{};};
inline std::vector<VulnerabilityIntent> vulnerabilityIntents;
inline void RememberDiagnosis(Address member,unsigned mask);
inline void WeakenImmunities(Address member,Address exemplar,unsigned restoredMask=0,const defender_provenance::Key* provenance=nullptr){
    namespace vp=defender_vulnerability_policy;
    if(!config.enabled || !config.forceTerrorPlayer || !Eligible(member) || !lookupTrait || !addTrait || !hasCapacity ||
       !defender_slots::erase || !defender_slots::push || !defender_slots::Editable(member))return;
    const auto identity=At<std::array<Address,3>>(member,0x168);const auto traits=PolicyTraits(member);
    if(!identity.readable || !traits.valid)return;
    const Mark token{member,identity.value[2],identity.value[0],identity.value[1]};
    const auto same=[&](){const auto now=At<std::array<Address,3>>(member,0x168);
        return config.enabled && config.forceTerrorPlayer && Eligible(member) && now.readable && now.value==identity.value;};
    for(std::size_t t=0;t<traits.size;++t)if(vp::FindRemovalOnlyAlias(ReadRecord(traits.values[t]))){
        if(!RemoveStoredTrait(member,traits.values[t])){Log("nonstandard immunity removal unconfirmed",member,ReadRecord(traits.values[t]).name.data());return;}
        if(!same())return;
        Log("nonstandard immunity removed; no mortal counterpart",member,ReadRecord(traits.values[t]).name.data());
    }
    unsigned wanted=0;
    if(restoredMask&63){
        unsigned activeMask=0,cachedMask=0;const auto active=Traits(member);const auto slots=defender_slots::ReadSlots(member);
        if(!active.valid || !slots.valid)return;
        for(std::size_t n=0;n<active.size;++n){const auto r=ReadRecord(active.values[n]);
            for(unsigned i=0;i<6;++i)if(r.Named(vp::families[i].mortal))activeMask|=1u<<i;}
        for(std::size_t n=0;n<slots.size;++n)if(!slots.slots[n].flags){const auto r=ReadRecord(slots.slots[n].trait);
            for(unsigned i=0;i<6;++i)if(r.Named(vp::families[i].mortal))cachedMask|=1u<<i;}
        wanted=(restoredMask&63)&~(activeMask&cachedMask);
    }
    for(std::size_t t=0;t<traits.size;++t){const auto r=ReadRecord(traits.values[t]);
        for(unsigned i=0;i<std::size(vp::families);++i)if(r.Named(vp::families[i].immunity))wanted|=1u<<i;}
    if(wanted){if(provenance){if(provenance->valid)defender_provenance::Remember(*provenance,wanted);}else RememberDiagnosis(member,wanted);}
    VulnerabilityIntent* intent=nullptr;
    for(auto& entry:vulnerabilityIntents)if(entry.token.member==member && entry.token.key==token.key &&
        entry.token.seed==token.seed && entry.token.identity==token.identity){intent=&entry;break;}
    if(!intent){
        if(!wanted)return;
        if(vulnerabilityIntents.size()>=512){Log("immunity conversion deferred; retry registry full",member);return;}
        vulnerabilityIntents.push_back({token,wanted,{}});intent=&vulnerabilityIntents.back();
    }else intent->wanted|=wanted;
    for(unsigned i=0;i<std::size(vp::families);++i){
        if(!(intent->wanted&(1u<<i)))continue;const auto& family=vp::families[i];
        const auto now=At<std::array<Address,3>>(member,0x168);
        if(!Eligible(member) || !now.readable || now.value!=identity.value)return;
        Address destination=0,picker=intent->pickers[i];
        if(family.mortal){
            if(picker && !ReadRecord(picker).Named(family.picker))picker=0;
            destination=lookupTrait(exemplar,family.mortal);
            const auto slots=defender_slots::ReadSlots(member);if(!slots.valid)return;
            for(std::size_t n=0;n<slots.size && !picker;++n)if(ReadRecord(slots.slots[n].picker).nameComplete)
                picker=lookupTrait(slots.slots[n].picker,family.picker);
            if(!ReadRecord(destination).Named(family.mortal) || !ReadRecord(picker).Named(family.picker)){
                Log("immunity conversion deferred; replacement lookup unavailable",member,family.immunity);continue;}
            intent->pickers[i]=picker;
        }
        const auto current=PolicyTraits(member);if(!current.valid)return;
        bool removed=true;
        for(std::size_t n=0;n<current.size;++n)if(ReadRecord(current.values[n]).Named(family.immunity))
            if(!RemoveStoredTrait(member,current.values[n])){removed=false;break;}
        if(!removed){Log("immunity source removal unconfirmed; retry retained",member,family.immunity);continue;}
        if(!same())return;
        if(!family.mortal){intent->wanted&=~(1u<<i);Log("immunity removed; no shipped mortal counterpart",member,family.immunity);continue;}
        const auto active=Traits(member);if(!active.valid)return;
        if(!Has(active,destination)){
            if(!hasCapacity(member,destination,{}) || !addTrait(member,destination) || !Has(Traits(member),destination)){
                Log("mortal vulnerability activation unconfirmed; retry retained",member,family.mortal);continue;}
        }
        if(!same())return;
        if(!PreserveResolvedSlot(member,destination,picker,family.option)){
            Log("mortal vulnerability active; rebuild cache unconfirmed; retry retained",member,family.mortal);continue;}
        intent->wanted&=~(1u<<i);Log("immunity replaced with cached mortal vulnerability",member,family.mortal);
    }
    vulnerabilityIntents.erase(std::remove_if(vulnerabilityIntents.begin(),vulnerabilityIntents.end(),
        [](const VulnerabilityIntent& entry){return !entry.wanted;}),vulnerabilityIntents.end());
}
inline void RememberDiagnosis(Address member,unsigned mask=0){
    if(!defender_provenance::enabled || ScopeReason(member))return;
    const auto key=defender_provenance::ReadKey(image,member);if(key.valid)defender_provenance::Remember(key,mask);
}
inline bool PriorDiagnosis(Address member,unsigned* mask=nullptr){
    if(!defender_provenance::enabled)return false;
    return defender_provenance::Lookup(defender_provenance::ReadKey(image,member),mask);
}
inline void Repair(Address member) {
    if(!config.enabled || !removeTrait || !Eligible(member))return;
    const auto before=PolicyTraits(member);
    if(!before.valid){LogMember(member,"trait-list-unreadable-or-invalid");return;}
    const auto identity=At<Address>(member,0x170),key=At<Address>(member,0x178);
    const auto seed=At<Address>(member,0x168);
    if(!identity.readable || !key.readable || !seed.readable)return;
    bool flagged=EpicCount(before)>3;Address exemplar=0;
    if(flagged)Log("flagged; more than three epic traits",member,"",true);
    for(std::size_t i=0;i<before.size;++i)if(Blacklisted(ReadRecord(before.values[i]))){flagged=true;exemplar=before.values[i];break;}
    if(const auto mismatch=ClassMismatch(member,before)){
        flagged=true;exemplar=mismatch;Log("flagged; class/title trait mismatch",member,ReadRecord(mismatch).name.data(),true);
    }
    const auto classContext=ReadClassContext(member);
    if(config.diagnostics){bool necromancy=false;
        for(std::size_t i=0;i<before.size;++i)necromancy|=defender_ability_policy::NecromancyTrait(ReadRecord(before.values[i]));
        if(necromancy){const auto review=defender_ability_policy::ReviewNecromancy(ReadNecromancy(member,before));
            if(const auto* why=defender_ability_policy::ReviewReason(review))Log(why,member,"necromancy observation only");}}

    for(std::size_t i=0;i<before.size;++i)if(UnresolvedSlayerFrost(ReadRecord(before.values[i]),classContext)){
        Log("review only; Slayer frost immunity; shipped CMT acquisition path unresolved",member,"Immune_Frost",true);break;
    }
    bool remembered=false;
    for(const auto& m:marked)if(m.member==member && m.key==key.value && m.seed==seed.value && m.identity==identity.value)remembered=true;
    const auto provenanceKey=defender_provenance::enabled?defender_provenance::ReadKey(image,member):defender_provenance::Key{};
    unsigned restoredMask=0;const bool prior=defender_provenance::Lookup(provenanceKey,&restoredMask);
    if(prior && !remembered){remembered=true;if(marked.size()<512)marked.push_back({member,key.value,seed.value,identity.value});
        Log("restored diagnosed member from invasion-scoped provenance",member);}
    if(flagged && !remembered && marked.size()<512)marked.push_back({member,key.value,seed.value,identity.value});
    if((flagged || remembered) && provenanceKey.valid)defender_provenance::Remember(provenanceKey);
    if(!exemplar && before.size)exemplar=before.values[0];
    for(std::size_t i=0;i<before.size;++i){
        const auto trait=before.values[i];const auto record=ReadRecord(trait);
        if(!Blacklisted(record))continue;
        const auto nowIdentity=At<Address>(member,0x170),nowKey=At<Address>(member,0x178);
        if(!nowIdentity.readable || !nowKey.readable || nowIdentity.value!=identity.value ||
           nowKey.value!=key.value || !Eligible(member))return;
        const auto current=Traits(member);if(!current.valid)return;
        // Native 353454 erases the trait and handles inventory/item cleanup.
        // No direct vector edits, remote calls, or worker-thread game calls.
        const bool result=RemoveStoredTrait(member,trait);
        const auto after=Traits(member);bool remains=false;
        for(std::size_t n=0;n<after.size && after.valid;++n)remains|=after.values[n]==trait;
        Log(result && after.valid && !remains?"removed-native; unspawned; picker sources erased":"removal-unconfirmed",member,record.name.data());
        if(!result || !after.valid || remains)return;
    }
    if(flagged || remembered){
        if(config.forceTerrorPlayer){const auto stored=PolicyTraits(member);
            for(std::size_t i=0;stored.valid && i<stored.size;++i)if(ReadRecord(stored.values[i]).Named("Quirk_Fearless") && !Has(Traits(member),stored.values[i]))
                if(!RemoveStoredTrait(member,stored.values[i])){Log("Fearless source removal unconfirmed",member);return;}}
        CleanOffClassTraits(member);CleanInspiringPresence(member);ApplyAwe(member,exemplar);WeakenImmunities(member,exemplar,restoredMask,&provenanceKey);
    }
}
inline void RepairFaction(Address seed) {
    if(!config.enabled || !removeTrait || ScopeReason(seed))return;
    const auto key=At<Address>(seed,0x178);
    const auto globals=At<Address>(image,offset::globalsRva),manager=At<Address>(globals.value,0x6C68);
    const auto slots=At<Address>(manager.value,0x98);const auto count=At<unsigned>(manager.value,0xA0);
    if(!slots.readable || !slots.value || !count.readable || count.value>4096)return;
    Address pool=0;unsigned matches=0;
    for(unsigned i=0;i<count.value;++i){
        const auto entry=slots.value+static_cast<Address>(i)*0x290;
        const auto found=Read<Address>(entry);
        if(!found.readable)return;
        if(found.value==key.value){++matches;const auto p=At<Address>(entry+8,0x20);if(!p.readable)return;pool=p.value;}
    }
    if(matches!=1 || !pool){Log("faction roster unavailable",seed);return;}
    const auto begin=At<Address>(pool,0x18),end=At<Address>(pool,0x20);
    if(!begin.readable || !end.readable || !begin.value || end.value<begin.value ||
       (end.value-begin.value)%8 || (end.value-begin.value)/8>512){Log("faction roster rejected bounds",seed);return;}
    std::vector<Address> members;
    const auto size=(end.value-begin.value)/8;members.reserve(size);
    for(Address i=0;i<size;++i){
        const auto m=Read<Address>(begin.value+i*8);if(!m.readable)return;
        if(m.value){const auto faction=At<Address>(m.value,0x178);
            if(!faction.readable || faction.value!=key.value)return;
            bool duplicate=false;for(auto prior:members)duplicate|=prior==m.value;
            if(!duplicate)members.push_back(m.value);
        }
    }
    unsigned eligible=0,skipped=0;
    for(auto m:members){
        // Stop if the roster root/bounds changed. Repair revalidates member
        // faction owner, class, actor state and identity before each native call.
        const auto g=At<Address>(image,offset::globalsRva),mgr=At<Address>(g.value,0x6C68);
        const auto s=At<Address>(mgr.value,0x98);const auto c=At<unsigned>(mgr.value,0xA0);
        const auto b=At<Address>(pool,0x18),e=At<Address>(pool,0x20),k=At<Address>(m,0x178);
        if(!mgr.readable || mgr.value!=manager.value || !s.readable || s.value!=slots.value ||
           !c.readable || c.value!=count.value || !b.readable || b.value!=begin.value ||
           !e.readable || e.value!=end.value || !k.readable || k.value!=key.value){Log("faction roster changed; stopped",seed);return;}
        auto reason=SkipReason(m);
        LogMember(m,reason?reason:"eligible");
        if(beforeRosterRepair){beforeRosterRepair(m);reason=SkipReason(m);}
        if(!reason){++eligible;Repair(m);}else ++skipped;
    }
    char summary[160]{};
    std::snprintf(summary,sizeof(summary),"faction pass loaded=%zu eligible=%u skipped=%u",members.size(),eligible,skipped);
    Log(summary,seed,"",true);
}
inline bool ArmedFor(Address member) {
    const auto key=At<Address>(member,0x178);const auto name=ReadRecord(key.value);
    return config.enabled && !armedFaction.empty() &&
        name.nameComplete && armedFaction==name.name.data();
}
inline void ObserveThreat(Address member,Address actor) {
    // Called under stateMutex after binding; publication is read-only and
    // without taking the repair mutex at the damage call site.
    if(!ArmedFor(member) || !actor)return;
    const auto link=At<Address>(member,0x48),key=At<Address>(member,0x178);
    const auto seed=At<Address>(member,0x168),identity=At<Address>(member,0x170);
    const auto globals=At<Address>(image,offset::globalsRva),manager=At<Address>(globals.value,0x6C68);
    const auto owner=At<Address>(manager.value,0xA8),tag=At<Address>(key.value,0x18);
    if(!link.readable || link.value!=actor || !key.readable || !seed.readable || !identity.readable ||
       !owner.readable || !owner.value || !tag.readable || owner.value!=tag.value)return;
    bool flagged=false;
    for(const auto& m:marked)if(m.member==member && m.key==key.value && m.seed==seed.value && m.identity==identity.value)flagged=true;
    if(!flagged)flagged=PriorDiagnosis(member);
    if(!flagged){const auto list=Traits(member);if(!list.valid)return;
        flagged=EpicCount(list)>3;
        flagged|=ClassMismatch(member,list)!=0;
        for(std::size_t i=0;i<list.size;++i)flagged|=Blacklisted(ReadRecord(list.values[i]));
    }
    if(!flagged)return;
    auto next=std::make_shared<std::vector<Threat>>(*LoadThreats());
    for(auto& t:*next)if(t.member==member || t.actor==actor){
        t={actor,member,key.value,seed.value,identity.value};
        StoreThreats(ThreatSnapshot(next));return;
    }
    if(next->size()>=512){Log("damage registry full; actor not registered",member,"",true);return;}
    next->push_back({actor,member,key.value,seed.value,identity.value});
    StoreThreats(ThreatSnapshot(next));
    Log("flagged actor registered for damage blocking",member,"",true);
}
inline bool DamageBlocked(const Entity& source) {
    if(!config.enabled || !source.actor.readable || !source.actor.value)return false;
    const auto snapshot=LoadThreats();
    for(const auto& t:*snapshot)if(t.actor==source.actor.value){
        const auto link=At<Address>(t.member,0x48),key=At<Address>(t.member,0x178);
        const auto seed=At<Address>(t.member,0x168),identity=At<Address>(t.member,0x170);
        return link.readable && link.value==t.actor && key.readable && key.value==t.key &&
            seed.readable && seed.value==t.seed && identity.readable && identity.value==t.identity;
    }
    return false;
}
inline void ArmForSpawn(Address member) {
    if(!config.enabled || ScopeReason(member))return;
    const auto name=ReadRecord(At<Address>(member,0x178).value);
    if(name.nameComplete && armedFaction!=name.name.data()){
        armedFaction=name.name.data();Log("binding reapply armed until disable/restart",member,name.name.data(),true);
    }
}
inline bool AutomaticSeedReady(Address member) {
    if(!config.enabled || ScopeReason(member))return false;
    const auto backing=At<Address>(member,0x158),appearance=At<Address>(member,0x2D8);
    return backing.readable && backing.value && appearance.readable && appearance.value &&
        Traits(member).valid && Traits(member,0x1B0).valid;
}
// Called under stateMutex/handling from the existing native UI formatter.
inline void AutomaticRepair(Address member,ULONGLONG now) {
    if(!config.enabled || ScopeReason(member))return;
    const auto key=At<Address>(member,0x178);
    if(!key.readable || !key.value)return;
    // A different imported roster is not delayed by the previous fort's timer.
    if(lastBatch && lastBatchKey==key.value && now>=lastBatch && now-lastBatch<1500)return;
    if(!AutomaticSeedReady(member))return;
    if(beforeImportedRepair)beforeImportedRepair(member);
    ArmForSpawn(member);
    lastBatch=now;lastBatchKey=key.value;
    Log("automatic imported roster pass",member);
    const auto repairStart=GetTickCount64();
    RepairFaction(member);
    const auto elapsed=GetTickCount64()-repairStart;
    if(elapsed>=10){char timing[100]{};std::snprintf(timing,sizeof(timing),"roster pass elapsedMs=%llu",static_cast<unsigned long long>(elapsed));Log(timing,member);}
}
inline Address __fastcall Spawn(Address actor,Address member,bool initialize) {
    bool observed=false,track=false,traceFlagged=false;
    if(!handling){
        std::unique_lock<std::mutex> lock(stateMutex,std::try_to_lock);
        if(lock.owns_lock() && config.enabled && !armedFaction.empty()){
            const bool inScope=ArmedFor(member);track=inScope;
            observed=config.diagnostics;
            if(observed){
                const auto faction=ReadRecord(At<Address>(member,0x178).value);
                char route[200]{};std::snprintf(route,sizeof(route),"bind route initialize=%u armedMatch=%u faction=%s",unsigned(initialize),unsigned(inScope),faction.nameComplete?faction.name.data():"<unknown>");
                Log(route,member,"",true);
                LogMember(member,inScope?"bind-before; armed faction":"bind-before; outside armed faction",true);
            }
            if(inScope){
                struct Scope{Scope(){handling=true;}~Scope(){handling=false;}} scope;
                const auto reason=SkipReason(member);
                if(!reason && initialize)Repair(member);
                const auto key=At<Address>(member,0x178),seed=At<Address>(member,0x168),identity=At<Address>(member,0x170);
                for(const auto& m:marked)if(m.member==member && key.readable && m.key==key.value && seed.readable && m.seed==seed.value && identity.readable && m.identity==identity.value)traceFlagged=true;
                else if(observed)Log(reason?reason:"bind repair skipped; initialize=false",member,"",true);
            }
        }
    }
    // 40ED18(actor, member, initialize) is shared by physical spawn,
    // actor registration and member/actor reassignment. Never mutate afterwards.
    const auto previousActor=traceBindingActor,previousMember=traceBindingMember;
    if(traceFlagged){traceBindingActor=actor;traceBindingMember=member;}
    const auto result=PLH::FnCast(spawnTrampoline,static_cast<SpawnFn>(nullptr))(actor,member,initialize);
    traceBindingActor=previousActor;traceBindingMember=previousMember;
    if(observed || track){
        std::unique_lock<std::mutex> lock(stateMutex,std::try_to_lock);
        if(lock.owns_lock()){
            if(observed)LogMember(member,"bind-after; observation-only",true);
            if(track)ObserveThreat(member,actor);
            if(track && onFlaggedBound)onFlaggedBound(actor,member);
        }
    }
    return result;
}

inline Address __fastcall Format(Address member,Address properties,bool flag) {
    if(!handling && onFormattedMember)onFormattedMember(member);
    // The formatter also runs during loading. Only complete, owned, unspawned
    // imported invasion members may trigger a pass; no keyboard polling needed.
    std::unique_lock<std::mutex> lock(stateMutex,std::defer_lock);
    if(!handling && config.enabled && lock.try_lock()){
        struct Scope{Scope(){handling=true;}~Scope(){handling=false;}} scope;
        AutomaticRepair(member,GetTickCount64());
    }
    if(lock.owns_lock())lock.unlock();
    return PLH::FnCast(trampoline,static_cast<FormatFn>(nullptr))(member,properties,flag);
}
template<std::size_t N> inline bool Signature(Address address,const unsigned char (&bytes)[N]) {
    for(std::size_t i=0;i<N;++i){const auto b=Read<unsigned char>(address+i);if(!b.readable || b.value!=bytes[i])return false;}
    return true;
}
inline bool StartProvenance(){
    static constexpr unsigned char context[]={0x48,0x83,0xEC,0x28,0x48,0x8B,0x05,0xF1,0x06,0xE7,0,0x48,0x8B,0x88,0x30,0x6D,0,0};
    static constexpr unsigned char launch[]={0x40,0x55,0x53,0x56,0x57,0x41,0x56,0x48,0x8B,0xEC,0x48,0x83,0xEC,0x70,0x48,0x8B,0xD9};
    defender_provenance::contextVerified=Signature(image+0x188F3AC,context) && Signature(image+0x1B39EE8,launch);
    if(!defender_provenance::contextVerified)return false;
    wchar_t base[MAX_PATH]{};auto count=GetEnvironmentVariableW(L"LOCALAPPDATA",base,MAX_PATH);
    if(!count || count>=MAX_PATH)return false;
    std::wstring dir(base);dir+=L"\\SoWPlugin";
    if(!CreateDirectoryW(dir.c_str(),nullptr) && GetLastError()!=ERROR_ALREADY_EXISTS)return false;
    dir+=L"\\RepairState";if(!CreateDirectoryW(dir.c_str(),nullptr) && GetLastError()!=ERROR_ALREADY_EXISTS)return false;
    return defender_provenance::Init(dir+L"\\provenance-v1.tsv");
}
inline bool Install(Config settings={}) {
    if(hook || spawnHook){Log("install rejected; hook already exists",0);return false;}
    {std::lock_guard<std::mutex> lock(diagnosticMutex);diagnosticStamps.clear();}
    config=settings;lastBatch=0;lastBatchKey=0;marked.clear();vulnerabilityIntents.clear();armedFaction.clear();
    StoreThreats(std::make_shared<const std::vector<Threat>>());
    if(!config.enabled){Log("disabled by configuration; no hook installed",0);return true;}
    image=reinterpret_cast<Address>(GetModuleHandleW(nullptr));
    static constexpr unsigned char formatter[]={0x44,0x88,0x44,0x24,0x18,0x48,0x89,0x54,0x24,0x10,0x48,0x89,0x4c,0x24,0x08};
    static constexpr unsigned char remover[]={0x48,0x89,0x5c,0x24,0x10,0x55,0x56,0x57,0x48,0x8b,0xec,0x48,0x83,0xec,0x50};
    if(!Signature(image+0x10BD90,formatter)){Log("install failed; formatter signature mismatch",0);return false;}
    if(!Signature(image+0x353454,remover)){Log("install failed; remover signature mismatch",0);return false;}
    static constexpr unsigned char slotErase[]={0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x48,0x89,0x7C,0x24,0x18};
    static constexpr unsigned char slotPush[]={0x48,0x8B,0xC4,0x48,0x89,0x58,0x08,0x48,0x89,0x68,0x10,0x48,0x89,0x70,0x18};
    static constexpr unsigned char slotAdd[]={0x48,0x89,0x5C,0x24,0x08,0x57,0x48,0x83,0xEC,0x40};
    if(!Signature(image+0x7A7378,slotErase) || !Signature(image+0x7CD0CC,slotPush) || !Signature(image+0x7CD03C,slotAdd)){
        Log("install failed; picker helper signature mismatch",0);return false;
    }
    static constexpr unsigned char update[]={0x40,0x53,0x48,0x83,0xEC,0x20,0x48,0x8B,0xD9};
    if(!Signature(image+0x3528A0,update)){Log("install failed; trait rebuild signature mismatch",0);return false;}
    defender_slots::update=reinterpret_cast<defender_slots::UpdateFn>(image+0x3528A0);
    defender_slots::erase=reinterpret_cast<defender_slots::EraseFn>(image+0x7A7378);
    defender_slots::push=reinterpret_cast<defender_slots::PushFn>(image+0x7CD0CC);
    defender_slots::add=reinterpret_cast<defender_slots::AddFn>(image+0x7CD03C);
    { // Lookup/add helpers also serve base-class slot reconciliation.
        static constexpr unsigned char add[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x18,0x48,0x89,0x54,0x24,0x10};
        static constexpr unsigned char count[]={0x48,0x83,0xec,0x28,0x4c,0x8b,0xc1,0xe8,0x74,0x11,0,0};
        static constexpr unsigned char attr[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57};
        static constexpr unsigned char hash[]={0x0f,0xb6,0x01,0x84,0xc0,0x74,0x27};
        static constexpr unsigned char find[]={0x45,0x33,0xc9,0x4c,0x8b,0xd9,0x41,0x83,0xe8,0x01};
        if(!Signature(image+0x3566B0,add) || !Signature(image+0x1BBC01C,count) ||
           !Signature(image+0x1169594,attr) || !Signature(image+0x423E70,hash) || !Signature(image+0x278540,find)){
            Log("install failed; awe helper signature mismatch",0);return false;
        }
        addTrait=reinterpret_cast<RemoveFn>(image+0x3566B0);lookupTrait=&LookupTrait;hasCapacity=&Capacity;
    }
    static constexpr unsigned char spawn[]={0x4c,0x8b,0xdc,0x49,0x89,0x5b,0x08,0x49,0x89,0x6b,0x10,0x49,0x89,0x73,0x18,0x57};
    if(!Signature(image+0x40ED18,spawn)){
        Log("install failed; actor binding signature mismatch",0);return false;
    }
    removeTrait=reinterpret_cast<RemoveFn>(image+0x353454);
    try{if(!StartProvenance())Log("reload provenance unavailable or not writable; direct repair remains active",0);
        else Log("invasion-scoped reload provenance enabled; background local journal",0);
        if(defender_provenance::loadFailed)Log("prior provenance journal rejected; new diagnoses only",0);}
    catch(...){defender_provenance::Close();Log("reload provenance initialization failed; direct repair remains active",0);}
    hook=std::make_unique<PLH::x64Detour>(image+0x10BD90,reinterpret_cast<std::uint64_t>(&Format),&trampoline);
    if(!hook->hook()){defender_provenance::Close();hook.reset();removeTrait=nullptr;Log("install failed; PolyHook detour rejected",0);return false;}
    spawnHook=std::make_unique<PLH::x64Detour>(image+0x40ED18,reinterpret_cast<std::uint64_t>(&Spawn),&spawnTrampoline);
    if(!spawnHook->hook()){
        spawnHook.reset();defender_provenance::Close();
        if(hook->unHook()){hook.reset();removeTrait=nullptr;}
        Log("install failed; actor binding detour rejected; check hook-removal result before unload",0);return false;
    }
    char installed[160]{};
    std::snprintf(installed,sizeof(installed),"v12 installed; strict tribes; reinforcement checks; optional Awe and mortal vulnerabilities; forceTerror=%u diagnostics=%u",unsigned(config.forceTerrorPlayer),unsigned(config.diagnostics));
    Log(installed,0);return true;
}
inline bool Remove() {
    const bool formatRemoved=!hook || hook->unHook();
    const bool spawnRemoved=!spawnHook || spawnHook->unHook();
    if(formatRemoved)hook.reset();if(spawnRemoved)spawnHook.reset();
    if(!formatRemoved || !spawnRemoved)return false;
    defender_provenance::Close();
    armedFaction.clear();
    lastBatch=0;lastBatchKey=0;
    StoreThreats(std::make_shared<const std::vector<Threat>>());
    defender_slots::erase=nullptr;defender_slots::push=nullptr;defender_slots::add=nullptr;defender_slots::update=nullptr;
    hook.reset();config.enabled=false;removeTrait=nullptr;addTrait=nullptr;lookupTrait=nullptr;hasCapacity=nullptr;marked.clear();vulnerabilityIntents.clear();return true;
}
}
