#pragma once
// Doubles mask-wight summon counts. Kept under the original filename/namespace
// for compatibility. Pair with sow_raise_dead_bridge.hpp for resurrection.
// Include once, from hooks.cpp. Requires sow_entities.hpp and PolyHook2.
#include "sow_entities.hpp"
#include <polyhook2/Detour/x64Detour.hpp>
#include <polyhook2/Misc.hpp>
#include <intrin.h>
#include <memory>
#include <limits>

namespace sow::summon_probe {
inline std::unique_ptr<PLH::x64Detour> actionHook, requestHook, countHook;
inline std::uint64_t actionTrampoline = 0, requestTrampoline = 0, countTrampoline = 0;
inline thread_local Address currentAction = 0;
inline thread_local bool boostCurrentRequest = false;
inline bool doubleWightsEnabled = false; // Configure only while hooks are inactive.

using ActionFn = void(__fastcall*)(Address action, Address stats);
using RequestFn = void(__fastcall*)(Address summon, Address overrideRecord,
    Address stats, Address actor, Address variables);
using CountFormulaFn = float(__fastcall*)(Address variables, Address formula, Address other);
using RoundCountFn = int(__fastcall*)(float value);

inline bool IsWightRequest(Address action, Address summon, Address stats, Address actor) {
    const auto actionName = ReadRecord(action);
    if (!actionName.Named("PC_Summon_Gondorians_Undead_NecroAoE_Lvl1") &&
        !actionName.Named("PC_Summon_Gondorians_Undead_NecroAoE_Lvl2")) return false;
    const auto player = At<Address>(stats, offset::stats::player);
    const auto sourceActor = At<Address>(stats, offset::stats::actor);
    if (!actor || !player.readable || player.value != actor ||
        !sourceActor.readable || sourceActor.value != actor) return false;
    const auto record = ReadRecord(summon);
    return record.Named("Gondorians_Undead_NecroAoE_1") ||
        record.Named("Gondorians_Undead_NecroAoE_2") ||
        record.Named("Gondorians_Undead_NecroAoE_3") ||
        record.Named("Gondorians_Undead_NecroAoE_4") ||
        record.Named("Gondorians_Undead_NecroAoE_5") ||
        record.Named("Gondorians_Undead_NecroAoE_6");
}

inline float __fastcall CountDetour(Address variables, Address formula, Address other) {
    const auto caller = reinterpret_cast<Address>(_ReturnAddress());
    auto original = PLH::FnCast(countTrampoline, static_cast<CountFormulaFn>(nullptr));
    const float value = original(variables, formula, other);
    if (!boostCurrentRequest) return value;
    const auto base = reinterpret_cast<Address>(GetModuleHandleW(nullptr));
    // Only the formula evaluation feeding the spawn loop in 0x140572740.
    if (caller != base + 0x5727B2 || !std::isfinite(value) ||
        value <= 0.0f || value > 1048576.0f) return value;
    // Call the unhooked native rounding helper so fractional counts retain
    // the game's rounding semantics. The caller rounds this integer again.
    const auto round = reinterpret_cast<RoundCountFn>(base + 0x114A138);
    const int count = round(value);
    if (count <= 0) return value;
    return static_cast<float>(count * 2);
}

inline void __fastcall ActionDetour(Address action, Address stats) {
    auto original = PLH::FnCast(actionTrampoline, static_cast<ActionFn>(nullptr));
    const auto previous = currentAction;
    currentAction = action;
    original(action, stats);
    currentAction = previous;
}

inline void __fastcall RequestDetour(Address summon, Address overrideRecord,
    Address stats, Address actor, Address variables) {
    auto original = PLH::FnCast(requestTrampoline, static_cast<RequestFn>(nullptr));
    const bool previous = boostCurrentRequest;
    boostCurrentRequest = doubleWightsEnabled &&
        IsWightRequest(currentAction, summon, stats, actor);
    original(summon, overrideRecord, stats, actor, variables);
    boostCurrentRequest = previous;
}

inline bool Remove();

inline bool Install(bool doubleWights = false) {
    if (actionHook || requestHook || countHook) return false;
    const auto base = reinterpret_cast<Address>(GetModuleHandleW(nullptr));
    doubleWightsEnabled = doubleWights;
    if (doubleWights) {
        countHook = std::make_unique<PLH::x64Detour>(base + 0x30ECD8,
            reinterpret_cast<std::uint64_t>(&CountDetour), &countTrampoline);
        if (!countHook->hook()) {
            countHook.reset(); Remove(); return false;
        }
    }
    actionHook = std::make_unique<PLH::x64Detour>(base + 0xF15EE0,
        reinterpret_cast<std::uint64_t>(&ActionDetour), &actionTrampoline);
    requestHook = std::make_unique<PLH::x64Detour>(base + 0x572740,
        reinterpret_cast<std::uint64_t>(&RequestDetour), &requestTrampoline);
    if (!actionHook->hook()) {
        actionHook.reset(); requestHook.reset(); Remove(); return false;
    }
    if (!requestHook->hook()) {
        // Retain the active detour if rollback fails.
        if (actionHook->unHook()) actionHook.reset();
        requestHook.reset();
        if (!actionHook) Remove();
        return false;
    }
    return true;
}

// Call only after ensuring no game thread is executing these detours.
inline bool Remove() {
    bool ok = true;
    if (requestHook) {
        if (requestHook->unHook()) requestHook.reset(); else ok = false;
    }
    if (actionHook) {
        if (actionHook->unHook()) actionHook.reset(); else ok = false;
    }
    if (countHook) {
        if (countHook->unHook()) countHook.reset(); else ok = false;
    }

    return ok;
}
} // namespace sow::summon_probe
