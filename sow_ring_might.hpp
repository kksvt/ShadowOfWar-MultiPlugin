#pragma once
// Reuse the DLC4 native Ally_HitEnemy proc while the ring affix is applied.
// No damage-calculation trigger, database patch, or direct Might-meter write.
#include "sow_entities.hpp"
#include <polyhook2/Detour/x64Detour.hpp>
#include <polyhook2/Misc.hpp>
#include <array>
#include <memory>
#include <mutex>
#include <atomic>
#include <cstdio>

namespace sow::ring_might {
using ApplyFn = void(__fastcall*)(Address, bool, Address, int, Address);
// Optional equipment observers share this detour; do not detour FE68F0 twice.
inline ApplyFn beforeApplyObserver = nullptr, afterApplyObserver = nullptr;
using ProcFn = void(__fastcall*)(Address, Address, Address);
using AddVariableFn = void(__fastcall*)(Address, Address, float);
struct NativeAPI { ProcFn addProc = nullptr, removeProc = nullptr; AddVariableFn addVariable = nullptr; };
inline NativeAPI native;
inline std::unique_ptr<PLH::x64Detour> applyHook;
inline std::uint64_t applyTrampoline = 0;
inline float mightPerHit = 4.0f; // DLC4's shipped amount; configure before Install.
inline bool diagnostics = true; // At most 32 equipment-state lines per launch.
inline thread_local bool changing = false;
inline std::atomic<unsigned> logCount{0};
inline std::mutex stateMutex, logMutex;

inline void Log(const char* event) {
    if (!diagnostics || logCount.fetch_add(1) >= 32) return;
    char path[MAX_PATH]{};
    const DWORD length = GetTempPathA(MAX_PATH, path);
    constexpr char name[] = "SoW-RingMight.log";
    if (!length || length + sizeof(name) > MAX_PATH) return;
    std::memcpy(path + length, name, sizeof(name));
    std::lock_guard<std::mutex> lock(logMutex);
    FILE* file = nullptr;
    if (fopen_s(&file, path, "a") == 0 && file) {
        std::fprintf(file, "[RingMight] %s amount=%.3f\n", event, mightPerHit);
        std::fclose(file);
    }
}

struct Grant {
    Address instance = 0, stats = 0, variables = 0, variable = 0, proc = 0;
    float amount = 0;
};
inline std::array<Grant, 16> grants{};

inline Address Base() noexcept {
    return reinterpret_cast<Address>(GetModuleHandleW(nullptr));
}
inline bool IsPlayer(Address stats) noexcept {
    const auto actor = At<Address>(stats, offset::stats::actor);
    const auto marker = At<Address>(stats, offset::stats::player);
    return actor.readable && actor.value && marker.readable && actor.value == marker.value;
}
inline bool IsRingAffix(const Record& r) noexcept {
    return r.Named("6Ring_5Lgnd_SetProp_Ringwraith");
}

inline Address Lookup(Address tableRva, const char* name) {
    const auto base = Base();
    const auto table = At<Address>(base, tableRva);
    const auto count = At<std::uint32_t>(table.value, 0x28);
    const auto entries = At<Address>(table.value, 0x38);
    if (!table.readable || !table.value || !count.readable || !count.value ||
        count.value > 1000000 || !entries.readable || !entries.value) return 0;
    using Hash = std::uint32_t(__fastcall*)(const char*, Address, std::uint32_t);
    using Find = Address(__fastcall*)(Address, const std::uint32_t*, std::uint32_t);
    const auto hash = reinterpret_cast<Hash>(base + 0x423E70)(name, 0, 0);
    const auto result = reinterpret_cast<Find>(base + 0x278540)(entries.value, &hash, count.value);
    const auto owner = At<Address>(result, 0x18);
    return owner.readable && owner.value == table.value &&
        ReadRecord(result).Named(name) ? result : 0;
}

inline bool Live(const Grant& grant) noexcept {
    const auto variables = At<Address>(grant.stats, 0x318);
    const auto owner = At<Address>(grant.variable, 0x18);
    const auto floatOwner = At<Address>(grant.variables, 0x70);
    return IsPlayer(grant.stats) && variables.readable &&
        variables.value == grant.variables && grant.variables && owner.readable &&
        owner.value && floatOwner.readable && floatOwner.value == owner.value &&
        ReadRecord(grant.variable).Named("DLC4_MightGain_OnFollowerHitEnemy") &&
        ReadRecord(grant.proc).Named("ProcDef_Might_OnFollowerHitEnemy");
}

inline Grant Resolve(Address instance, Address stats) {
    Grant grant;
    if (!IsPlayer(stats) || !IsRingAffix(ReadRecord(At<Address>(instance, 0).value)))
        return grant;
    const auto donor = Lookup(0x2700488, "5Armor_5Lgnd_Might_OnFollowerHitEnemy");
    const auto schema = At<Address>(Base(), 0x264CF60);
    if (!donor || !schema.readable || !schema.value) return grant;
    // Generic native single-record getter. Affix Proc property is schema+0x650.
    using GetLink = Address(__fastcall*)(Address, Address, unsigned, const Address*);
    const Address fallback = 0;
    grant.proc = reinterpret_cast<GetLink>(Base() + 0x115AE24)(
        donor, schema.value + 0x650, 0, &fallback);
    grant.variable = Lookup(0x2700B20, "DLC4_MightGain_OnFollowerHitEnemy");
    grant.variables = At<Address>(stats, 0x318).value;
    grant.stats = stats; grant.instance = instance; grant.amount = mightPerHit;
    return Live(grant) ? grant : Grant{};
}

inline void Add(const Grant& grant) {
    native.addVariable(grant.variables, grant.variable, grant.amount);
    // First argument is unused by these two native proc lifecycle helpers.
    native.addProc(0, grant.stats, grant.proc);
}
inline void Subtract(const Grant& grant) {
    native.removeProc(0, grant.stats, grant.proc);
    native.addVariable(grant.variables, grant.variable, -grant.amount);
}

inline void Update(Address instance, bool enable, Address stats) {
    if (!enable) {
        Grant removed;
        {
            std::lock_guard<std::mutex> lock(stateMutex);
            for (auto& grant : grants) if (grant.instance == instance && grant.stats == stats) {
                removed = grant; grant = {}; break;
            }
        }
        if (removed.instance) {
            if (Live(removed)) { Subtract(removed); Log("removed native ally-hit proc"); }
            else Log("stale context; removal skipped");
        }
        return;
    }
    if (!IsPlayer(stats) || !IsRingAffix(ReadRecord(At<Address>(instance, 0).value))) return;
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        for (const auto& grant : grants)
            if (grant.instance == instance && grant.stats == stats) return;
    }
    const auto added = Resolve(instance, stats);
    if (!added.instance) { Log("ring found; native proc/variable resolution FAILED"); return; }
    bool stored = false;
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        for (const auto& grant : grants)
            if (grant.instance == instance && grant.stats == stats) return;
        for (auto& grant : grants) if (!grant.instance) { grant = added; stored = true; break; }
    }
    if (!stored) { Log("grant slots exhausted; skipped"); return; }
    Add(added);
    Log("native ally-hit proc enable requested");
}

