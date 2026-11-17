#pragma once
#include "sow_entities.hpp"
namespace sow::defender_slots {
struct Slot {Address picker=0,trait=0,unlocked=0;std::uint16_t flags=0;std::int16_t option=-1;std::uint32_t padding=0;};
static_assert(sizeof(Slot)==0x20);
struct Snapshot {std::array<Slot,512> slots{};std::size_t size=0;bool valid=false;};
inline Snapshot ReadSlots(Address member){
    Snapshot out;const auto b=At<Address>(member,0x1E0),e=At<Address>(member,0x1E8);
    const auto cap=At<unsigned>(member,0x1F0);
    if(!b.readable || !e.readable || !cap.readable || e.value<b.value || (e.value-b.value)%32 ||
        (e.value-b.value)/32>512 || (e.value-b.value)/32>cap.value || cap.value>4096 || (e.value!=b.value && !b.value))return out;
    out.size=(e.value-b.value)/32;
    for(std::size_t i=0;i<out.size;++i){const auto s=Read<Slot>(b.value+i*32);if(!s.readable)return out;out.slots[i]=s.value;}
    const auto bb=At<Address>(member,0x1E0),ee=At<Address>(member,0x1E8);
    out.valid=bb.readable && ee.readable && bb.value==b.value && ee.value==e.value;return out;
}
using EraseFn=bool(__fastcall*)(Address,unsigned);
using PushFn=bool(__fastcall*)(Address,const Slot*);
using AddFn=bool(__fastcall*)(Address,Address);
inline EraseFn erase=nullptr;inline PushFn push=nullptr;inline AddFn add=nullptr;
using UpdateFn=void(__fastcall*)(Address,unsigned char);inline UpdateFn update=nullptr;
inline bool Editable(Address member){const auto flags=At<std::uint64_t>(member,0x508);return flags.readable && !(flags.value&(1ULL<<38));}
inline bool Erase(Address member,unsigned index){
    const auto s=ReadSlots(member);if(!erase || !s.valid || index>=s.size || !Editable(member))return false;
    if(!erase(member+0x1E0,index))return false;
    const auto after=ReadSlots(member);if(!after.valid || after.size+1!=s.size)return false;
    for(std::size_t i=0;i<after.size;++i){const auto& old=s.slots[i+(i>=index)];const auto& now=after.slots[i];
        if(now.picker!=old.picker || now.trait!=old.trait || now.unlocked!=old.unlocked || now.flags!=old.flags || now.option!=old.option)return false;}
    return true;
}
inline bool EraseTraitSlots(Address member,Address trait){
    auto s=ReadSlots(member);if(!s.valid || !Editable(member))return false;
    for(std::size_t i=s.size;i>0;--i)if(s.slots[i-1].trait==trait || s.slots[i-1].unlocked==trait)
        if(!Erase(member,static_cast<unsigned>(i-1)))return false;
    return true;
}
inline const char* BasePicker(const Record& model){
    if(model.Named("Orc_Infantry") || model.Named("Orc_Infantry_Tough"))return "Picker_BaseStrength_Infantry";
    if(model.Named("Orc_Defender") || model.Named("Orc_Defender_Tough"))return "Picker_BaseStrength_Defender";
    if(model.Named("Orc_Archer") || model.Named("Orc_Archer_Tough"))return "Picker_BaseStrength_Archer";
    if(model.Named("Orc_Spearman") || model.Named("Orc_Spearman_Tough"))return "Picker_BaseStrength_Hunter";
    if(model.Named("Orc_Berserker") || model.Named("Orc_Berserker_Tough"))return "Picker_BaseStrength_Savage";
    if(model.Named("OlogHai_Infantry"))return "Picker_BaseStrength_Olog";
    return nullptr;
}
inline bool IsBasePicker(const Record& r){
    static constexpr const char* names[]={"Picker_BaseStrength_Infantry","Picker_BaseStrength_Defender","Picker_BaseStrength_Archer",
        "Picker_BaseStrength_Hunter","Picker_BaseStrength_Savage","Picker_BaseStrength_Olog"};
    for(const auto* n:names)if(r.Named(n))return true;return false;
}
}
