#pragma once
// Observation only: preserve native command results and do not register buddies.
#include "sow_grunt_aggression.hpp"
#include <cstdarg>

namespace sow::grunt_command_probe {
using RegisterFn = void(__fastcall*)(Address, Address, Address);
using DispatchFn = void(__fastcall*)(Address, Address, Address, Address);
using AvailableFn = bool(__fastcall*)(Address, Address, Address);
using BuffFn = void(__fastcall*)(Address, Address, Address);
inline std::unique_ptr<PLH::x64Detour> registerHook, dispatchHook, availableHook, buffHook;
inline std::uint64_t registerTrampoline = 0, dispatchTrampoline = 0;
inline std::uint64_t availableTrampoline = 0, buffTrampoline = 0;
inline std::mutex logMutex, seenMutex;
inline std::array<std::atomic<unsigned>, 4> captures{};
inline std::array<std::pair<Address, Address>, 48> seen{};

inline void Log(const char* format, ...) {
    char message[8192]{};
    va_list args; va_start(args, format);
    vsnprintf(message, sizeof(message), format, args); va_end(args);
    char path[MAX_PATH]{};
    const auto length = GetTempPathA(MAX_PATH, path);
    constexpr char name[] = "SoW-GruntCommand.log";
    if (!length || length + sizeof(name) > MAX_PATH) return;
    std::memcpy(path + length, name, sizeof(name));
    std::lock_guard<std::mutex> lock(logMutex);
    FILE* file = nullptr;
    if (fopen_s(&file, path, "a") == 0 && file) {
        std::fprintf(file, "[GruntCommand] %s\n", message); std::fclose(file);
    }
}
inline bool Take(unsigned channel, unsigned limit) {
    // Saturating counters: no overflow in an indefinitely running query hook.
    auto count = captures[channel].load();
    while (count < limit) if (captures[channel].compare_exchange_weak(count, count + 1)) return true;
    return false;
}
struct Range { Address begin = 0, end = 0; unsigned count = 0; bool valid = false; };
inline Range Registry(Address manager) {
    const auto begin = At<Address>(manager, 0), end = At<Address>(manager, 8);
    if (!begin.readable || !end.readable || end.value < begin.value ||
        (!begin.value && end.value) || (end.value - begin.value) % 0x90 ||
        end.value - begin.value > 512 * 0x90) return {};
    return {begin.value, end.value, unsigned((end.value - begin.value) / 0x90), true};
}
inline Address Manager() { return At<Address>(ring_might::Base(), 0x26FFC30).value; }
inline Address FindEntry(Address actor) {
    const auto list = Registry(Manager());
    if (!list.valid || !actor) return 0;
    for (unsigned i = 0; i < list.count; ++i) {
        const auto entry = list.begin + Address(i) * 0x90;
        if (At<Address>(entry, 0).value == actor) return entry;
    }
    return 0;
}
inline Record Type(Address actor) {
    return ReadRecord(At<Address>(At<Address>(actor, 0x26A8).value, 0x60).value);
}
inline bool PlayerAlly(Address actor) {
    // Mirrors native IsPlayerAlly(stats) at RVA 366B88 for actor-backed stats.
    const auto a = At<std::uint8_t>(actor, 0x2700);
    const auto b = At<std::uint8_t>(actor, 0x23B8);
    const auto c = At<std::uint8_t>(actor, 0x2C04);
    return a.readable && b.readable && c.readable && (a.value || b.value || c.value);
}
inline void Dump(const char* phase) {
    const auto list = Registry(Manager());
    Log("%s registryValid=%d entries=%u (showing at most 12)", phase, list.valid, list.count);
    for (unsigned i = 0; list.valid && i < list.count && i < 12; ++i) {
        const auto entry = list.begin + Address(i) * 0x90;
        const auto actor = At<Address>(entry, 0).value;
        const auto type = Type(actor);
        const auto setup = ReadRecord(At<Address>(entry, 8).value);
        const auto command = ReadRecord(At<Address>(entry, 0x60).value);
        const auto index = At<std::int32_t>(actor, 0x2710);
        Log("  buddy[%u] actor=%p type=%s index=%d indexValid=%d ally=%d setup=%s command=%s state=%u",
            i, reinterpret_cast<void*>(actor), type.nameComplete ? type.name.data() : "<unavailable>",
            index.value, index.readable, PlayerAlly(actor),
            setup.nameComplete ? setup.name.data() : "<unavailable>",
            command.nameComplete ? command.name.data() : "<unavailable>",
            unsigned(At<std::uint8_t>(entry, 0x80).value));
    }
}
inline void __fastcall RegisterDetour(Address first, Address actor, Address setup) {
    PLH::FnCast(registerTrampoline, static_cast<RegisterFn>(nullptr))(first, actor, setup);
    if (!Take(0, 48)) return;
    const auto type = Type(actor), record = ReadRecord(setup);
    Log("registered actor=%p type=%s setup=%s ally=%d entry=%p",
        reinterpret_cast<void*>(actor), type.nameComplete ? type.name.data() : "<unavailable>",
        record.nameComplete ? record.name.data() : "<unavailable>", PlayerAlly(actor),
        reinterpret_cast<void*>(FindEntry(actor)));
}
inline void __fastcall DispatchDetour(Address first, Address command, Address target, Address marker) {
    const bool capture = Take(3, 32);
    if (capture) {
        const auto record = ReadRecord(command), type = Type(At<Address>(target, 0x28).value);
        Log("DISPATCH command=%s targetStats=%p targetType=%s",
            record.nameComplete ? record.name.data() : "<unavailable>",
            reinterpret_cast<void*>(target), type.nameComplete ? type.name.data() : "<unavailable>");
        Dump("before command");
    }
    PLH::FnCast(dispatchTrampoline, static_cast<DispatchFn>(nullptr))(first, command, target, marker);
    if (capture) Dump("after command");
}
inline bool __fastcall AvailableDetour(Address first, Address command, Address target) {
    const bool result = PLH::FnCast(availableTrampoline, static_cast<AvailableFn>(nullptr))(
        first, command, target);
    if (captures[2].load() >= 64) return result;
    struct Previous { Address command = 0, target = 0; bool result = false; ULONGLONG time = 0; };
    static thread_local Previous previous;
    const auto now = GetTickCount64();
    if (previous.command == command && previous.target == target && previous.result == result &&
        now - previous.time < 10000) return result;
    if (now - previous.time < 250) return result;
    const auto record = ReadRecord(command);
    if (!record.Named("Default") || !Take(2, 64)) return result;
    previous = {command, target, result, now};
    const auto actor = At<Address>(target, 0x28).value;
    const auto type = Type(actor);
    const auto list = Registry(Manager());
    Log("availability result=%d command=Default targetActor=%p targetType=%s buddies=%u registryValid=%d",
        result, reinterpret_cast<void*>(actor), type.nameComplete ? type.name.data() : "<unavailable>",
        list.count, list.valid);
    return result;
}
inline void __fastcall BuffDetour(Address buff, Address stats, Address context) {
    PLH::FnCast(buffTrampoline, static_cast<BuffFn>(nullptr))(buff, stats, context);
    if (captures[1].load() >= 48 || !grunt_aggression::OrcGrunt(stats)) return;
    const auto actor = At<Address>(stats, 0x28).value;
    if (!PlayerAlly(actor)) return;
    {
        std::lock_guard<std::mutex> lock(seenMutex);
        for (const auto& entry : seen) if (entry.first == actor && entry.second == stats) return;
        bool stored = false;
        for (auto& entry : seen) if (!entry.first) { entry = {actor, stats}; stored = true; break; }
        if (!stored) return;
    }
    if (!Take(1, 48)) return;
    const auto type = Type(actor), record = ReadRecord(buff);
    Log("friendly grunt observed actor=%p stats=%p type=%s buff=%s buddyEntry=%p",
        reinterpret_cast<void*>(actor), reinterpret_cast<void*>(stats),
        type.nameComplete ? type.name.data() : "<unavailable>",
        record.nameComplete ? record.name.data() : "<unavailable>",
        reinterpret_cast<void*>(FindEntry(actor)));
}
inline bool Remove() {
    bool ok = true;
    for (auto* hook : {&buffHook, &availableHook, &dispatchHook, &registerHook}) {
        if (*hook && !(*hook)->unHook()) { ok = false; continue; }
        hook->reset();
    }
    return ok;
}
inline bool Install() {
    if (registerHook || dispatchHook || availableHook || buffHook) return false;
    for (auto& count : captures) count = 0;
    seen = {};
    const auto base = ring_might::Base();
    registerHook = std::make_unique<PLH::x64Detour>(base + 0x1AA3784,
        reinterpret_cast<Address>(&RegisterDetour), &registerTrampoline);
    dispatchHook = std::make_unique<PLH::x64Detour>(base + 0x1AA45B4,
        reinterpret_cast<Address>(&DispatchDetour), &dispatchTrampoline);
    availableHook = std::make_unique<PLH::x64Detour>(base + 0x1AA4534,
        reinterpret_cast<Address>(&AvailableDetour), &availableTrampoline);
    buffHook = std::make_unique<PLH::x64Detour>(base + 0x452ED8,
        reinterpret_cast<Address>(&BuffDetour), &buffTrampoline);
    for (auto* hook : {&registerHook, &dispatchHook, &availableHook, &buffHook}) {
        if (!(*hook)->hook()) {
            // Discard this and later, never-installed detours; retain earlier hooks for cleanup.
            bool discard = false;
            for (auto* pendingHook : {&registerHook, &dispatchHook, &availableHook, &buffHook}) {
                if (pendingHook == hook) discard = true;
                if (discard) pendingHook->reset();
            }
            Remove(); Log("installation FAILED"); return false;
        }
    }
    Log("probe installed; observation only; native AI and command results retained");
    return true;
}
} // namespace sow::grunt_command_probe
