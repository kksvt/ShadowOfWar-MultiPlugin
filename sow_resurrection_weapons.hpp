#pragma once
// Preserve native resurrection behavior and add the selected Ringwraith weapon
// to newly created player-raised Orc grunts. No database or corpse-state writes.
#include "sow_ring_boost.hpp"
#include "sow_ring_might.hpp"
#include "sow_gear_resurrection.hpp"
#include <array>
#include <atomic>
#include <mutex>
#include <cstdio>

namespace sow::resurrection_weapons {
enum class Element : int { None, Fire, Poison, Curse };
using ConstructorFn = Address(__fastcall*)(Address, Address, Address);
using WorkerFn = Address(__fastcall*)(Address);
using HasItemFn = bool(__fastcall*)(Address, Address);
using GiveItemFn = void(__fastcall*)(Address, Address);
inline constexpr Address giveItemRva = 0x1E6B88;
enum class GrantResult { AlreadyPresent, Granted, Unconfirmed };
inline GrantResult AddIfMissing(Address actor, Address inventory, Address item,
    HasItemFn has, GiveItemFn give) {
    if (has(inventory, item)) return GrantResult::AlreadyPresent;
    give(actor, item);
    return has(inventory, item) ? GrantResult::Granted : GrantResult::Unconfirmed;
}
inline std::unique_ptr<PLH::x64Detour> constructorHook, workerHook;
inline std::uint64_t constructorTrampoline = 0, workerTrampoline = 0;
inline bool diagnostics = true;
inline std::atomic<unsigned> logCount{0};
inline std::mutex pendingMutex, logMutex;

inline Element Select(bool fire, bool poison, bool curse) noexcept {
    if (static_cast<unsigned>(fire) + static_cast<unsigned>(poison) +
        static_cast<unsigned>(curse) != 1) return Element::None;
    return fire ? Element::Fire : poison ? Element::Poison : Element::Curse;
}
// Multi-equipped native flags still resolve to one weapon type per raised grunt.
inline std::atomic<bool> multiSelection{false};
inline std::atomic<Element> preferredElement{Element::None};
inline void OnSkillActivated(const char* name) {
    if(std::strcmp(name,"PSS_CBMeter_DarkTalion_Child1")==0)preferredElement=Element::Fire;
    else if(std::strcmp(name,"PSS_CBMeter_DarkTalion_Child2")==0)preferredElement=Element::Poison;
    else if(std::strcmp(name,"PSS_CBMeter_DarkTalion_Child3")==0)preferredElement=Element::Curse;
}
inline Element SelectEquipped(bool fire,bool poison,bool curse) {
    const auto unique=Select(fire,poison,curse);
    if(unique!=Element::None || !multiSelection.load())return unique;
    const auto preferred=preferredElement.load();
    if((preferred==Element::Fire && fire)||(preferred==Element::Poison && poison)||
       (preferred==Element::Curse && curse))return preferred;
    // After process restart the last click is unknown; choose a stable active
    // fallback rather than disabling the previously working weapon extension.
    return fire?Element::Fire:poison?Element::Poison:curse?Element::Curse:Element::None;
}
inline const char* ItemName(Element element) noexcept {
    switch (element) {
    case Element::Fire: return "FlamingWeapon";
    case Element::Poison: return "PoisonedWeapon";
    case Element::Curse: return "CursedWeapon";
    default: return "<none>";
    }
}
inline bool StandardGruntData(const Record& r) noexcept {
    return r.Named("GruntResurrection") || r.Named("GruntResurrection_GiveCurseWeapon");
}
inline void Log(const char* event, Element element = Element::None) {
    if (!diagnostics || logCount.fetch_add(1) >= 64) return;
    char path[MAX_PATH]{};
    const DWORD length = GetTempPathA(MAX_PATH, path);
    constexpr char name[] = "SoW-ResurrectionWeapons.log";
    if (!length || length + sizeof(name) > MAX_PATH) return;
    std::memcpy(path + length, name, sizeof(name));
    std::lock_guard<std::mutex> lock(logMutex);
    FILE* file = nullptr;
    if (fopen_s(&file, path, "a") == 0 && file) {
        std::fprintf(file, "[ResurrectionWeapons] %s element=%s\n", event, ItemName(element));
        std::fclose(file);
    }
}

inline bool SkillFlag(Address variables, const char* name) {
    const auto record = ring_might::Lookup(0x2700AC0, name);
    const auto owner = At<Address>(record, 0x18);
    const auto mapOwner = At<Address>(variables, 0xA0);
    if (!record || !variables || !owner.readable || !owner.value ||
        !mapOwner.readable || mapOwner.value != owner.value) return false;
    using ReadBool = bool(__fastcall*)(Address, Address);
    return reinterpret_cast<ReadBool>(ring_might::Base() + 0x1E48DC)(variables, record);
}
inline Element PlayerElement(Address stats) {
    if (!ring_might::IsPlayer(stats)) return Element::None;
    const auto variables = At<Address>(stats, 0x318);
    if (!variables.readable || !variables.value) return Element::None;
    return SelectEquipped(SkillFlag(variables.value, "SkillB_CBMeter_DarkTalion_Child1"),
        SkillFlag(variables.value, "SkillB_CBMeter_DarkTalion_Child2"),
        SkillFlag(variables.value, "SkillB_CBMeter_DarkTalion_Child3"));
}
inline bool OrcGrunt(Address actor) noexcept {
    const auto index = At<std::int32_t>(actor, offset::actor::rosterIndex);
    const auto stats = At<Address>(actor, 0x26A8);
    const auto marker = At<Address>(stats.value, offset::stats::player);
    const auto type = ReadRecord(At<Address>(stats.value, offset::stats::characterType).value);
    return actor && index.readable && index.value == -1 && stats.readable && stats.value &&
        marker.readable && !marker.value && type.nameComplete &&
        std::strncmp(type.name.data(), "Orc_", 4) == 0;
}

struct Pending {
    Address job = 0, corpse = 0, data = 0;
    Element element = Element::None;
    ULONGLONG expires = 0;
};
inline std::array<Pending, 256> pending{};

// Every constructor invalidates an earlier association for a reused address.
inline void Forget(Address job) {
    std::lock_guard<std::mutex> lock(pendingMutex);
    for (auto& entry : pending) if (entry.job == job) entry = {};
}
inline bool Remember(const Pending& value, ULONGLONG now) {
    std::lock_guard<std::mutex> lock(pendingMutex);
    for (auto& entry : pending) if (!entry.job || entry.expires <= now) {
        entry = value; return true;
    }
    return false;
}
inline Pending Find(Address job, ULONGLONG now) {
    std::lock_guard<std::mutex> lock(pendingMutex);
    for (auto& entry : pending) if (entry.job == job) {
        if (entry.expires <= now) { entry = {}; return {}; }
        return entry;
    }
    return {};
}

inline Address SourcePlayer(const ring_boost::Scope& context) {
    if (ring_might::IsPlayer(context.casterStats)) return context.casterStats;
    const auto tick = ReadRecord(context.tick);
    if (tick.Named("RingwraithGear_Resurrect_AllyDie"))
        return gear_resurrection::EquippedPlayer(gear_resurrection::Gear::Armor);
    if (tick.Named("RingwraithGear_Resurrect"))
        return gear_resurrection::EquippedPlayer(gear_resurrection::Gear::Sword);
    return 0;
}

inline Address __fastcall ConstructorDetour(Address job, Address corpse, Address data) {
    Forget(job);
    Element element = Element::None;
    if (StandardGruntData(ReadRecord(data)) && OrcGrunt(corpse)) {
        auto& context = ring_boost::scope;
        const auto player = SourcePlayer(context);
        if (player) {
            if (context.resurrectionElement < 0)
                context.resurrectionElement = static_cast<int>(PlayerElement(player));
            element = static_cast<Element>(context.resurrectionElement);
            if (element == Element::None) Log("no unique weapon upgrade selected");
        }
    }
    const auto original = PLH::FnCast(constructorTrampoline, static_cast<ConstructorFn>(nullptr));
    const auto result = original(job, corpse, data);
    if (result && element != Element::None) {
        const auto now = GetTickCount64();
        if (Remember({result, corpse, data, element, now + 30000}, now))
            Log("queued player grunt", element);
        else Log("pending slots exhausted; skipped", element);
    }
    return result;
}

inline void Equip(Address actor, Element element) {
    if (!OrcGrunt(actor)) { Log("spawned actor was not an Orc grunt; skipped", element); return; }
    const auto inventory = At<Address>(actor, 0x2348);
    const auto item = ring_might::Lookup(0x2700530, ItemName(element));
    if (!inventory.readable || !inventory.value || !item) {
        Log("weapon/inventory unavailable; skipped", element); return;
    }
    const auto base = ring_might::Base();
    // 0x1E6B88 -> 0x1C01E0 -> 0x1C1224 adds an item, then refreshes the actor.
    // 0x4F12B4 instead removes an item via 0x4F163C -> 0x1C0840.
    const auto result = AddIfMissing(actor, inventory.value, item,
        reinterpret_cast<HasItemFn>(base + 0x24DF7C),
        reinterpret_cast<GiveItemFn>(base + giveItemRva));
    Log(result == GrantResult::AlreadyPresent ? "weapon already present" :
        result == GrantResult::Granted ? "weapon granted" : "weapon grant unconfirmed", element);
}

inline Address __fastcall WorkerDetour(Address job) {
    auto selected = Find(job, GetTickCount64());
    if (selected.job && (At<Address>(job, 0x38).value != selected.corpse ||
        At<Address>(job, 0x40).value != selected.data)) {
        Forget(job); selected = {};
    }
    const auto original = PLH::FnCast(workerTrampoline, static_cast<WorkerFn>(nullptr));
    const auto actor = original(job);
    if (selected.job && actor) {
        Forget(job);
        Equip(actor, selected.element);
    }
    return actor;
}

// Quiesce game threads before removal, including queued job worker execution.
inline bool Remove() {
    bool ok = true;
    if (constructorHook) {
        if (constructorHook->unHook()) constructorHook.reset(); else ok = false;
    }
    if (workerHook) { if (workerHook->unHook()) workerHook.reset(); else ok = false; }
    if (ok) { std::lock_guard<std::mutex> lock(pendingMutex); pending = {}; }
    return ok;
}
inline bool Install(bool logWeapons = true) {
    if (constructorHook || workerHook || !ring_boost::tickHook) return false;
    diagnostics = logWeapons;
    const auto base = ring_might::Base();
    workerHook = std::make_unique<PLH::x64Detour>(base + 0x1A92558,
        reinterpret_cast<std::uint64_t>(&WorkerDetour), &workerTrampoline);
    if (!workerHook->hook()) { workerHook.reset(); Log("worker hook FAILED"); return false; }
    constructorHook = std::make_unique<PLH::x64Detour>(base + 0x1A90520,
        reinterpret_cast<std::uint64_t>(&ConstructorDetour), &constructorTrampoline);
    if (!constructorHook->hook()) {
        constructorHook.reset(); Remove(); Log("constructor hook FAILED"); return false;
    }
    Log("v3 installed; Raise Dead and tracked armor/Urfael; selected Ringwraith upgrade");
    return true;
}
} // namespace sow::resurrection_weapons
