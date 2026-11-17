#pragma once
#include "sow_defender_repair.hpp"
#include "sow_ring_might.hpp"
#include <algorithm>

namespace sow::defender_normalize {
namespace dr=defender_repair;
using LookupFn=Address(*)(Address,const char*);
using RecordFn=Address(__fastcall*)(Address);
using TribeFn=void(__fastcall*)(Address,Address,const char*);
using RoleFn=void(__fastcall*)(Address,Address);
using WriteFn=bool(*)(Address,Address,Address);
using RefreshFn=void(__fastcall*)(Address);
struct API {LookupFn lookup=nullptr;RecordFn modelFromAI=nullptr,templateFromAI=nullptr;TribeFn tribe=nullptr;RoleFn role=nullptr;WriteFn write=nullptr;RefreshFn refresh=nullptr;};
inline API native;
inline bool enabled=false;
inline thread_local bool changing=false;
inline const char* DefaultAI(const Record& model){
    const auto pair=dr::StandardClass(model);return pair?pair->ai:nullptr;
}
inline bool WriteRecord(Address address,Address expected,Address replacement){
    // Native import/copy assigns these immutable database links directly.
    // Never used for traits, containers, live actors or appearance storage.
    const auto before=Read<Address>(address);MEMORY_BASIC_INFORMATION info{};
    if(!before.readable || before.value!=expected || !VirtualQuery(reinterpret_cast<void*>(address),&info,sizeof(info)) ||
       info.State!=MEM_COMMIT || (info.Protect&(PAGE_GUARD|PAGE_NOACCESS)) ||
       !(info.Protect&(PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)))return false;
    SIZE_T written=0;
    if(!WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),&replacement,sizeof(replacement),&written) || written!=sizeof(replacement))return false;
    const auto after=Read<Address>(address);return after.readable && after.value==replacement;
}
struct Token {Address key=0,seed=0,id=0;bool valid=false;};
struct PendingAppearance {Address member;Token token;};
inline std::vector<PendingAppearance> pendingAppearance;
inline bool AppearancePending(Address member,const Token& token){
    for(const auto& p:pendingAppearance)if(p.member==member && p.token.key==token.key && p.token.seed==token.seed && p.token.id==token.id)return true;
    return false;
}
inline void ClearPending(Address member){
    pendingAppearance.erase(std::remove_if(pendingAppearance.begin(),pendingAppearance.end(),[&](const auto& p){return p.member==member;}),pendingAppearance.end());
}
inline Token Identity(Address member){
    const auto k=At<Address>(member,0x178),s=At<Address>(member,0x168),id=At<Address>(member,0x170);
    return {k.value,s.value,id.value,k.readable && s.readable && id.readable};
}
inline bool Scoped(Address member,const Token& token){
    const auto now=Identity(member);
    return enabled && dr::config.enabled && token.valid && now.valid && now.key==token.key && now.seed==token.seed && now.id==token.id &&
        dr::ArmedFor(member) && !dr::ScopeReason(member);
}
inline void Remember(Address member,const Token& token){
    dr::RememberDiagnosis(member);
    for(const auto& m:dr::marked)if(m.member==member && m.key==token.key && m.seed==token.seed && m.identity==token.id)return;
    if(dr::marked.size()<512)dr::marked.push_back({member,token.key,token.seed,token.id});
}
inline void Report(Address member,const char* field,const Record& from,const char* to,const char* result){
    char msg[550]{};std::snprintf(msg,sizeof(msg),"normalize field=%s from=%s to=%s result=%s",field,
        from.nameComplete?from.name.data():"<null-or-unknown>",to,result);
    dr::Log(msg,member,"",true);
}
inline bool CompatibleAppearance(Address member,bool olog=false){
    const auto storage=At<Address>(member,0x2D8);
    const auto base=ReadRecord(At<Address>(storage.value,0x20).value);
    if(!storage.readable || !storage.value || !base.Named(olog?"olog_anim_base":"orc_anim_base"))return false;
    for(const Address offset:{Address(0x28),Address(0x30)}){
        const auto ptr=At<Address>(storage.value,offset);if(!ptr.readable)return false;
        if(!ptr.value)continue;
        const auto record=ReadRecord(ptr.value);
        if(!record.nameComplete || dr::Matches(record,dr::structuralVisuals))return false;
    }
    return true;
}
inline bool PlainOrcAppearance(Address member){return CompatibleAppearance(member);}
inline bool RefreshReady(Address member){
    const auto storage=At<Address>(member,0x2D8);
    if(!storage.readable || !storage.value || !Read<std::array<unsigned char,0x1B4>>(storage.value).readable)return false;
    // Validate the known appearance-tag and material-override vector shapes.
    for(const auto layout:{std::pair<Address,Address>{0x8,16},{0x158,8}}){
        const auto begin=At<Address>(storage.value,layout.first),end=At<Address>(storage.value,layout.first+8);
        if(!begin.readable || !end.readable || end.value<begin.value || (end.value-begin.value)%layout.second ||
           (end.value-begin.value)/layout.second>512 || (end.value!=begin.value && !begin.value))return false;
    }
    return true;
}
inline const char* FallbackModel(Address member,const Record& cls,const Record& ai){
    if(dr::StandardClass(cls))return cls.name.data();
    if(cls.Named("SQ_Zog"))return "Orc_Spearman";
    if(cls.Named("ZTestVeryLargeOrcInfantry"))return "Orc_Infantry_Tough";
    const auto storage=At<Address>(member,0x2D8);const auto base=ReadRecord(At<Address>(storage.value,0x20).value);
    if(base.Named("olog_anim_base") || dr::Matches(cls,dr::shippedOlogClasses))return "OlogHai_Infantry";
    if(const auto pair=dr::StandardAI(ai))return pair->model;
    return "Orc_Infantry_Tough";
}
inline bool ReadyMember(Address member,const Token& token){
    return Scoped(member,token) && dr::Traits(member).valid && dr::Traits(member,0x1B0).valid &&
        At<Address>(member,0x2D8).value && At<Address>(member,0x158).value;
}
struct TribeChoice {const char* name="TribeDef_Dark";const char* reason="no-attack-clue";};
inline TribeChoice ChooseTribe(Address member){
    // Preserve the Mystic family when normalizing this known DLC-only alias.
    // Its UI name/icon are identical; fort lists compare distinct records.
    if(ReadRecord(At<Address>(member,0x328).value).Named("DLC_TribeDef_Sorcerous_Poison"))
        return {"TribeDef_Sorcerous","DLC-Mystic-alias-to-stock"};
    // Exact shipped tribe -> picker -> entry -> trait links, audited offline.
    static constexpr const char* pairs[][2]={
        {"TribeAttack_Dark","TribeDef_Dark"},{"TribeAttack_Feral","TribeDef_Feral"},
        {"TribeAttack_Machine","TribeDef_Machine"},{"TribeAttack_Marauder","TribeDef_Marauder"},
        {"TribeAttack_Mystic","TribeDef_Sorcerous"},{"TribeAttack_Terror","TribeDef_Terror"},
        {"TribeAttack_Warmonger","TribeDef_Warmonger"},{"TribeAttack_Slaughter","TribeDef_Slaughter"},
        {"TribeAttack_Outlaw","TribeDef_Outlaw"}};
    const auto traits=dr::Traits(member);
    if(!traits.valid)return {"TribeDef_Dark","incomplete-traits"};
    const char* selected=nullptr;
    for(std::size_t i=0;i<traits.size;++i){
        const auto record=ReadRecord(traits.values[i]);
        if(!record.nameComplete)return {"TribeDef_Dark","incomplete-traits"};
        for(const auto& p:pairs)if(record.Named(p[0])){
            if(selected && std::strcmp(selected,p[1])!=0)return {"TribeDef_Dark","conflicting-attacks"};
            selected=p[1];
        }
    }
    return selected?TribeChoice{selected,"single-attack-clue"}:TribeChoice{};
}
struct BaseRepair {Address member;Token token;};
inline std::vector<BaseRepair> pendingBase;
inline bool BasePending(Address member,const Token& token){for(const auto& p:pendingBase)
    if(p.member==member && p.token.key==token.key && p.token.seed==token.seed && p.token.id==token.id)return true;return false;}
