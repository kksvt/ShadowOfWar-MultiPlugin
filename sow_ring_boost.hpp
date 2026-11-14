#pragma once
// Isildur's Ring: enhance native Bigger corpse limits and mask-bridge casts.
// No database writes. Temporary bounded file diagnostics are enabled below.
// Build-specific RVAs; include once.
#include "sow_entities.hpp"
#include <polyhook2/Detour/x64Detour.hpp>
#include <polyhook2/Misc.hpp>
#include <memory>
#include <cstdio>
#include <mutex>
#include <atomic>
#include <cstdarg>

namespace sow::ring_boost {
using LimitFn = int(__fastcall*)(Address);
using RadiusFn = float(__fastcall*)(Address, float);
using TickFn = void(__fastcall*)(Address, Address, Address, Address);
using ProcessFn = int(__fastcall*)(Address, Address, Address, Address);
inline std::unique_ptr<PLH::x64Detour> limitHook, radiusHook, tickHook;
inline std::unique_ptr<PLH::x64Detour> processHook;
inline std::uint64_t limitTrampoline = 0, radiusTrampoline = 0, tickTrampoline = 0;
inline std::uint64_t processTrampoline = 0;
inline bool diagnosticsEnabled = true; // Temporary bounded file capture.
inline std::mutex logMutex;
inline std::atomic<unsigned> logTicks{0};
inline void Log(const char* format, ...) {
    if (!diagnosticsEnabled) return;
    char text[1024]{};
    va_list args; va_start(args, format);
    vsnprintf(text, sizeof(text), format, args); va_end(args);
    char path[MAX_PATH]{};
    const DWORD size = GetTempPathA(MAX_PATH, path);
    constexpr char suffix[] = "SoW-RaiseDead-diagnostic.log";
    if (!size || size >= MAX_PATH || size + sizeof(suffix) > MAX_PATH) return;
    std::memcpy(path + size, suffix, sizeof(suffix));
    std::lock_guard<std::mutex> lock(logMutex);
    FILE* file = nullptr;
    if (fopen_s(&file, path, "a") == 0 && file) {
        std::fprintf(file, "%s\n", text); std::fclose(file);
    }
}
inline int countMultiplier = 4; // Configure while hooks are inactive.
inline bool includeMask = true;
struct Scope {
    Address tick = 0; int contextBaseLimit = 0;
    bool capture = false;
    int nativeLimit = -999, appliedLimit = -999;
    unsigned limitReads = 0, passes = 0, candidates = 0, dead = 0;
    unsigned missingObject = 0, blocked = 0, preDispatch = 0, unreadable = 0;
    float radius = -1;
    Address casterStats = 0;
    int resurrectionElement = -1; // Lazily cached per AOE tick by weapon extension.
};
inline thread_local Scope scope;

inline bool IsRingTick(const Record& r) noexcept {
    return r.Named("NecroAoE_RaiseGrunts_Tick_Lvl1_Bigger") ||
        r.Named("NecroAoE_RaiseGrunts_Tick_Lvl2_Bigger") ||
        r.Named("NecroAoE_RaiseMoreGrunts_Tick_Lvl1_Bigger") ||
        r.Named("NecroAoE_RaiseMoreGrunts_Tick_Lvl2_Bigger");
}

inline bool HasRingBonus(Address stats) noexcept {
    const auto base = reinterpret_cast<Address>(GetModuleHandleW(nullptr));
    const auto list = At<Address>(base, 0x2700AC0);
    const auto count = At<std::uint32_t>(list.value, 0x28);
    const auto entries = At<Address>(list.value, 0x38);
    if (!list.readable || !count.readable || !count.value || count.value > 65536 ||
        !entries.readable || !entries.value) return false;
    // Native lookup performs the same bounded name-hash search in this table.
    using Lookup = Address(__fastcall*)(const char*);
    const auto variable = reinterpret_cast<Lookup>(base + 0x7B40C8)(
        "Ringwraith_RaiseAoE_Bigger");
    if (!ReadRecord(variable).Named("Ringwraith_RaiseAoE_Bigger")) return false;
    const auto owner = At<Address>(variable, 0x18);
    // Native summon-request caller RVA 0xE8DD10 loads variables from +0x318.
    // +0x68 is a different context and must not be used for this bool map.
    const auto variables = At<Address>(stats, 0x318);
    const auto mapOwner = At<Address>(variables.value, 0xA0);
    if (!variables.readable || !variables.value || !owner.readable ||
        owner.value != list.value || !mapOwner.readable || mapOwner.value != owner.value)
        return false;
    // Native bool map getter: (variable context, variable record).
    using ReadBool = bool(__fastcall*)(Address, Address);
    return reinterpret_cast<ReadBool>(base + 0x1E48DC)(variables.value, variable);
}

// Select a stock ring cap for ordinary ticks under enhanced player attacks.
// Native Bigger tick records are handled independently by IsRingTick().
inline int ContextBaseLimit(const Record& attack, const Record& tick,
    bool ringFlag, bool allowMask) noexcept {
    const bool grunts = tick.Named("NecroAoE_RaiseGrunts_Tick_Lvl1") ||
        tick.Named("NecroAoE_RaiseGrunts_Tick_Lvl2");
    const bool more = tick.Named("NecroAoE_RaiseMoreGrunts_Tick_Lvl1") ||
        tick.Named("NecroAoE_RaiseMoreGrunts_Tick_Lvl2");
    if (!grunts && !more) return 0;
    unsigned level = 0;
    if (attack.Named("NecroAoE_Attack_Lvl1_Bigger") ||
        attack.Named("NecroAoE_Attack_Lvl1_Bigger_Fear")) level = 1;
    else if (attack.Named("NecroAoE_Attack_Lvl2_Bigger") ||
        attack.Named("NecroAoE_Attack_Lvl2_Bigger_Fear")) level = 2;
    else if (allowMask && ringFlag) {
        if (attack.Named("NecroAoE_Attack_Lvl1_Ghosts")) level = 1;
        else if (attack.Named("NecroAoE_Attack_Lvl2_Ghosts")) level = 2;
    }
    if (!level) return 0;
    return more ? (level == 1 ? 9 : 12) : (level == 1 ? 6 : 10);
}

inline int CasterBaseLimit(Address aoe, Address tick) noexcept {
    const auto tickName = ReadRecord(tick);
    if (!tickName.Named("NecroAoE_RaiseGrunts_Tick_Lvl1") &&
        !tickName.Named("NecroAoE_RaiseGrunts_Tick_Lvl2") &&
        !tickName.Named("NecroAoE_RaiseMoreGrunts_Tick_Lvl1") &&
        !tickName.Named("NecroAoE_RaiseMoreGrunts_Tick_Lvl2")) return 0;
    const auto stats = At<Address>(aoe, 0x38);
    if (!stats.readable || !stats.value) return 0;
    const auto actor = At<Address>(stats.value, offset::stats::actor);
    const auto player = At<Address>(stats.value, offset::stats::player);
    if (!actor.readable || !actor.value || !player.readable || player.value != actor.value)
        return 0;
    const auto root = At<Address>(stats.value, 0xE0);
    const auto name = ReadRecord(root.value);
    const bool isMask = name.Named("NecroAoE_Attack_Lvl1_Ghosts") ||
        name.Named("NecroAoE_Attack_Lvl2_Ghosts");
    return ContextBaseLimit(name, tickName,
        includeMask && isMask && HasRingBonus(stats.value), includeMask);
}

inline int __fastcall LimitDetour(Address tick) {
    const auto original = PLH::FnCast(limitTrampoline, static_cast<LimitFn>(nullptr));
    const int value = original(tick);
    int applied = value;
    if (scope.tick == tick && scope.contextBaseLimit)
        applied = scope.contextBaseLimit * countMultiplier;
    else if (value > 0 && value <= 1024 && IsRingTick(ReadRecord(tick)))
        applied = value * countMultiplier;
    if (scope.capture && scope.tick == tick) {
        scope.nativeLimit = value; scope.appliedLimit = applied; ++scope.limitReads;
    }
    return applied;
}

inline float __fastcall RadiusDetour(Address tick, float time) {
    const auto original = PLH::FnCast(radiusTrampoline, static_cast<RadiusFn>(nullptr));
    const float value = original(tick, time);
    // Match the ring's existing 2x radius for ordinary ticks selected under a
    // Bigger attack or a validated ring+mask cast. Bigger ticks stay unchanged.
    const float applied = scope.tick == tick && scope.contextBaseLimit &&
        std::isfinite(value) && value > 0.0f ? value * 2.0f : value;
    if (scope.capture && scope.tick == tick) scope.radius = applied;
    return applied;
}

inline int __fastcall ProcessDetour(Address collector, Address a2, Address a3, Address a4) {
    const auto original = PLH::FnCast(processTrampoline, static_cast<ProcessFn>(nullptr));
    if (scope.capture && At<Address>(collector, 0x28).value == scope.tick) {
        ++scope.passes;
        const auto begin = At<Address>(collector, 0x10);
        const auto end = At<Address>(collector, 0x18);
        if (begin.readable && end.readable && end.value >= begin.value &&
            (end.value - begin.value) % 0x28 == 0 &&
            (end.value - begin.value) / 0x28 <= 512) {
            const auto base = reinterpret_cast<Address>(GetModuleHandleW(nullptr));
            using Check = bool(__fastcall*)(Address);
            const auto isDead = reinterpret_cast<Check>(base + 0x2FBB54);
            const auto isBlocked = reinterpret_cast<Check>(base + 0x190EFC);
            for (Address item = begin.value; item < end.value; item += 0x28) {
                ++scope.candidates;
                const auto stats = At<Address>(item, 0x20);
                if (!stats.readable || !stats.value) { ++scope.unreadable; continue; }
                if (!isDead(stats.value)) continue;
                ++scope.dead;
                const auto object = At<Address>(stats.value, 0x38);
                if (!object.readable) { ++scope.unreadable; continue; }
                if (!object.value) { ++scope.missingObject; continue; }
                if (isBlocked(object.value)) ++scope.blocked;
                else ++scope.preDispatch;
            }
        } else ++scope.unreadable;
    }
    return original(collector, a2, a3, a4);
}

inline void __fastcall TickDetour(Address aoe, Address tick, Address position, Address context) {
    const auto original = PLH::FnCast(tickTrampoline, static_cast<TickFn>(nullptr));
    const auto previous = scope;
    scope = {tick, CasterBaseLimit(aoe, tick)};
    scope.casterStats = At<Address>(aoe, 0x38).value;
    const auto name = ReadRecord(tick);
    if (diagnosticsEnabled && name.nameComplete &&
        std::strncmp(name.name.data(), "NecroAoE_Raise", 13) == 0)
        scope.capture = logTicks.fetch_add(1) < 64;
    Address root = 0; bool ring = false;
    Record rootName;
    if (scope.capture) {
        const auto stats = At<Address>(aoe, 0x38);
        root = At<Address>(stats.value, 0xE0).value;
        rootName = ReadRecord(root);
        ring = HasRingBonus(stats.value);
    }
    original(aoe, tick, position, context);
    if (scope.capture)
        Log("[RaiseAudit] tick=%s active=%s ring=%u contextBase=%d limit=%d->%d reads=%u "
            "radius=%.1f passes=%u candidates=%u dead=%u missingObject=%u blocked=%u "
            "preDispatch=%u unreadable=%u",
            name.name.data(), rootName.nameComplete ? rootName.name.data() : "<unavailable>",
            static_cast<unsigned>(ring), scope.contextBaseLimit, scope.nativeLimit,
            scope.appliedLimit, scope.limitReads, scope.radius, scope.passes,
            scope.candidates, scope.dead, scope.missingObject, scope.blocked,
            scope.preDispatch, scope.unreadable);
    scope = previous;
}

// Only remove once all game threads are outside these detours.
inline bool Remove() {
    bool ok = true;
    if (processHook) { if (processHook->unHook()) processHook.reset(); else ok = false; }
    if (tickHook) { if (tickHook->unHook()) tickHook.reset(); else ok = false; }
    if (radiusHook) { if (radiusHook->unHook()) radiusHook.reset(); else ok = false; }
    if (limitHook) { if (limitHook->unHook()) limitHook.reset(); else ok = false; }
    return ok;
}

inline bool Install(int multiplier = 4, bool boostMaskCasts = true) {
    if (limitHook || radiusHook || tickHook || processHook || multiplier < 1 || multiplier > 16)
        return false;
    countMultiplier = multiplier;
    includeMask = boostMaskCasts;
    const auto base = reinterpret_cast<Address>(GetModuleHandleW(nullptr));
    limitHook = std::make_unique<PLH::x64Detour>(base + 0x19D0A4,
        reinterpret_cast<std::uint64_t>(&LimitDetour), &limitTrampoline);
    if (!limitHook->hook()) { Log("[RaiseAuditInstall] limit FAILED"); limitHook.reset(); return false; }
    // Tick scope also identifies the player for resurrection-weapon inheritance.
    // Ordinary fallback ticks need this even when mask integration is disabled.
    radiusHook = std::make_unique<PLH::x64Detour>(base + 0x4EAB70,
        reinterpret_cast<std::uint64_t>(&RadiusDetour), &radiusTrampoline);
    if (!radiusHook->hook()) { Log("[RaiseAuditInstall] radius FAILED"); radiusHook.reset(); Remove(); return false; }
    tickHook = std::make_unique<PLH::x64Detour>(base + 0x4EA0A4,
        reinterpret_cast<std::uint64_t>(&TickDetour), &tickTrampoline);
    if (!tickHook->hook()) { Log("[RaiseAuditInstall] tick FAILED"); tickHook.reset(); Remove(); return false; }
    if (diagnosticsEnabled) {
        processHook = std::make_unique<PLH::x64Detour>(base + 0x2E6534,
            reinterpret_cast<std::uint64_t>(&ProcessDetour), &processTrampoline);
        if (!processHook->hook()) { Log("[RaiseAuditInstall] collector FAILED"); processHook.reset(); Remove(); return false; }
    }
    Log("[RaiseAuditInstall] v2 OK multiplier=%d mask=%u; capture limited to 64 resurrection ticks",
        multiplier, static_cast<unsigned>(includeMask));
    return true;
}
} // namespace sow::ring_boost