inline void __fastcall ApplyDetour(Address instance, bool enable, Address stats,
    int level, Address modifiers) {
    const auto original = PLH::FnCast(applyTrampoline, static_cast<ApplyFn>(nullptr));
    // Preserve native equipment processing, including the ring's existing buffs.
    if (beforeApplyObserver) beforeApplyObserver(instance, enable, stats, level, modifiers);
    original(instance, enable, stats, level, modifiers);
    if (afterApplyObserver) afterApplyObserver(instance, enable, stats, level, modifiers);
    if (changing) return;
    changing = true;
    Update(instance, enable, stats);
    changing = false;
}

// Quiesce game threads first, as with the other plugin hooks.
inline bool Remove() {
    if (applyHook && !applyHook->unHook()) return false;
    applyHook.reset();
    std::array<Grant, 16> previous;
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        previous = grants; grants = {};
    }
    changing = true;
    for (const auto& grant : previous) if (grant.instance && Live(grant)) Subtract(grant);
    changing = false;
    return true;
}

inline bool Install(float amount = 4.0f, bool logEquipmentChanges = true) {
    if (applyHook || !std::isfinite(amount) || amount <= 0 || amount > 100) return false;
    mightPerHit = amount; diagnostics = logEquipmentChanges;
    native = {reinterpret_cast<ProcFn>(Base() + 0x7A2AF0),
        reinterpret_cast<ProcFn>(Base() + 0x7A2900),
        reinterpret_cast<AddVariableFn>(Base() + 0x1A105D8)};
    applyHook = std::make_unique<PLH::x64Detour>(Base() + 0xFE68F0,
        reinterpret_cast<std::uint64_t>(&ApplyDetour), &applyTrampoline);
    if (!applyHook->hook()) { applyHook.reset(); Log("installation FAILED"); return false; }
    Log("installed; waiting for ring affix application");
    return true;
}
} // namespace sow::ring_might