inline void QueueBase(Address member,const Token& token){
    if(BasePending(member,token))return;
    if(pendingBase.size()>=512)pendingBase.erase(pendingBase.begin());pendingBase.push_back({member,token});
}
inline void ReconcileBase(Address member,const Token& token){
    if(!BasePending(member,token) || !Scoped(member,token))return;
    if(dr::ReconcileBaseSlot(member)){
        pendingBase.erase(std::remove_if(pendingBase.begin(),pendingBase.end(),[&](const BaseRepair& p){return p.member==member;}),pendingBase.end());
        dr::Log("base-class picker reconciled; advanced class preserved",member);
    }else dr::Log("base-class picker deferred; native admission or rebuild unconfirmed",member);
}
inline void Normalize(Address member){
    if(!enabled || changing || !native.lookup || !native.modelFromAI || !native.templateFromAI || !native.write)return;
    const auto token=Identity(member);if(!Scoped(member,token))return;
    if(!ReadyMember(member,token)){dr::Log("normalize deferred; member containers unavailable",member,"",true);return;}
    struct Scope{Scope(){changing=true;}~Scope(){changing=false;}} scope;
    const auto clsPtr=At<Address>(member,0x278),aiPtr=At<Address>(member,0x188),rolePtr=At<Address>(member,0x318);
    const auto cls=ReadRecord(clsPtr.value),ai=ReadRecord(aiPtr.value),role=ReadRecord(rolePtr.value);
    const auto standard=dr::StandardClass(cls);
    const bool badClass=cls.nameComplete && !standard;
    const bool badAI=standard && ai.nameComplete && !ai.Named(standard->ai);
    if(badClass || badAI || AppearancePending(member,token)){
        const auto modelName=FallbackModel(member,cls,ai);
        const auto model=standard?clsPtr.value:native.lookup(0x27018B8,modelName);
        const auto modelRecord=ReadRecord(model);const auto* aiName=DefaultAI(modelRecord);
        const auto pair=dr::StandardClass(modelRecord);
        const bool refresh=pair && !CompatibleAppearance(member,pair->olog);
        const auto tree=aiName?native.lookup(0x2700960,aiName):0;
        const auto tmpl=tree?native.templateFromAI(tree):0;
        // Validate the complete fallback before changing either record link.
        if(model && pair && aiPtr.readable && clsPtr.readable && tree && tmpl && (!refresh || (native.refresh && RefreshReady(member))) && native.modelFromAI(tree)==model &&
           ReadRecord(tmpl).Named(modelRecord.name.data()) && Scoped(member,token)){
            const bool aiChanged=aiPtr.value!=tree,modelChanged=clsPtr.value!=model;
            if(aiChanged || modelChanged)Remember(member,token);
            bool ok=!aiChanged || native.write(member+0x188,aiPtr.value,tree);
            if(ok && modelChanged){
                ok=Scoped(member,token) && native.write(member+0x278,clsPtr.value,model);
                // Best effort restore the first pointer if the second write failed.
                if(!ok && aiChanged && Scoped(member,token))native.write(member+0x188,tree,aiPtr.value);
            }
            if(ok && modelChanged)QueueBase(member,token);
            if(ok && refresh && Scoped(member,token)){
                // Native member regeneration, including SMC construction and
                // inventory reapplication. Never copy/clear appearance vectors.
                if(!AppearancePending(member,token)){
                    ClearPending(member);
                    if(pendingAppearance.size()>=512)pendingAppearance.erase(pendingAppearance.begin());
                    pendingAppearance.push_back({member,token});
                }
                native.refresh(member);
                const bool compatible=CompatibleAppearance(member,pair->olog);
                if(compatible)ClearPending(member);
                Report(member,"appearance",cls,modelRecord.name.data(),compatible?"verified-native-rebuild":"native-rebuild-unconfirmed; inspect export");
            }
            if(ok && !refresh)ClearPending(member);
            Report(member,"AI/class",cls,modelRecord.name.data(),ok?"verified":"write-failed; inspect state");
        }else Report(member,"AI/class",cls,modelName,"fallback-or-appearance-preflight-unresolved; unchanged");
    }
    ReconcileBase(member,token);
    // Use the native role setter for its previous-role/counter/bookkeeping work.
    const auto activePair=dr::StandardClass(ReadRecord(At<Address>(member,0x278).value));
    if(dr::Matches(role,dr::structuralRoles) && native.role && activePair && CompatibleAppearance(member,activePair->olog) && Scoped(member,token)){
        const auto replacement=native.lookup(0x2701D70,"Role_Outcast");
        if(replacement){Remember(member,token);native.role(member,replacement);
            Report(member,"role",role,"Role_Outcast",At<Address>(member,0x318).value==replacement?"verified-native":"native-unconfirmed");}
        else Report(member,"role",role,"Role_Outcast","lookup-failed; unchanged");
    }
    const auto tribePtr=At<Address>(member,0x328);const auto tribe=ReadRecord(tribePtr.value);
    const bool invalidTribe=tribePtr.readable && (!tribePtr.value || dr::InvalidTribe(tribe));
    if(invalidTribe && native.tribe && Scoped(member,token)){
        auto choice=ChooseTribe(member);
        auto replacement=native.lookup(0x2701D80,choice.name);
        if(!ReadRecord(replacement).Named(choice.name)){
            choice={"TribeDef_Dark","inferred-lookup-failed"};
            replacement=native.lookup(0x2701D80,choice.name);
        }
        char msg[200]{};std::snprintf(msg,sizeof(msg),"tribe replacement choice=%s reason=%s",choice.name,choice.reason);
        dr::Log(msg,member);
        // Revalidate identity/scope after lookup; never pass an unverified record.
        if(ReadRecord(replacement).Named(choice.name) && Scoped(member,token)){
            Remember(member,token);native.tribe(member,replacement,"DefenderRepair");
            Report(member,"tribe",tribe,choice.name,At<Address>(member,0x328).value==replacement?"verified-native":"native-unconfirmed");}
        else Report(member,"tribe",tribe,choice.name,"lookup-or-scope-failed; unchanged");
    }
}
inline Address CreationTemplate(Address member,Address original){
    // Called under the existing repair mutex, before native actor construction.
    Normalize(member);
    const auto token=Identity(member);if(!Scoped(member,token))return original;
    bool repaired=false;for(const auto& m:dr::marked)if(m.member==member && m.key==token.key && m.seed==token.seed && m.identity==token.id)repaired=true;
    if(!repaired)return original;
    const auto cls=ReadRecord(At<Address>(member,0x278).value);const auto* expected=DefaultAI(cls);
    const auto tree=At<Address>(member,0x188);
    const auto pair=dr::StandardClass(cls);
    if(!expected || !pair || !tree.readable || !ReadRecord(tree.value).Named(expected) || !CompatibleAppearance(member,pair->olog))return original;
    const auto tmpl=native.templateFromAI(tree.value);
    return tmpl && ReadRecord(tmpl).Named(cls.name.data())?tmpl:original;
}
inline bool Install(){
    if(!dr::config.enabled)return true;
    if(enabled || dr::beforeRosterRepair)return false;
    const auto base=dr::image;
    static constexpr unsigned char tribe[]={0x48,0x89,0x5C,0x24,0x10,0x48,0x89,0x74,0x24,0x18};
    static constexpr unsigned char role[]={0x48,0x89,0x5C,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x48,0x89,0x7C,0x24,0x20};
    static constexpr unsigned char model[]={0x48,0x83,0xEC,0x28,0xE8};
    static constexpr unsigned char tmpl[]={0x48,0x83,0xEC,0x28,0x4C,0x8B,0xD1};
    static constexpr unsigned char hash[]={0x0F,0xB6,0x01,0x84,0xC0,0x74,0x27};
    static constexpr unsigned char find[]={0x45,0x33,0xC9,0x4C,0x8B,0xD9,0x41,0x83,0xE8,0x01};
    static constexpr unsigned char refresh[]={0x48,0x89,0x5C,0x24,0x08,0x57,0x48,0x81,0xEC,0x80,0x02,0x00,0x00};
    if(!dr::Signature(base+0x1BC6718,tribe) || !dr::Signature(base+0x1BC63F8,role) ||
       !dr::Signature(base+0x57710C,model) || !dr::Signature(base+0x577124,tmpl) ||
       !dr::Signature(base+0x423E70,hash) || !dr::Signature(base+0x278540,find) || !dr::Signature(base+0x1BC3AA8,refresh))return false;
    native={&ring_might::Lookup,reinterpret_cast<RecordFn>(base+0x57710C),reinterpret_cast<RecordFn>(base+0x577124),
        reinterpret_cast<TribeFn>(base+0x1BC6718),reinterpret_cast<RoleFn>(base+0x1BC63F8),&WriteRecord,reinterpret_cast<RefreshFn>(base+0x1BC3AA8)};
    pendingAppearance.clear();pendingBase.clear();enabled=true;dr::beforeRosterRepair=&Normalize;
    dr::Log("structural normalization enabled; strict stock class/AI pairs; native appearance regeneration; Undead preserved",0);return true;
}
inline bool Remove(){if(dr::beforeRosterRepair==&Normalize)dr::beforeRosterRepair=nullptr;enabled=false;native={};pendingAppearance.clear();pendingBase.clear();return true;}
}
