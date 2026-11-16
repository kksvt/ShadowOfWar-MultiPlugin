#pragma once
#include "sow_entities.hpp"
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
// Public feature settings. F9 activation and repair policy are internal defaults.
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
inline std::uint64_t trampoline=0;
inline thread_local bool handling=false;
inline ULONGLONG lastBatch=0;
inline void Log(const char* event,Address member,const char* trait="",bool /*summary*/=false) {
    if(!config.diagnostics)return;
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
    return Matches(r,structuralTribes);
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
    // Exact humanoid compatibility for stored-trait cleanup, not a declaration
    // that every class here is a legitimate generated captain. The oversized
    // debug Orc remains a structural finding and is never model-converted here.
    static constexpr const char* classes[]={"Orc_Defender_Tough","Orc_Infantry","Orc_Infantry_Tough","Orc_Spearman","Orc_Archer","Orc_Archer_Tough",
        "Orc_Berserker","Orc_Berserker_Tough","Orc_Defender","OlogHai_Infantry"};
    const auto cls=At<Address>(member,0x278);const auto record=ReadRecord(cls.value);
    if(!cls.readable || !record.nameComplete)return "class-unreadable-or-unnamed";
    return Matches(record,classes)?nullptr:"unsupported-class";
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
        if(i==0)f.incompatible=Matches(f.record,structuralClasses);
        else if(i==1)f.incompatible=Matches(f.record,structuralAI);
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
inline void LogMember(Address member,const char* reason,bool /*transition*/=false) {
    if(!config.diagnostics)return;
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
    // Include unflagged/skipped observed members too. Preview and bind-before /
    // bind-after snapshots retain phase and identity in the member-detail line.
    // Native vectors are read through the same bounded, stability-checked reader;
    // an unreadable list is never reported as an empty list.
    LogMemberList(member,"traits",list,snapshot);
    LogMemberList(member,"markers",Traits(member,0x1B0),snapshot);
    // Unsupported classes still reach structural inspection. No findings are
    // promoted to trait-repair eligibility or used to mutate live actors.
    if(!ScopeReason(member))for(const auto& field:InspectStructure(member)){
        char line[512]{};
        std::snprintf(line,sizeof(line),"[DefenderStructure] snapshot=%llu member=%p field=%s record=%p name=%s status=%s action=observation-only\n",
            static_cast<unsigned long long>(snapshot),reinterpret_cast<void*>(member),field.field,
            reinterpret_cast<void*>(field.record.address),field.readable?field.record.name.data():"<unknown>",
            !field.readable?(field.nullRecord?"null-record":"unreadable-or-unnamed"):field.incompatible?"known-incompatible":"no-listed-signal");
        WriteDiagnostic(line);
    }
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
        const bool ok=removeTrait(member,p);const auto now=Traits(member);
        if(!ok || !now.valid || Has(now,p)){Log("planned replacement removal unconfirmed; stopped",member,ReadRecord(p).name.data(),true);return;}
        removed.push_back(p);Log("replaced trait for awe",member,ReadRecord(p).name.data(),true);
    }
    if(already){Log("awe already present",member,"Terror_Player",true);return;}
    const auto before=Traits(member);if(!before.valid || !Eligible(member))return;
    const bool added=addTrait(member,awe);const auto after=Traits(member);
    if(added && after.valid && Has(after,awe)){
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
inline bool InvalidClassTrait(const Record& trait,const ClassContext& ctx) {
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
inline void CleanOffClassTraits(Address member) {
    if(!Eligible(member))return;
    const auto traits=Traits(member);const auto ctx=ReadClassContext(member);
    if(!traits.valid || !ctx.specialization){Log("off-class cleanup skipped; incomplete or ambiguous context",member,"",true);return;}
    for(std::size_t i=0;i<traits.size;++i){
        const auto record=ReadRecord(traits.values[i]);
        const bool dying=record.Named("Quirk_DyingCommand") && std::strcmp(ctx.specialization,"Marker_Specialization_Commander")!=0;
        if(!dying && !InvalidClassTrait(record,ctx))continue;
        if(!Eligible(member))return;
        const bool removed=removeTrait(member,traits.values[i]);const auto after=Traits(member);
        Log(removed && after.valid && !Has(after,traits.values[i])?"removed off-class trait on flagged defender":"off-class removal unconfirmed",
            member,record.name.data(),true);
        if(!removed || !after.valid || Has(after,traits.values[i]))return;
    }
}
inline void CleanInspiringPresence(Address member) {
    if(!config.forceTerrorPlayer || !Eligible(member))return;
    const auto list=Traits(member);if(!list.valid)return;
    for(std::size_t i=0;i<list.size;++i){
        if(!ReadRecord(list.values[i]).Named("Quirk_InspiringPresence"))continue;
        const bool removed=removeTrait(member,list.values[i]);
        const auto after=Traits(member);
        Log(removed && after.valid && !Has(after,list.values[i])?
            "removed inspiring presence on flagged defender":"inspiring presence removal unconfirmed",
            member,"Quirk_InspiringPresence",true);
        return;
    }
}
inline void Repair(Address member) {
    if(!config.enabled || !removeTrait || !Eligible(member))return;
    const auto before=Traits(member);
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
    for(std::size_t i=0;i<before.size;++i)if(UnresolvedSlayerFrost(ReadRecord(before.values[i]),classContext)){
        Log("review only; Slayer frost immunity; shipped CMT acquisition path unresolved",member,"Immune_Frost",true);break;
    }
    bool remembered=false;
    for(const auto& m:marked)if(m.member==member && m.key==key.value && m.seed==seed.value && m.identity==identity.value)remembered=true;
    if(flagged && !remembered && marked.size()<512)marked.push_back({member,key.value,seed.value,identity.value});
    if(!exemplar && before.size)exemplar=before.values[0];
    for(std::size_t i=0;i<before.size;++i){
        const auto trait=before.values[i];const auto record=ReadRecord(trait);
        if(!Blacklisted(record))continue;
        const auto nowIdentity=At<Address>(member,0x170),nowKey=At<Address>(member,0x178);
        if(!nowIdentity.readable || !nowKey.readable || nowIdentity.value!=identity.value ||
           nowKey.value!=key.value || !Eligible(member))return;
        const auto current=Traits(member);if(!current.valid)return;
        bool present=false;for(std::size_t n=0;n<current.size;++n)present|=current.values[n]==trait;
        if(!present)continue;
        // Native 353454 erases the trait and handles inventory/item cleanup.
        // No direct vector edits, remote calls, or worker-thread game calls.
        const bool result=removeTrait(member,trait);
        const auto after=Traits(member);bool remains=false;
        for(std::size_t n=0;n<after.size && after.valid;++n)remains|=after.values[n]==trait;
        Log(result && after.valid && !remains?"removed-native; unspawned":"removal-unconfirmed",member,record.name.data());
        if(!result || !after.valid || remains)return;
    }
    if(flagged || remembered){CleanOffClassTraits(member);CleanInspiringPresence(member);ApplyAwe(member,exemplar);}
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
    // The formatter also runs while loading/caching UI data. Require explicit
    // held input while this process owns the foreground window.
    DWORD foregroundPid=0;
    GetWindowThreadProcessId(GetForegroundWindow(),&foregroundPid);
    const bool requested=config.enabled &&
        foregroundPid==GetCurrentProcessId() && (GetAsyncKeyState(VK_F9)&0x8000);
    std::unique_lock<std::mutex> lock(stateMutex,std::defer_lock);
    if(!handling && requested && lock.try_lock()){
        struct Scope{Scope(){handling=true;}~Scope(){handling=false;}} scope;
        ArmForSpawn(member);
        const auto now=GetTickCount64();
        if((!lastBatch || now-lastBatch>=1500) && !ScopeReason(member)){lastBatch=now;RepairFaction(member);}
    }
    if(lock.owns_lock())lock.unlock();
    return PLH::FnCast(trampoline,static_cast<FormatFn>(nullptr))(member,properties,flag);
}
template<std::size_t N> inline bool Signature(Address address,const unsigned char (&bytes)[N]) {
    for(std::size_t i=0;i<N;++i){const auto b=Read<unsigned char>(address+i);if(!b.readable || b.value!=bytes[i])return false;}
    return true;
}
inline bool Install(Config settings={}) {
    if(hook || spawnHook){Log("install rejected; hook already exists",0);return false;}
    config=settings;lastBatch=0;marked.clear();armedFaction.clear();
    StoreThreats(std::make_shared<const std::vector<Threat>>());
    if(!config.enabled){Log("disabled by configuration; no hook installed",0);return true;}
    image=reinterpret_cast<Address>(GetModuleHandleW(nullptr));
    static constexpr unsigned char formatter[]={0x44,0x88,0x44,0x24,0x18,0x48,0x89,0x54,0x24,0x10,0x48,0x89,0x4c,0x24,0x08};
    static constexpr unsigned char remover[]={0x48,0x89,0x5c,0x24,0x10,0x55,0x56,0x57,0x48,0x8b,0xec,0x48,0x83,0xec,0x50};
    if(!Signature(image+0x10BD90,formatter)){Log("install failed; formatter signature mismatch",0);return false;}
    if(!Signature(image+0x353454,remover)){Log("install failed; remover signature mismatch",0);return false;}
    if(config.forceTerrorPlayer){
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
    hook=std::make_unique<PLH::x64Detour>(image+0x10BD90,reinterpret_cast<std::uint64_t>(&Format),&trampoline);
    if(!hook->hook()){hook.reset();removeTrait=nullptr;Log("install failed; PolyHook detour rejected",0);return false;}
    spawnHook=std::make_unique<PLH::x64Detour>(image+0x40ED18,reinterpret_cast<std::uint64_t>(&Spawn),&spawnTrampoline);
    if(!spawnHook->hook()){
        spawnHook.reset();
        if(hook->unHook()){hook.reset();removeTrait=nullptr;}
        Log("install failed; actor binding detour rejected; check hook-removal result before unload",0);return false;
    }
    char installed[160]{};
    std::snprintf(installed,sizeof(installed),"installed; F9 activation; forceTerror=%u diagnostics=%u",unsigned(config.forceTerrorPlayer),unsigned(config.diagnostics));
    Log(installed,0);return true;
}
inline bool Remove() {
    const bool formatRemoved=!hook || hook->unHook();
    const bool spawnRemoved=!spawnHook || spawnHook->unHook();
    if(formatRemoved)hook.reset();if(spawnRemoved)spawnHook.reset();
    if(!formatRemoved || !spawnRemoved)return false;
    armedFaction.clear();
    StoreThreats(std::make_shared<const std::vector<Threat>>());
    hook.reset();config.enabled=false;removeTrait=nullptr;addTrait=nullptr;lookupTrait=nullptr;hasCapacity=nullptr;marked.clear();return true;
}
}
