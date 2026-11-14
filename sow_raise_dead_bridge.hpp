#pragma once
// Experimental additive Raise Dead bridge. Include once in hooks.cpp.
// Optionally retains PC_Kill_OldSummonedGhosts; adds the normal resurrection AOE.
// No record writes, pool scans, recursive dumps, or second skill activation.
#include "sow_entities.hpp"
#include <polyhook2/Detour/x64Detour.hpp>
#include <polyhook2/Misc.hpp>
#include <memory>
#include <mutex>
#include <atomic>

namespace sow::raise_dead_bridge {
using Getter = Address(__fastcall*)(Address);
// 0x14015BD0C: optional context, caster stats, AOE record, flag.
using CreateAOE = Address(__fastcall*)(Address, Address, Address, bool);
inline std::unique_ptr<PLH::x64Detour> coreHook, creationHook;
inline std::uint64_t coreTrampoline = 0, creationTrampoline = 0;
inline std::atomic<bool> enabled{false}, installing{false};
inline std::atomic<bool> cleanupOldGhosts{true};
inline thread_local bool reading = false, adding = false;

struct Binding {
    Address normalRoot = 0, normalCore = 0, normalAOE = 0;
    Address ghostRoot = 0, ghostCore = 0, ghostAOE = 0;
};
// Menu-time record access may occur on a different thread than gameplay.
inline Binding bindings[2];
inline std::mutex bindingMutex;

inline bool IsNormal(const Record& r, unsigned level) {
    return r.Named(level == 0 ? "NecroAoE_Attack_Lvl1" : "NecroAoE_Attack_Lvl2");
}
inline bool IsGhost(const Record& r, unsigned level) {
    return r.Named(level == 0 ? "NecroAoE_Attack_Lvl1_Ghosts" : "NecroAoE_Attack_Lvl2_Ghosts");
}

inline Address ReadAOE(Address core) {
    const auto base = reinterpret_cast<Address>(GetModuleHandleW(nullptr));
    return core ? reinterpret_cast<Getter>(base + 0x56FE88)(core) : 0;
}


inline Address __fastcall CoreDetour(Address root) {
    const auto original = PLH::FnCast(coreTrampoline, static_cast<Getter>(nullptr));
    const auto core = original(root);
    if (!root || !core || reading || installing.load()) return core;
    reading = true;
    const auto record = ReadRecord(root);
    for (unsigned i = 0; i < 2; ++i) {
        const bool normal = IsNormal(record, i);
        const bool ghost = IsGhost(record, i);
        if (!normal && !ghost) continue;
        const auto aoe = ReadAOE(core);
        {
            std::lock_guard<std::mutex> lock(bindingMutex);
            auto& b = bindings[i];
            if (normal) {
                b.normalRoot = root; b.normalCore = core; b.normalAOE = aoe;
            } else {
                b.ghostRoot = root; b.ghostCore = core; b.ghostAOE = aoe;
            }
        }

        break;
    }
    reading = false;
    return core;
}

inline Address FindResurrectionAOE(Address stats, Address cleanup) {
    if (!stats || !cleanup) return 0;
    if (!ReadRecord(cleanup).Named("PC_Kill_OldSummonedGhosts")) return 0;
    const auto actor = At<Address>(stats, offset::stats::actor);
    const auto player = At<Address>(stats, offset::stats::player);
    if (!actor.readable || !actor.value || !player.readable || player.value != actor.value)
        return 0;
    const auto active = At<Address>(stats, 0xE0);
    const auto activeName = ReadRecord(active.value);
    for (unsigned i = 0; i < 2; ++i) {
        if (!active.readable || !IsGhost(activeName, i)) continue;
        Binding b;
        {
            std::lock_guard<std::mutex> lock(bindingMutex);
            b = bindings[i];
        }
        if (!b.normalRoot || !b.normalCore || !b.normalAOE ||
            b.ghostRoot != active.value || b.ghostAOE != cleanup) return 0;
        const auto normalName = ReadRecord(b.normalRoot);
        const auto normalOwner = At<Address>(b.normalRoot, 0x18);
        const auto ghostOwner = At<Address>(active.value, 0x18);
        if (!IsNormal(normalName, i) || !normalOwner.readable || !ghostOwner.readable ||
            !normalOwner.value || normalOwner.value != ghostOwner.value) return 0;
        const auto originalCore = PLH::FnCast(coreTrampoline, static_cast<Getter>(nullptr));
        const auto currentCore = originalCore(b.normalRoot);
        if (!currentCore || currentCore != b.normalCore || ReadAOE(currentCore) != b.normalAOE)
            return 0;
        // Both observed normal variants use this resurrection area effect.
        if (!ReadRecord(b.normalAOE).Named("PC_NecroAoE_Lvl1")) return 0;
        return b.normalAOE;
    }
    return 0;
}

inline Address __fastcall CreationDetour(Address context, Address stats, Address aoe, bool flag) {
    const auto original = PLH::FnCast(creationTrampoline, static_cast<CreateAOE>(nullptr));
    if (adding || installing.load() || !enabled.load())
        return original(context, stats, aoe, flag);
    // Validate before the original can invoke callbacks.
    adding = true;
    const auto extra = FindResurrectionAOE(stats, aoe);
    // Replace only a validated player mask cleanup request. Returning the created
    // resurrection AOE keeps the native caller's normal AOE handling intact.
    // If validation fails, leave the original request unchanged.
    if (extra && !cleanupOldGhosts.load()) {
        const auto raised = original(context, stats, extra, flag);
        adding = false;
        return raised;
    }
    const auto result = original(context, stats, aoe, flag);
    if (result && extra) {
        original(context, stats, extra, flag);
    }
    adding = false;
    return result; // Preserve the original cleanup's return value.
}

// Call only when no game thread is executing these detours (e.g. shutdown).
inline bool Remove() {
    bool ok = true;
    if (creationHook) {
        if (creationHook->unHook()) creationHook.reset(); else ok = false;
    }
    if (coreHook) { if (coreHook->unHook()) coreHook.reset(); else ok = false; }
    return ok;
}

// restoreResurrection=false observes only. cleanOldGhosts controls mask recast
// cleanup when the resurrection binding validates. Existing Install(true) calls
// retain their previous behavior. This does not disable other despawn rules.
inline bool Install(bool restoreResurrection = false, bool cleanOldGhosts = true) {
    if (coreHook || creationHook) return false;
    const auto base = reinterpret_cast<Address>(GetModuleHandleW(nullptr));
    enabled.store(restoreResurrection);
    cleanupOldGhosts.store(cleanOldGhosts);
    installing.store(true);
    coreHook = std::make_unique<PLH::x64Detour>(base + 0x30E758,
        reinterpret_cast<std::uint64_t>(&CoreDetour), &coreTrampoline);
    creationHook = std::make_unique<PLH::x64Detour>(base + 0x15BD0C,
        reinterpret_cast<std::uint64_t>(&CreationDetour), &creationTrampoline);
    if (!coreHook->hook()) {
        coreHook.reset(); creationHook.reset(); installing.store(false); return false;
    }
    if (!creationHook->hook()) {
        creationHook.reset(); Remove(); installing.store(false); return false;
    }
    installing.store(false);
    return true;
}
} // namespace sow::raise_dead_bridge
