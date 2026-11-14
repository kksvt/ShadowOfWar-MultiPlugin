#pragma once
#include "sow_ring_might.hpp"
#include <intrin.h>
#include <atomic>
#include <mutex>
#include <cstdio>

namespace sow::grunt_aggression {
using AddFn = int(__fastcall*)(Address, Address, int, Address, bool, bool, Address, bool);
using UpdateFn = void(__fastcall*)(Address, Address, Address);
using TimerFn = void(__fastcall*)(Address, float);
inline std::unique_ptr<PLH::x64Detour> addHook, updateHook, timerHook;
inline std::uint64_t addTrampoline = 0, updateTrampoline = 0, timerTrampoline = 0;
inline float delayScale = 0.4f;
inline bool running = true, diagnostics = true;
inline thread_local Address gruntTimer = 0;
inline std::atomic<unsigned> logCount{0};
inline std::mutex logMutex;

inline void Log(const char* event, float before = 0, float after = 0) {
    if (!diagnostics || logCount.fetch_add(1) >= 64) return;
    char path[MAX_PATH]{};
    const auto length = GetTempPathA(MAX_PATH, path);
    constexpr char name[] = "SoW-GruntAggression.log";
    if (!length || length + sizeof(name) > MAX_PATH) return;
    std::memcpy(path + length, name, sizeof(name));
    std::lock_guard<std::mutex> lock(logMutex);
    FILE* file = nullptr;
    if (fopen_s(&file, path, "a") == 0 && file) {
        std::fprintf(file, "[GruntAggression] %s before=%.3f after=%.3f\n", event, before, after);
        std::fclose(file);
    }
}

// Stats pointers in combat-stage roles are the acting participants, not the
// fight-circle target. Verify the actor back-reference and exclude roster members.
inline bool OrcGrunt(Address stats) {
    const auto actor = At<Address>(stats, 0x28);
    const auto marker = At<Address>(stats, 0x40);
    if (!actor.readable || !actor.value || !marker.readable || marker.value) return false;
    const auto back = At<Address>(actor.value, 0x26A8);
    const auto index = At<std::int32_t>(actor.value, 0x2710);
    if (!back.readable || back.value != stats || !index.readable || index.value != -1) return false;
    const auto type = At<Address>(stats, 0x60);
    const auto flags = At<std::uint8_t>(type.value, 1);
    const auto name = At<Address>(type.value, 0x20);
    const auto prefix = Read<std::array<char, 4>>(name.value);
    return type.readable && flags.readable && (flags.value & 2) && name.readable &&
        prefix.readable && std::memcmp(prefix.value.data(), "Orc_", 4) == 0;
}

// Fail closed on mixed groups, malformed lists, or missing role owners.
inline bool GruntPlay(Address group) {
    const Address sentinel = group + 8;
    auto node = At<Address>(group, 0x10);
    bool found = false;
    for (unsigned stages = 0; node.readable && node.value != sentinel && stages < 16; ++stages) {
        if (!node.value) return false;
        const auto count = At<unsigned>(node.value, 0x10);
        if (!count.readable || count.value == 0 || count.value > 16) return false;
        for (unsigned i = 0; i < count.value; ++i) {
            // Stage entries begin +0x18, stride 0x90; first weakref's live pointer +0x20.
            const auto stats = At<Address>(node.value, 0x38 + Address(i) * 0x90);
            if (!stats.readable || !OrcGrunt(stats.value)) return false;
            found = true;
        }
        node = At<Address>(node.value, 8);
    }
    return found && node.readable && node.value == sentinel;
}

inline float AdjustDelay(float seconds, bool eligible, float scale) noexcept {
    return eligible && std::isfinite(seconds) && seconds > 0 ? seconds * scale : seconds;
}

inline void __fastcall UpdateDetour(Address group, Address circle, Address context) {
    const Address previous = gruntTimer;
    gruntTimer = GruntPlay(group) ? group : 0;
    PLH::FnCast(updateTrampoline, static_cast<UpdateFn>(nullptr))(group, circle, context);
    gruntTimer = previous;
}

inline void __fastcall TimerDetour(Address timer, float seconds) {
    // Only the post-play cooldown call, never other game timers or nested groups.
    const bool eligible = timer == gruntTimer && gruntTimer &&
        reinterpret_cast<Address>(_ReturnAddress()) == ring_might::Base() + 0x377B1C;
    const float adjusted = AdjustDelay(seconds, eligible, delayScale);
    if (adjusted != seconds && eligible) Log("post-play cooldown", seconds, adjusted);
    PLH::FnCast(timerTrampoline, static_cast<TimerFn>(nullptr))(timer, adjusted);
}

inline int __fastcall AddDetour(Address inventory, Address item, int count, Address fourth,
    bool fifth, bool sixth, Address seventh, bool eighth) {
    const auto hash = running ? At<std::uint32_t>(item, 4) : Field<std::uint32_t>{};
    if (hash.readable && hash.value == 0x47EBA329 &&
        ReadRecord(item).Named("I_Am_Undead_WalkOnly")) {
        const auto stats = At<Address>(inventory, 0x440);
        if (stats.readable && OrcGrunt(stats.value)) {
            const auto fearless = ring_might::Lookup(0x2700530, "I_Am_Undead_Fearless");
            if (fearless && ReadRecord(fearless).Named("I_Am_Undead_Fearless")) {
                item = fearless;
                Log("walk-only item redirected to Fearless");
            } else Log("Fearless lookup failed; unchanged");
        } else Log("walk-only owner not a verified Orc grunt; unchanged");
    }
    return PLH::FnCast(addTrampoline, static_cast<AddFn>(nullptr))(
        inventory, item, count, fourth, fifth, sixth, seventh, eighth);
}

inline bool Remove() {
    bool ok = true;
    for (auto* hook : {&updateHook, &timerHook, &addHook}) {
        if (*hook && !(*hook)->unHook()) { ok = false; continue; }
        hook->reset();
    }
    return ok;
}

inline bool Install(float scale = 0.4f, bool allowUndeadRunning = true, bool log = true) {
    if (addHook || updateHook || timerHook || !std::isfinite(scale) || scale <= 0 || scale > 1)
        return false;
    delayScale = scale; running = allowUndeadRunning; diagnostics = log; logCount = 0;
    const auto base = ring_might::Base();
    addHook = std::make_unique<PLH::x64Detour>(base + 0x1C1224,
        reinterpret_cast<Address>(&AddDetour), &addTrampoline);
    timerHook = std::make_unique<PLH::x64Detour>(base + 0x728980,
        reinterpret_cast<Address>(&TimerDetour), &timerTrampoline);
    updateHook = std::make_unique<PLH::x64Detour>(base + 0x3778D4,
        reinterpret_cast<Address>(&UpdateDetour), &updateTrampoline);
    if (!addHook->hook()) {
        addHook.reset(); timerHook.reset(); updateHook.reset();
        Log("inventory hook FAILED"); return false;
    }
    if (!timerHook->hook()) {
        timerHook.reset(); updateHook.reset(); Remove();
        Log("timer hook FAILED"); return false;
    }
    if (!updateHook->hook()) {
        updateHook.reset(); Remove(); Log("play-update hook FAILED"); return false;
    }
    Log("installed; all allied/enemy Orc grunts; newly assigned walk-only items", 1, scale);
    return true;
}
} // namespace sow::grunt_aggression
