#pragma once
#include "sow_entities.hpp"
namespace sow::defender_inventory_audit {
// Verified RVA 355474 / 61B868: inventory at member+50, 10 weapon +15
// armour instance pointers, instance+38 Item record; look records at inv+E0.
// RVA 36A17C / 509074: visual+740 pointer vector -> stack {record,+20 count}.
inline constexpr const char* trackedNames[]={"orc_Cursed","olog_Cursed","IT_Albino","IT_DarkSkin",
    "CursedWeapon","Item_HasACurseTrait","Item_HasANecromancyTrait","Item_Necromancer_SelfRes"};
inline int Tracked(const Record& record){for(unsigned i=0;i<std::size(trackedNames);++i)if(record.Named(trackedNames[i]))return static_cast<int>(i);return -1;}
struct Snapshot {
    std::array<Address,25> instances{},equipment{};
    std::array<Address,512> looks{};std::size_t lookCount=0;
    std::array<std::uint32_t,std::size(trackedNames)> runtimeCounts{};
    bool equipmentComplete=false,looksComplete=false,runtimeComplete=false,spawned=false,runtimeLinked=false;
    bool equipmentOrcCurse=false,equipmentOlogCurse=false,lookOrcCurse=false,lookOlogCurse=false;
    Address actor=0,visual=0;
    bool CursedSkinEvidence()const{return (equipmentComplete && (equipmentOrcCurse || equipmentOlogCurse)) || (looksComplete && (lookOrcCurse || lookOlogCurse)) || (runtimeComplete && (runtimeCounts[0]>0 || runtimeCounts[1]>0));}
};
inline Snapshot ReadInventory(Address member){
    Snapshot out;
    const auto slots=At<std::array<Address,25>>(member,0x50);
    if(slots.readable){
        out.instances=slots.value;out.equipmentComplete=true;
        for(unsigned i=0;i<25;++i)if(slots.value[i]){
            const auto item=At<Address>(slots.value[i],0x38);const auto record=ReadRecord(item.value);
            if(!item.readable || !item.value || !record.nameComplete){out.equipmentComplete=false;continue;}
            out.equipment[i]=item.value;out.equipmentOrcCurse|=record.Named("orc_Cursed");out.equipmentOlogCurse|=record.Named("olog_Cursed");
        }
        const auto after=At<std::array<Address,25>>(member,0x50);
        out.equipmentComplete&=after.readable && after.value==slots.value;
        for(unsigned i=0;out.equipmentComplete && i<25;++i)if(out.instances[i])
            out.equipmentComplete=At<Address>(out.instances[i],0x38).readable && At<Address>(out.instances[i],0x38).value==out.equipment[i];
    }
    const auto begin=At<Address>(member,0x130),end=At<Address>(member,0x138);
    if(begin.readable && end.readable && end.value>=begin.value && !((end.value-begin.value)%8) &&
       (end.value-begin.value)/8<=512 && (begin.value || begin.value==end.value)){
        out.lookCount=(end.value-begin.value)/8;out.looksComplete=true;
        for(std::size_t i=0;i<out.lookCount;++i){const auto pointer=Read<Address>(begin.value+i*8);const auto record=ReadRecord(pointer.value);
            out.looks[i]=pointer.value;if(!pointer.readable || !record.nameComplete){out.looksComplete=false;continue;}
            out.lookOrcCurse|=record.Named("orc_Cursed");out.lookOlogCurse|=record.Named("olog_Cursed");}
        const auto b=At<Address>(member,0x130),e=At<Address>(member,0x138);
        out.looksComplete&=b.readable && e.readable && b.value==begin.value && e.value==end.value;
        for(std::size_t i=0;out.looksComplete && i<out.lookCount;++i){const auto pointer=Read<Address>(begin.value+i*8);out.looksComplete=pointer.readable && pointer.value==out.looks[i];}
    }
    const auto actor=At<Address>(member,0x48);if(!actor.readable || !actor.value)return out;
    out.spawned=true;out.actor=actor.value;
    const auto link=At<Address>(actor.value,0x2708);const auto index=At<std::uint32_t>(actor.value,0x2710);
    const auto key=At<Address>(member,0x178);const auto memberIndex=At<std::uint32_t>(member,0x174);
    if(!link.readable || !index.readable || !key.readable || !memberIndex.readable || link.value!=key.value || index.value!=memberIndex.value)return out;
    out.runtimeLinked=true;
    const auto visual=At<Address>(actor.value,0x2348);out.visual=visual.value;
    const auto b=At<Address>(visual.value,0x740),e=At<Address>(visual.value,0x748);
    if(!visual.readable || !visual.value || !b.readable || !e.readable || e.value<b.value || (e.value-b.value)%8 ||
       (e.value-b.value)/8>4096 || (!b.value && b.value!=e.value))return out;
    out.runtimeComplete=true;
    for(Address p=b.value;p<e.value;p+=8){
        const auto stack=Read<Address>(p),recordPointer=At<Address>(stack.value,0);
        const auto count=At<std::uint32_t>(stack.value,0x20);
        if(!stack.readable || !stack.value || !recordPointer.readable || !recordPointer.value || !count.readable){out.runtimeComplete=false;continue;}
        if(!count.value)continue;
        const auto record=ReadRecord(recordPointer.value);if(!record.nameComplete){out.runtimeComplete=false;continue;}
        const auto tracked=Tracked(record);if(tracked>=0){
            auto& total=out.runtimeCounts[static_cast<unsigned>(tracked)];
            if(UINT32_MAX-total<count.value)out.runtimeComplete=false;else total+=count.value;
        }
    }
    const auto bb=At<Address>(visual.value,0x740),ee=At<Address>(visual.value,0x748),aa=At<Address>(member,0x48),vv=At<Address>(actor.value,0x2348);
    const auto finalKey=At<Address>(actor.value,0x2708),memberKey=At<Address>(member,0x178);
    const auto finalIndex=At<std::uint32_t>(actor.value,0x2710),memberIndexAfter=At<std::uint32_t>(member,0x174);
    out.runtimeComplete&=bb.readable && ee.readable && aa.readable && vv.readable && bb.value==b.value && ee.value==e.value && aa.value==actor.value && vv.value==visual.value &&
        finalKey.readable && memberKey.readable && finalIndex.readable && memberIndexAfter.readable && finalKey.value==memberKey.value && finalIndex.value==memberIndexAfter.value && memberKey.value==key.value && memberIndexAfter.value==memberIndex.value;
    return out;
}
}
