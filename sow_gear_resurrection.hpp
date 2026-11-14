#pragma once
#include "sow_ring_might.hpp"

namespace sow::gear_resurrection {
using LinkFn = Address(__fastcall*)(Address);
using EvaluateFn = float(__fastcall*)(Address, int, bool);
using ChangeFn = void(__fastcall*)(Address, Address, float);
struct NativeAPI { LinkFn chance = nullptr, variable = nullptr;
    EvaluateFn evaluate = nullptr; ChangeFn add = nullptr, subtract = nullptr; };
inline NativeAPI native;
inline bool installed = false, armorEnabled = true, swordEnabled = true, diagnostics = true;
inline std::mutex stateMutex, logMutex;
inline std::atomic<unsigned> logCount{0};
enum class Gear { None, Armor, Sword };
struct Grant {
    Address instance = 0, stats = 0, actor = 0, manager = 0, chance = 0;
    float delta = 0;
    Gear gear = Gear::None;
};
inline std::array<Grant, 32> grants{};

inline Gear Select(const Record& record) {
    if (armorEnabled && record.Named("4Armor_5Lgnd_SetProp_Ringwraith_Black")) return Gear::Armor;
    if (swordEnabled && record.Named("1Sword_5Lgnd_SetProp_Ringwraith")) return Gear::Sword;
    return Gear::None;
}
inline const char* Name(Gear gear) { return gear == Gear::Armor ? "armor" : "Urfael"; }
inline void Log(const char* event, Gear gear = Gear::None, float value = 0) {
    if (!diagnostics || logCount.fetch_add(1) >= 32) return;
    char path[MAX_PATH]{};
    const auto length = GetTempPathA(MAX_PATH, path);
    constexpr char name[] = "SoW-GearResurrection.log";
    if (!length || length + sizeof(name) > MAX_PATH) return;
    std::memcpy(path + length, name, sizeof(name));
    std::lock_guard<std::mutex> lock(logMutex);
    FILE* file = nullptr;
    if (fopen_s(&file, path, "a") == 0 && file) {
        std::fprintf(file, "[GearResurrection] %s gear=%s delta=%.3f\n", event,
            gear == Gear::None ? "<none>" : Name(gear), value);
        std::fclose(file);
    }
}
inline bool Live(const Grant& grant) {
    return grant.instance && ring_might::IsPlayer(grant.stats) &&
        At<Address>(grant.stats, 0x28).value == grant.actor &&
        At<Address>(grant.actor, 0x26A8).value == grant.stats &&
        At<Address>(grant.stats, 0x320).value == grant.manager;
}
// Gear-triggered AOEs may be owned by the corpse carrying the proc buff.
// Resolve the equipped player's skill context from a live tracked gear grant.
inline Address EquippedPlayer(Gear gear) {
    Address player = 0;
    std::lock_guard<std::mutex> lock(stateMutex);
    for (const auto& grant : grants) if (grant.gear == gear && Live(grant)) {
        if (player && player != grant.stats) return 0;
        player = grant.stats;
    }
    return player;
}
inline Grant Resolve(Address instance, Address stats, int level) {
    Grant grant;
    if (!ring_might::IsPlayer(stats)) return grant;
    const auto definition = At<Address>(instance, 0);
    const auto gear = Select(ReadRecord(definition.value));
    if (gear == Gear::None) return grant;
    const auto chance = native.chance(definition.value);
    const auto variable = native.variable(definition.value);
    const auto chanceRecord = ReadRecord(chance);
    if (!variable || !chanceRecord.Named(gear == Gear::Armor ?
        "C_Resurrect_AllyDeath" : "C_Resurrect_MeleeKill")) return grant;
    const float stock = native.evaluate(variable, level, false);
    if (!std::isfinite(stock) || stock < 0 || stock > 1) return grant;
    grant = {instance, stats, At<Address>(stats, 0x28).value,
        At<Address>(stats, 0x320).value, chance, 1.0f - stock, gear};
    return grant.manager && Live(grant) ? grant : Grant{};
}
inline void __fastcall Before(Address instance, bool enable, Address stats, int, Address) {
    if (enable) return;
    Grant removed;
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        for (auto& grant : grants) if (grant.instance == instance && grant.stats == stats) {
            removed = grant; grant = {}; break;
        }
    }
    if (removed.instance && Live(removed)) {
        native.subtract(removed.manager, removed.chance, removed.delta);
        Log("removed bonus before native unequip", removed.gear, removed.delta);
    }
}
inline void __fastcall After(Address instance, bool enable, Address stats, int level, Address) {
    if (!enable) return;
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        for (const auto& grant : grants)
            if (grant.instance == instance && grant.stats == stats) return;
    }
    const auto added = Resolve(instance, stats, level);
    if (!added.instance) return;
    bool stored = false;
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        for (const auto& grant : grants)
            if (grant.instance == instance && grant.stats == stats) return;
        for (auto& grant : grants) if (!grant.instance) { grant = added; stored = true; break; }
    }
    if (!stored) { Log("tracking slots exhausted; unchanged"); return; }
    native.add(added.manager, added.chance, added.delta);
    Log("native chance bonus added; affix contribution=1.000", added.gear, added.delta);
}
// Equipment callbacks and game threads must be quiescent during installation/removal.
inline bool Remove() {
    if (ring_might::beforeApplyObserver == &Before) ring_might::beforeApplyObserver = nullptr;
    if (ring_might::afterApplyObserver == &After) ring_might::afterApplyObserver = nullptr;
    std::array<Grant, 32> previous;
    { std::lock_guard<std::mutex> lock(stateMutex); previous = grants; grants = {}; }
    for (const auto& grant : previous) if (grant.instance && Live(grant))
        native.subtract(grant.manager, grant.chance, grant.delta);
    installed = false;
    return true;
}
inline bool Install(bool armor = true, bool sword = true, bool log = true) {
    if (installed || !ring_might::applyHook || ring_might::beforeApplyObserver ||
        ring_might::afterApplyObserver) return false;
    armorEnabled = armor; swordEnabled = sword; diagnostics = log; logCount = 0;
    const auto base = ring_might::Base();
    native = {reinterpret_cast<LinkFn>(base + 0x17DB90C),
        reinterpret_cast<LinkFn>(base + 0x17DB958),
        reinterpret_cast<EvaluateFn>(base + 0x64AA40),
        reinterpret_cast<ChangeFn>(base + 0x1A2B314),
        reinterpret_cast<ChangeFn>(base + 0x1A2B654)};
    ring_might::beforeApplyObserver = &Before; ring_might::afterApplyObserver = &After;
    installed = true;
    Log("installed; equip armor/Urfael after installation");
    return true;
}
} // namespace sow::gear_resurrection
