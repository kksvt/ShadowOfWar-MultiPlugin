#pragma once
#include "sow_defender_normalize.hpp"

namespace sow::defender_spawn_audit {
// Full native actor-creation entry, not the tiny member/template getter.
// RCX manager, RDX label, R8 placement, R9 template; stack: two booleans,
// opaque argument, member, opaque argument. Guarded normalization may replace
// the template argument; all other parameters and the return are forwarded.
using CreateFn=Address(__fastcall*)(Address,Address,Address,Address,bool,bool,Address,Address,Address);
inline std::unique_ptr<PLH::x64Detour> hook;
inline std::uint64_t trampoline=0;
inline thread_local bool observing=false;
inline const char* ExpectedAI(const Record& model) {
    static constexpr const char* pairs[][2]={
        {"Orc_Infantry","AI_Infantry"},{"Orc_Spearman","AI_Spearman"},
        {"Orc_Infantry_Tough","AI_Infantry_Tough"},{"Orc_Archer_Tough","AI_Archer_Tough"},
        {"Orc_Archer","AI_Archer"},{"Orc_Defender_Tough","AI_Defender_Tough"},
        {"Orc_Defender","AI_Defender"},{"Orc_Berserker","AI_Berserker"},
        {"Orc_Berserker_Tough","AI_Berserker_Tough"},{"OlogHai_Infantry","AI_Olog_Infantry"},
        {"ZTestVeryLargeOrcInfantry","AI_Infantry"},{"SQ_Zog","AI_Zog"}};
    for(const auto& pair:pairs)if(model.Named(pair[0]))return pair[1];
    return "<unmapped>";
}
inline const char* Name(const Record& record){return record.nameComplete?record.name.data():"<unknown>";}
inline Address __fastcall Create(Address manager,Address label,Address placement,Address definition,
    bool first,bool second,Address opaque,Address member,Address last) {
    bool capture=false;Address key=0,seed=0,identity=0;
    if(!observing){
        std::unique_lock<std::mutex> lock(defender_repair::stateMutex,std::try_to_lock);
        if(lock.owns_lock() && defender_repair::config.enabled &&
           defender_repair::ArmedFor(member) && !defender_repair::ScopeReason(member)){
            const auto incoming=definition;
            if(defender_normalize::enabled)definition=defender_normalize::CreationTemplate(member,definition);
            const auto k=At<Address>(member,0x178),s=At<Address>(member,0x168),id=At<Address>(member,0x170);
            if(defender_repair::config.diagnostics && k.readable && s.readable && id.readable){
                capture=true;key=k.value;seed=s.value;identity=id.value;
                const auto ai=ReadRecord(At<Address>(member,0x188).value);
                const auto model=ReadRecord(At<Address>(member,0x278).value);
                const auto input=ReadRecord(definition),original=ReadRecord(incoming);
                char out[1000]{};
                std::snprintf(out,sizeof(out),"[DefenderSpawnAudit] before member=%p nameHash=0x%08X memberAI=%s memberClass=%s inputTemplate=%s effectiveTemplate=%s expectedDefaultAI=%s normalizationEnabled=%u\n",
                    reinterpret_cast<void*>(member),unsigned(identity),Name(ai),Name(model),Name(original),Name(input),ExpectedAI(model),unsigned(defender_normalize::enabled));
                defender_repair::WriteDiagnostic(out);
            }
        }
    }
    const bool previous=observing;observing=true;
    const auto result=PLH::FnCast(trampoline,static_cast<CreateFn>(nullptr))(
        manager,label,placement,definition,first,second,opaque,member,last);
    observing=previous;
    if(capture){
        const auto k=At<Address>(member,0x178),s=At<Address>(member,0x168),id=At<Address>(member,0x170);
        const bool stable=k.readable && s.readable && id.readable && k.value==key && s.value==seed && id.value==identity;
        // Native getter 3F12F0 reads actor -> stats(+26A8) -> tree(+58).
        const auto stats=At<Address>(result,0x26A8);
        const auto runtimeAI=ReadRecord(At<Address>(stats.value,0x58).value);
        const auto runtimeClass=ReadRecord(At<Address>(result,0x2B30).value);
        char out[700]{};
        std::snprintf(out,sizeof(out),"[DefenderSpawnAudit] after member=%p nameHash=0x%08X identityStable=%u actor=%p runtimeAI=%s runtimeClass=%s action=observation-only\n",
            reinterpret_cast<void*>(member),unsigned(identity),unsigned(stable),reinterpret_cast<void*>(result),Name(runtimeAI),Name(runtimeClass));
        defender_repair::WriteDiagnostic(out);
    }
    return result;
}
inline bool Install(){
    if(!defender_repair::config.enabled || (!defender_repair::config.diagnostics && !defender_normalize::enabled))return true;
    if(hook)return false;
    const auto base=defender_repair::image;
    static constexpr unsigned char entry[]={0x48,0x8B,0xC4,0x48,0x89,0x58,0x08,0x48,0x89,0x70,0x10,0x48,0x89,0x78,0x18,0x55};
    if(!defender_repair::Signature(base+0x1036D0,entry))return false;
    hook=std::make_unique<PLH::x64Detour>(base+0x1036D0,reinterpret_cast<std::uint64_t>(&Create),&trampoline);
    if(!hook->hook()){hook.reset();trampoline=0;return false;}
    defender_repair::Log("pre-construction hook installed; guarded normalization; optional diagnostics",0);return true;
}
inline bool Remove(){
    if(hook && !hook->unHook())return false;
    hook.reset();trampoline=0;return true;
}
}
