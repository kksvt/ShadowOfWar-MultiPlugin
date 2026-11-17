#pragma once
#include "sow_defender_repair.hpp"
#include "sow_defender_inventory_audit.hpp"
#include <algorithm>

namespace sow::uruk_export {
inline bool enabled=false,keyDown=false;
inline bool inventoryLayoutVerified=false;
inline int shortcutKey=VK_F8;
inline std::mutex mutex;
inline unsigned sequence=0;
inline std::atomic<Address> observedMember{0};
inline constexpr std::size_t maxMembers=512,maxBytes=16*1024*1024;
inline std::string Quote(const char* value){
    std::string out="\"";
    for(const auto* p=reinterpret_cast<const unsigned char*>(value);*p;++p){
        switch(*p){case '"':out+="\\\"";break;case '\\':out+="\\\\";break;
        case '\n':out+="\\n";break;case '\r':out+="\\r";break;case '\t':out+="\\t";break;
        default:if(*p<32 || *p>=127){char b[7]{};std::snprintf(b,sizeof(b),"\\u%04x",unsigned(*p));out+=b;}else out+=char(*p);}
    }
    return out+'"';
}
inline std::string Pointer(Address value){char b[24]{};std::snprintf(b,sizeof(b),"0x%llX",static_cast<unsigned long long>(value));return Quote(b);}
template<class T> inline std::string Number(const Field<T>& value){
    if(!value.readable)return "null";
    return std::to_string(value.value);
}
inline std::string RecordJSON(const Field<Address>& pointer){
    const auto r=ReadRecord(pointer.value);
    return "{\"address\":"+(pointer.readable?Pointer(pointer.value):"null")+
        ",\"name\":"+(r.nameComplete?Quote(r.name.data()):"null")+
        ",\"status\":"+Quote(!pointer.readable?"unreadable-pointer":!pointer.value?"null":r.nameComplete?"named":"unreadable-or-incomplete-name")+"}";
}
inline std::string ListJSON(Address member,Address offset){
    const auto begin=At<Address>(member,offset),end=At<Address>(member,offset+8);
    if(!begin.readable || !end.readable || end.value<begin.value || (end.value-begin.value)%8 ||
       (end.value-begin.value)/8>512 || (end.value!=begin.value && !begin.value))
        return "{\"status\":\"invalid-or-unreadable-bounds\",\"records\":[],\"count\":null}";
    const auto count=(end.value-begin.value)/8;bool complete=true;std::string records="[";
    for(Address i=0;i<count;++i){
        const auto p=Read<Address>(begin.value+i*8);const auto r=ReadRecord(p.value);
        complete&=p.readable && r.nameComplete;
        if(i)records+=',';records+=RecordJSON(p);
    }
    records+=']';const auto b=At<Address>(member,offset),e=At<Address>(member,offset+8);
    complete&=b.readable && e.readable && b.value==begin.value && e.value==end.value;
    return "{\"status\":"+Quote(complete?"complete":"changed-or-incomplete")+",\"count\":"+std::to_string(count)+",\"records\":"+records+"}";
}
inline std::string SlotsJSON(Address member){
    const auto snapshot=defender_slots::ReadSlots(member);
    if(!snapshot.valid)return "{\"status\":\"invalid-or-unreadable-bounds\",\"count\":null,\"slots\":[]}";
    std::string out="{\"status\":\"complete\",\"count\":"+std::to_string(snapshot.size)+",\"slots\":[";
    for(std::size_t i=0;i<snapshot.size;++i){const auto& slot=snapshot.slots[i];if(i)out+=',';
        out+="{\"index\":"+std::to_string(i)+",\"picker\":"+RecordJSON({slot.picker,true})+
            ",\"resolved_trait\":"+RecordJSON({slot.trait,true})+",\"mission_unlocked_trait\":"+RecordJSON({slot.unlocked,true})+
            ",\"flags\":"+std::to_string(slot.flags)+",\"option_index\":"+std::to_string(slot.option)+"}";
    }
    const auto after=defender_slots::ReadSlots(member);bool stable=after.valid && after.size==snapshot.size;
    for(std::size_t i=0;stable && i<snapshot.size;++i){const auto& a=snapshot.slots[i];const auto& b=after.slots[i];
        stable=a.picker==b.picker && a.trait==b.trait && a.unlocked==b.unlocked && a.flags==b.flags && a.option==b.option;}
    out+="]}";if(!stable)out.replace(out.find("complete"),8,"changed-during-capture");return out;
}
inline std::string InventoryJSON(const defender_inventory_audit::Snapshot& inventory){
    std::string out="{\"layout_verified\":"+std::string(inventoryLayoutVerified?"true":"false")+
        ",\"equipment_complete\":"+(inventory.equipmentComplete?"true":"false")+",\"equipment\":[";
    for(unsigned i=0;i<25;++i){if(i)out+=',';out+="{\"slot\":"+std::to_string(i)+",\"instance\":"+Pointer(inventory.instances[i])+
        ",\"item\":"+RecordJSON({inventory.equipment[i],inventory.equipmentComplete || inventory.equipment[i]!=0})+"}";}
    out+="],\"look_list_complete\":"+std::string(inventory.looksComplete?"true":"false")+",\"look_items\":[";
    for(std::size_t i=0;i<inventory.lookCount;++i){if(i)out+=',';out+=RecordJSON({inventory.looks[i],true});}
    out+="],\"spawned\":"+std::string(inventory.spawned?"true":"false")+",\"runtime_link_verified\":"+(inventory.runtimeLinked?"true":"false")+
        ",\"runtime_counts_complete\":"+(inventory.runtimeComplete?"true":"false")+",\"runtime_tracked_counts\":{";
    for(unsigned i=0;i<std::size(defender_inventory_audit::trackedNames);++i){if(i)out+=',';
        out+=Quote(defender_inventory_audit::trackedNames[i])+':'+(inventory.runtimeComplete?std::to_string(inventory.runtimeCounts[i]):"null");}
    return out+"},\"cursed_skin_positive_evidence\":"+(inventory.CursedSkinEvidence()?"true":"false")+"}";
}
inline std::string MemberJSON(Address member){
    const auto token=At<std::array<Address,3>>(member,0x168);
    const auto level=At<unsigned>(member,0x4B8),hash=At<unsigned>(member,0x170);
    const auto actor=At<Address>(member,0x48),storage=At<Address>(member,0x2D8);
    const char* scope=defender_repair::ScopeReason(member),*skip=defender_repair::SkipReason(member);
    std::string out="{\"type\":\"member\",\"read_only\":true,\"member\":"+Pointer(member)+
        ",\"name_hash\":"+Number(hash)+",\"level_raw\":"+Number(level)+
        ",\"level_display\":"+(level.readable?std::to_string(static_cast<std::uint64_t>(level.value)+1):"null")+
        ",\"actor\":"+(actor.readable?Pointer(actor.value):"null")+
        ",\"repair_scope_reason\":"+(scope?Quote(scope):"null")+
        ",\"trait_repair_skip_reason\":"+(skip?Quote(skip):"null")+",\"fields\":{";
    struct Named {const char* name;Address offset;};
    static constexpr Named fields[]={{"faction",0x178},{"initial_faction",0x180},{"ai_combat_tree",0x188},
        {"preset",0x190},{"row",0x268},{"personality",0x270},{"class_character_model",0x278},
        {"role",0x318},{"previous_role",0x320},{"tribe",0x328},{"location",0x330},{"initial_builder",0x3A0}};
    bool first=true;for(const auto& f:fields){if(!first)out+=',';first=false;out+=Quote(f.name)+':'+RecordJSON(At<Address>(member,f.offset));}
    out+="},\"appearance\":{";
    static constexpr Named visuals[]={{"base",0x20},{"body",0x28},{"head",0x30},{"hair",0x38},
        {"accessory1",0x40},{"accessory2",0x48},{"body_flair",0x50},{"body_scar",0x58},
        {"helmet",0x60},{"shoulders",0x68},{"chest",0x70},{"arms",0x78},{"legs",0x80},{"low_lod",0x88}};
    first=true;for(const auto& f:visuals){if(!first)out+=',';first=false;out+=Quote(f.name)+':'+RecordJSON(At<Address>(storage.value,f.offset));}
    const auto traits=defender_repair::Traits(member);const auto ctx=defender_repair::ReadClassContext(member);
    out+="},\"traits\":"+ListJSON(member,0x198)+",\"picker_slots\":"+SlotsJSON(member)+",\"markers\":"+ListJSON(member,0x1B0)+
        ",\"epic_count\":"+(traits.valid?std::to_string(defender_repair::EpicCount(traits)):"null")+
        ",\"advanced_class\":"+(ctx.specialization?Quote(ctx.specialization):"null")+",\"policy_findings\":[";
    first=true;if(traits.valid)for(std::size_t i=0;i<traits.size;++i){const auto r=ReadRecord(traits.values[i]);
        const char* why=defender_repair::Blacklisted(r)?"mercenary-trait":defender_repair::InvalidClassTrait(r,ctx)?"class-title-mismatch":defender_repair::OffClassTrait(r,ctx)?"cleanup-only-off-class":nullptr;
        if(why){if(!first)out+=',';first=false;out+="{\"trait\":"+Quote(r.name.data())+",\"reason\":"+Quote(why)+"}";}}
    const auto inventory=inventoryLayoutVerified?defender_inventory_audit::ReadInventory(member):defender_inventory_audit::Snapshot{};
    const auto necro=defender_repair::ReadNecromancy(member,traits);
    const auto provenanceKey=defender_provenance::ReadKey(defender_repair::image,member);unsigned provenanceMask=0;
    const bool provenance=defender_provenance::Lookup(provenanceKey,&provenanceMask);
    const auto review=defender_ability_policy::ReviewReason(defender_ability_policy::ReviewNecromancy(necro));
    if(review){if(!first)out+=',';out+="{\"trait\":\"necromancy\",\"reason\":"+Quote(review)+",\"observation_only\":true}";}
    bool marked=false;for(const auto& m:defender_repair::marked)if(m.member==member && token.readable && m.seed==token.value[0] && m.identity==token.value[1] && m.key==token.value[2])marked=true;
    const auto stats=At<Address>(actor.value,0x26A8);
    out+="],\"necromancy_audit\":{\"traits_known\":"+std::string(necro.traitsKnown?"true":"false")+
        ",\"markers_known\":"+(necro.markersKnown?"true":"false")+",\"raise_dead\":"+(necro.raiseDead?"true":"false")+
        ",\"self_resurrection\":"+(necro.selfResurrection?"true":"false")+",\"eligibility_marker\":"+(necro.necromancerMarker?"true":"false")+
        ",\"curse_marker\":"+(necro.curseMarker?"true":"false")+",\"curse_weapon_trait\":"+(necro.curseWeapon?"true":"false")+
        ",\"cursed_skin_verified\":"+(inventory.CursedSkinEvidence()?"true":"null")+",\"automatic_cheat_verdict\":false},\"inventory_audit\":"+InventoryJSON(inventory)+
        ",\"provenance_match\":"+(provenance?"true":"false")+",\"provenance_key_valid\":"+(provenanceKey.valid?"true":"false")+
        ",\"provenance_weakening_mask\":"+std::to_string(provenanceMask)+",\"seed\":"+Pointer(token.value[0])+",\"repair_marked\":"+std::string(marked?"true":"false")+
        ",\"runtime\":{\"ai\":"+RecordJSON(At<Address>(stats.value,0x58))+
        ",\"class\":"+RecordJSON(At<Address>(actor.value,0x2B30))+"}";
    const auto after=At<std::array<Address,3>>(member,0x168);
    const bool stable=token.readable && after.readable && token.value==after.value;
    return out+",\"identity_stable\":"+(stable?"true":"false")+"}\n";
}
struct Capture {std::string text;std::size_t members=0;bool complete=true;};
inline Capture Collect(Address factionFilter=0,const char* reason="manual-F8"){
    Capture result;
    auto& out=result.text;
    out="{\"type\":\"header\",\"schema\":1,\"plugin\":\"v12\",\"read_only\":true,\"capture_reason\":"+Quote(reason)+",\"faction_filter\":"+Pointer(factionFilter)+",\"module_base\":"+Pointer(defender_repair::image)+
        ",\"repair_enabled\":"+(defender_repair::config.enabled?"true":"false")+
        ",\"force_terror_player\":"+(defender_repair::config.forceTerrorPlayer?"true":"false")+
        ",\"routine_diagnostics\":"+(defender_repair::config.diagnostics?"true":"false")+
        ",\"armed_faction\":"+Quote(defender_repair::armedFaction.c_str())+
        ",\"last_formatted_member\":"+Pointer(observedMember.load())+
        ",\"note\":\"Current known member fields from loaded owned roster buckets; a nonzero faction_filter restricts automatic capture to that invasion faction. Not all engine memory. Already repaired values may be present. No remote account metadata collected.\"}\n";
    const auto context=defender_provenance::ReadContext(defender_repair::image);
    out+="{\"type\":\"repair_provenance_context\",\"status\":"+Quote(context.status)+",\"cloud_id\":"+Quote(context.cloud.c_str())+
        ",\"mode\":"+Quote(context.mode.c_str())+",\"region\":"+Quote(context.region.c_str())+",\"valid\":"+(context.valid?"true":"false")+"}\n";
    {std::lock_guard<std::mutex> guard(defender_provenance::mutex);
        out+="{\"type\":\"repair_provenance_store\",\"enabled\":"+std::string(defender_provenance::enabled?"true":"false")+
            ",\"writer_available\":"+(defender_provenance::work?"true":"false")+",\"entries\":"+std::to_string(defender_provenance::entries.size())+",\"load_failed\":"+(defender_provenance::loadFailed?"true":"false")+
            ",\"write_failed\":"+(defender_provenance::writeFailed?"true":"false")+",\"write_pending\":"+(defender_provenance::generation!=defender_provenance::savedGeneration?"true":"false")+"}\n";}
    const auto globals=At<Address>(defender_repair::image,offset::globalsRva),manager=At<Address>(globals.value,0x6C68);
    const auto slots=At<Address>(manager.value,0x98),owner=At<Address>(manager.value,0xA8);
    const auto count=At<unsigned>(manager.value,0xA0);
    if(!manager.readable || !manager.value || !slots.readable || !owner.readable || !owner.value || !count.readable || count.value>4096 || (count.value && !slots.value)){
        result.complete=false;out+="{\"type\":\"error\",\"reason\":\"roster-manager-unavailable-or-invalid\"}\n";
    }else{
        std::vector<Address> seen;
        // Put the last formatted member's faction, then imported factions first
        // so a large home army cannot consume the entire export budget first.
        const auto focus=At<Address>(observedMember.load(),0x178);
        std::vector<std::pair<unsigned,unsigned>> order;
        for(unsigned i=0;i<count.value;++i){
            const auto f=Read<Address>(slots.value+Address(i)*0x290);const auto name=ReadRecord(f.value);
            const unsigned priority=focus.readable && focus.value && f.readable && f.value==focus.value?0u:
                name.nameComplete && std::strncmp(name.name.data(),"InvasionFaction_",16)==0?1u:2u;
            order.emplace_back(priority,i);
        }
        std::stable_sort(order.begin(),order.end());
        for(const auto& bucket:order){
            const auto i=bucket.second;
            const auto entry=slots.value+Address(i)*0x290;const auto faction=Read<Address>(entry);
            if(factionFilter && (!faction.readable || faction.value!=factionFilter))continue;
            if(faction.readable && !faction.value)continue;
            const auto tag=At<Address>(faction.value,0x18),pool=At<Address>(entry+8,0x20);
            const auto begin=At<Address>(pool.value,0x18),end=At<Address>(pool.value,0x20);
            if(!faction.readable || !tag.readable || tag.value!=owner.value || !pool.readable || !pool.value || !begin.readable || !end.readable || end.value<begin.value ||
               (end.value-begin.value)%8 || (end.value-begin.value)/8>8192 || (end.value!=begin.value && !begin.value)){
                result.complete=false;out+="{\"type\":\"roster\",\"faction\":"+RecordJSON(faction)+",\"status\":\"invalid-or-unavailable-pool\"}\n";continue;
            }
            out+="{\"type\":\"roster\",\"faction\":"+RecordJSON(faction)+",\"slots\":"+std::to_string((end.value-begin.value)/8)+"}\n";
            // Keep a validated last-formatted member ahead of the 512-member
            // export budget, without treating the cached pointer as authority.
            const auto preferred=observedMember.load();const auto preferredIndex=At<std::uint32_t>(preferred,0x174);
            if(preferred && preferredIndex.readable && preferredIndex.value<(end.value-begin.value)/8 &&
               Read<Address>(begin.value+Address(preferredIndex.value)*8).value==preferred && At<Address>(preferred,0x178).value==faction.value &&
               std::find(seen.begin(),seen.end(),preferred)==seen.end()){
                auto line=MemberJSON(preferred);
                if(result.members<maxMembers && out.size()+line.size()<=maxBytes){out+=line;seen.push_back(preferred);++result.members;}
                else result.complete=false;
            }
            for(Address p=begin.value;p<end.value;p+=8){
                const auto m=Read<Address>(p);if(m.readable && !m.value)continue;
                if(!m.readable || At<Address>(m.value,0x178).value!=faction.value){result.complete=false;continue;}
                if(std::find(seen.begin(),seen.end(),m.value)!=seen.end())continue;
                if(result.members>=maxMembers){result.complete=false;break;}
                auto line=MemberJSON(m.value);
                if(out.size()+line.size()>maxBytes){result.complete=false;break;}
                out+=line;seen.push_back(m.value);++result.members;
            }
            const auto b=At<Address>(pool.value,0x18),e=At<Address>(pool.value,0x20);
            if(!b.readable || !e.readable || b.value!=begin.value || e.value!=end.value){result.complete=false;out+="{\"type\":\"error\",\"reason\":\"roster-changed-during-read\"}\n";}
            if(result.members>=maxMembers || out.size()>maxBytes-1024*1024){result.complete=false;break;}
        }
        const auto g=At<Address>(defender_repair::image,offset::globalsRva),m=At<Address>(g.value,0x6C68);
        const auto s=At<Address>(m.value,0x98),o=At<Address>(m.value,0xA8);const auto c=At<unsigned>(m.value,0xA0);
        if(!m.readable || m.value!=manager.value || !s.readable || s.value!=slots.value || !c.readable || c.value!=count.value || !o.readable || o.value!=owner.value)result.complete=false;
    }
    out+="{\"type\":\"summary\",\"members\":"+std::to_string(result.members)+",\"enumeration_complete\":"+(result.complete?"true":"false")+",\"note\":\"Best-effort snapshot; per-record/list statuses must also be checked.\"}\n";
    return result;
}
inline void Notify(const char* text){OutputDebugStringA(text);if(GetConsoleWindow()){std::fputs(text,stdout);std::fflush(stdout);}}
inline bool Save(const Capture& capture){
    wchar_t base[MAX_PATH]{};auto n=GetEnvironmentVariableW(L"LOCALAPPDATA",base,MAX_PATH);
    if(!n || n>=MAX_PATH){n=GetTempPathW(MAX_PATH,base);if(!n || n>=MAX_PATH)return false;}
    std::wstring directory(base);if(!directory.empty() && directory.back()!=L'\\')directory+=L'\\';directory+=L"SoWPlugin";
    if(!CreateDirectoryW(directory.c_str(),nullptr) && GetLastError()!=ERROR_ALREADY_EXISTS)return false;
    directory+=L"\\Diagnostics";if(!CreateDirectoryW(directory.c_str(),nullptr) && GetLastError()!=ERROR_ALREADY_EXISTS)return false;
    SYSTEMTIME utc{};GetSystemTime(&utc);wchar_t suffix[160]{};
    swprintf_s(suffix,L"\\Uruks-%04u%02u%02uT%02u%02u%02u-%03uZ-%lu-%u.jsonl",utc.wYear,utc.wMonth,utc.wDay,utc.wHour,utc.wMinute,utc.wSecond,utc.wMilliseconds,GetCurrentProcessId(),++sequence);
    const auto path=directory+suffix;const auto file=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    DWORD written=0;const bool ok=WriteFile(file,capture.text.data(),static_cast<DWORD>(capture.text.size()),&written,nullptr) && written==capture.text.size();CloseHandle(file);
    if(ok){char utf8[2048]{};WideCharToMultiByte(CP_UTF8,0,path.c_str(),-1,utf8,sizeof(utf8),nullptr,nullptr);std::string message="[UrukExport] saved "+std::string(utf8)+" members="+std::to_string(capture.members)+"\n";Notify(message.c_str());}
    return ok;
}
inline bool(*writeCapture)(const Capture&)=&Save;
struct RosterStamp {Address manager=0,key=0,pool=0,begin=0,end=0;std::uint64_t identity=0;};
inline std::vector<RosterStamp> exported;
inline ULONGLONG nextAutoScan=0,nextAutoRetry=0;
struct AutoCooldown {Address key;ULONGLONG next;};
inline std::vector<AutoCooldown> autoCooldowns;
inline ULONGLONG autoCooldownMs=10000;
inline bool AutoDue(Address key,ULONGLONG now){for(const auto& entry:autoCooldowns)if(entry.key==key)return now>=entry.next;return true;}
inline void DelayAuto(Address key,ULONGLONG now){
    for(auto& entry:autoCooldowns)if(entry.key==key){entry.next=now+autoCooldownMs;return;}
    if(autoCooldowns.size()>=64)autoCooldowns.erase(autoCooldowns.begin());autoCooldowns.push_back({key,now+autoCooldownMs});
}
inline bool Same(const RosterStamp& a,const RosterStamp& b){return a.manager==b.manager && a.key==b.key && a.pool==b.pool && a.begin==b.begin && a.end==b.end && a.identity==b.identity;}
inline bool Imported(Address key){
    static constexpr const char* names[]={"InvasionFaction_Island","InvasionFaction_MtnPass","InvasionFaction_SnowMt","InvasionFaction_desert","InvasionFaction_Volcano","InvasionFaction_MinasIthil"};
    return defender_repair::Matches(ReadRecord(key),names);
}
inline bool Stamp(Address key,RosterStamp& stamp){
    if(!Imported(key))return false;
    const auto g=At<Address>(defender_repair::image,offset::globalsRva),m=At<Address>(g.value,0x6C68);
    const auto slots=At<Address>(m.value,0x98),owner=At<Address>(m.value,0xA8),tag=At<Address>(key,0x18);const auto count=At<unsigned>(m.value,0xA0);
    if(!m.readable || !m.value || !slots.readable || !slots.value || !count.readable || count.value>4096 || !owner.readable || !owner.value || !tag.readable || tag.value!=owner.value)return false;
    Address pool=0;unsigned matches=0;
    for(unsigned i=0;i<count.value;++i)if(Read<Address>(slots.value+Address(i)*0x290).value==key){++matches;pool=At<Address>(slots.value+Address(i)*0x290+8,0x20).value;}
    const auto begin=At<Address>(pool,0x18),end=At<Address>(pool,0x20);
    if(matches!=1 || !pool || !begin.readable || !end.readable || !begin.value || end.value<=begin.value || (end.value-begin.value)%8 || (end.value-begin.value)/8>8192)return false;
    std::uint64_t hash=14695981039346656037ull;unsigned live=0;
    for(Address p=begin.value;p<end.value;p+=8){
        const auto member=Read<Address>(p);if(!member.readable)return false;if(!member.value)continue;
        const auto token=At<std::array<Address,3>>(member.value,0x168);
        if(!token.readable || token.value[2]!=key)return false;
        for(auto value:{member.value,token.value[0],token.value[1]}){hash^=value;hash*=1099511628211ull;}++live;
    }
    if(!live || At<Address>(pool,0x18).value!=begin.value || At<Address>(pool,0x20).value!=end.value)return false;
    stamp={m.value,key,pool,begin.value,end.value,hash};return true;
}
// Both locks are held by the caller; no native repair calls are made here.
inline void AutomaticLocked(Address key){
    const auto now=GetTickCount64();if(now<nextAutoRetry || !AutoDue(key,now))return;
    RosterStamp before;if(!Stamp(key,before))return;
    for(const auto& seen:exported)if(Same(seen,before))return;

    try{
        auto capture=Collect(key,"automatic-invasion-roster");RosterStamp after;
        if(!capture.members || !Stamp(key,after) || !Same(before,after))return;
        if(!writeCapture || !writeCapture(capture)){nextAutoRetry=now+5000;Notify("[UrukExport] automatic file write failed; retry delayed\n");return;}
        DelayAuto(key,GetTickCount64());
        if(exported.size()>=64)exported.erase(exported.begin());exported.push_back(before);
    }catch(...){nextAutoRetry=now+5000;Notify("[UrukExport] automatic capture failed; retry delayed\n");}
}
// Native repair/construction already holds stateMutex, so never acquire it again.
inline void BeforeRepair(Address member){
    if(!enabled)return;std::unique_lock<std::mutex> own(mutex,std::try_to_lock);if(!own.owns_lock())return;
    AutomaticLocked(At<Address>(member,0x178).value);
}
inline void ScanAutomaticLocked(){
    const auto now=GetTickCount64();if(now<nextAutoScan)return;nextAutoScan=now+1000;
    const auto g=At<Address>(defender_repair::image,offset::globalsRva),m=At<Address>(g.value,0x6C68),slots=At<Address>(m.value,0x98);
    const auto count=At<unsigned>(m.value,0xA0);if(!slots.readable || !slots.value || !count.readable || count.value>4096)return;
    for(unsigned i=0;i<count.value;++i){const auto key=Read<Address>(slots.value+Address(i)*0x290);if(key.readable && key.value && Imported(key.value))AutomaticLocked(key.value);}
}
inline void Poll(){
    if(!enabled)return;std::unique_lock<std::mutex> own(mutex,std::try_to_lock);if(!own.owns_lock())return;
    DWORD pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&pid);const bool focused=pid==GetCurrentProcessId();
    const bool down=focused && (GetAsyncKeyState(shortcutKey)&0x8000);const bool pressed=down && !keyDown;keyDown=down;
    std::unique_lock<std::mutex> lock(defender_repair::stateMutex,std::try_to_lock);
    if(!lock.owns_lock()){if(pressed)Notify("[UrukExport] repair busy; release and press export key again\n");return;}
    ScanAutomaticLocked();
    if(!pressed)return;
    try{
        const auto capture=Collect();lock.unlock();if(!writeCapture || !writeCapture(capture))Notify("[UrukExport] file creation/write failed\n");
    }catch(...){Notify("[UrukExport] capture failed; no exception forwarded to the game\n");}
}
inline void Formatted(Address member){
    observedMember=member;
    if(enabled){std::unique_lock<std::mutex> lock(defender_repair::stateMutex,std::try_to_lock);if(lock.owns_lock())BeforeRepair(member);}
    Poll();
}
inline bool Install(bool requested=true,int shortcut=VK_F8){
    if(!requested)return true;if(enabled || shortcut<1 || shortcut>255 || defender_repair::onFormattedMember || defender_repair::beforeImportedRepair)return false;
    if(!defender_repair::image)defender_repair::image=reinterpret_cast<Address>(GetModuleHandleW(nullptr));
    static constexpr unsigned char give[]={0x48,0x8B,0xC4,0x48,0x89,0x58,0x08,0x44,0x88,0x48,0x20,0x4C,0x89,0x40,0x18};
    static constexpr unsigned char equip[]={0x48,0x8B,0xC4,0x48,0x89,0x58,0x08,0x48,0x89,0x68,0x10,0x48,0x89,0x78,0x18};
    inventoryLayoutVerified=defender_repair::Signature(defender_repair::image+0x355474,give) && defender_repair::Signature(defender_repair::image+0x61B868,equip);
    shortcutKey=shortcut;keyDown=false;exported.clear();autoCooldowns.clear();autoCooldownMs=10000;nextAutoScan=nextAutoRetry=0;writeCapture=&Save;enabled=true;
    defender_repair::onFormattedMember=&Formatted;defender_repair::beforeImportedRepair=&BeforeRepair;
    Notify("[UrukExport] automatic invasion-roster export enabled; F8 additional loaded-roster snapshot\n");return true;
}
inline void Remove(){enabled=false;if(defender_repair::onFormattedMember==&Formatted)defender_repair::onFormattedMember=nullptr;if(defender_repair::beforeImportedRepair==&BeforeRepair)defender_repair::beforeImportedRepair=nullptr;exported.clear();}
}
