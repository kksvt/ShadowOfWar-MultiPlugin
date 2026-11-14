#pragma once

// Shadow of War: entity snapshots for the build inspected in IDA.
// MSVC x64. No hooks, patches, ownership, or writes to game memory.
// Offsets are build-specific. These are snapshots, not engine ABI structs.
#include <Windows.h>
#include <cstdint>
#include <cstddef>
#include <array>
#include <cmath>
#include <cstring>

namespace sow {
using Address = std::uintptr_t;
static_assert(sizeof(Address) == 8, "Requires a 64-bit build");

namespace offset {
inline constexpr Address calculateDamageRva = 0x3643E8;
inline constexpr Address actorToMemberRva = 0x18D920;
inline constexpr Address globalsRva = 0x26FFAA8;
inline constexpr Address rosterManager = 0x6C68;
inline constexpr Address managerOwner = 0xA8;
inline constexpr Address keyOwner = 0x18;
namespace context {
inline constexpr Address source = 0x18, target = 0x20, definition = 0x48;
}
namespace stats {
inline constexpr Address actor = 0x28, player = 0x40, characterType = 0x60;
inline constexpr Address maxHealth = 0x428, health = 0x42C;
inline constexpr Address healthFraction = 0x430;
inline constexpr Address tagsBegin = 0x5A8, tagsEnd = 0x5B0;
}
namespace actor {
inline constexpr Address rosterKey = 0x2708, rosterIndex = 0x2710;
}
namespace member {
inline constexpr Address role = 0x318, level = 0x4B8;
}
}

template<class T> struct Field {
    T value{};
    bool readable = false;
};

template<class T> Field<T> Read(Address address) noexcept {
    Field<T> result;
    SIZE_T bytes = 0;
    if (address)
        result.readable = ReadProcessMemory(GetCurrentProcess(),
            reinterpret_cast<const void*>(address), &result.value,
            sizeof(T), &bytes) && bytes == sizeof(T);
    if (!result.readable) result.value = {};
    return result;
}

template<class T> Field<T> At(Address base, Address displacement) noexcept {
    return base ? Read<T>(base + displacement) : Field<T>{};
}

struct Record {
    Address address = 0;
    std::array<char, 192> name{};
    bool nameComplete = false;

    bool Named(const char* expected) const noexcept {
        return nameComplete && std::strcmp(name.data(), expected) == 0;
    }
};

inline Record ReadRecord(Address address) noexcept {
    Record result;
    result.address = address;
    const auto flags = At<std::uint8_t>(address, 1);
    const auto text = At<Address>(address, 0x20);
    // Mirrors native record-name helper 0x14018BDB0.
    if (!flags.readable || !(flags.value & 2) ||
        !text.readable || !text.value) return result;
    for (std::size_t i = 0; i + 1 < result.name.size(); ++i) {
        const auto ch = Read<char>(text.value + i);
        if (!ch.readable) return result;
        result.name[i] = ch.value;
        if (!ch.value) { result.nameComplete = true; return result; }
    }
    return result; // Truncated names never pass Named().
}

enum class MemberStatus {
    NoStats, NoActor, Unreadable, Unregistered, NoManager,
    NoKey, WrongOwner, NativeUnavailable, NativeFault, NotFound, Resolved
};

// The resolver can assert on invalid/stale keys. Owner validation below
// reduces that risk; SEH cannot recover from process termination.
inline bool ResolveNative(Address function, Address actor, Address* out) noexcept {
#if defined(_MSC_VER)
    using Fn = Address(__fastcall*)(Address);
    __try {
        *out = reinterpret_cast<Fn>(function)(actor);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        *out = 0;
        return false;
    }
#else
    (void)function; (void)actor; *out = 0;
    return false;
#endif
}

struct Entity {
    Address stats = 0;
    Field<Address> actor;
    Field<Address> playerMarker; // Non-null on the observed player.
    Field<Address> typePointer;
    Record type;
    Field<float> health, maxHealth, healthFraction;
    // Situational tags, e.g. LedgeHanging; NOT a rank/type list.
    Field<Address> tagsBegin, tagsEnd;
    Field<std::int32_t> registrationIndex;
    Field<Address> rosterKey;
    MemberStatus memberStatus = MemberStatus::NoStats;
    Address member = 0;
    Field<std::uint32_t> level;
    Field<Address> rolePointer;
    Record role; // Role_Judge is an epithet/role, not a captain-rank enum.

    bool IsObservedOrcType() const noexcept {
        return type.Named("Orc_Infantry_FacA") ||
               type.Named("Orc_Defender_FacA") ||
               type.Named("Orc_Spearman_FacA") ||
               type.Named("Orc_Berserker_FacA") ||
               type.Named("Orc_Archer_FacA");
    }
    bool IsAllowedType(bool includeOlogs = false) const noexcept {
        return IsObservedOrcType() ||
               (includeOlogs && type.Named("OlogHai_Infantry"));
    }
    bool IsGruntCandidate(bool includeOlogs = false) const noexcept {
        return actor.readable && actor.value &&
            playerMarker.readable && !playerMarker.value &&
            registrationIndex.readable && registrationIndex.value == -1 &&
            memberStatus == MemberStatus::Unregistered &&
            IsAllowedType(includeOlogs);
    }
    bool IsCaptainCandidate(bool includeOlogs = false) const noexcept {
        // Provisional roster-membership heuristic, not a proven rank test.
        return actor.readable && actor.value &&
            playerMarker.readable && !playerMarker.value &&
            registrationIndex.readable && registrationIndex.value >= 0 &&
            memberStatus == MemberStatus::Resolved && member &&
            IsAllowedType(includeOlogs);
    }
};

inline Entity ReadEntity(Address stats, Address moduleBase) noexcept {
    Entity e;
    e.stats = stats;
    if (!stats) return e;
    e.actor = At<Address>(stats, offset::stats::actor);
    e.playerMarker = At<Address>(stats, offset::stats::player);
    e.typePointer = At<Address>(stats, offset::stats::characterType);
    e.type = ReadRecord(e.typePointer.value);
    e.health = At<float>(stats, offset::stats::health);
    e.maxHealth = At<float>(stats, offset::stats::maxHealth);
    e.healthFraction = At<float>(stats, offset::stats::healthFraction);
    e.tagsBegin = At<Address>(stats, offset::stats::tagsBegin);
    e.tagsEnd = At<Address>(stats, offset::stats::tagsEnd);
    e.memberStatus = MemberStatus::Unreadable;
    if (!e.actor.readable) return e;
    if (!e.actor.value) { e.memberStatus = MemberStatus::NoActor; return e; }
    e.registrationIndex = At<std::int32_t>(e.actor.value, offset::actor::rosterIndex);
    if (!e.registrationIndex.readable) return e;
    if (e.registrationIndex.value == -1) {
        e.memberStatus = MemberStatus::Unregistered;
        return e;
    }
    if (e.registrationIndex.value < 0) return e;
    e.rosterKey = At<Address>(e.actor.value, offset::actor::rosterKey);
    if (!e.rosterKey.readable) return e;
    if (!e.rosterKey.value) { e.memberStatus = MemberStatus::NoKey; return e; }
    const auto globals = At<Address>(moduleBase, offset::globalsRva);
    const auto manager = At<Address>(globals.value, offset::rosterManager);
    if (!manager.readable || !manager.value) {
        e.memberStatus = MemberStatus::NoManager; return e;
    }
    const auto keyOwner = At<Address>(e.rosterKey.value, offset::keyOwner);
    const auto mapOwner = At<Address>(manager.value, offset::managerOwner);
    if (!keyOwner.readable || !mapOwner.readable) return e;
    if (!keyOwner.value || keyOwner.value != mapOwner.value) {
        e.memberStatus = MemberStatus::WrongOwner; return e;
    }
#if !defined(_MSC_VER)
    e.memberStatus = MemberStatus::NativeUnavailable;
    return e;
#endif
    if (!ResolveNative(moduleBase + offset::actorToMemberRva, e.actor.value, &e.member)) {
        e.memberStatus = MemberStatus::NativeFault; return e;
    }
    if (!e.member) { e.memberStatus = MemberStatus::NotFound; return e; }
    e.memberStatus = MemberStatus::Resolved;
    e.level = At<std::uint32_t>(e.member, offset::member::level);
    e.rolePointer = At<Address>(e.member, offset::member::role);
    e.role = ReadRecord(e.rolePointer.value);
    return e;
}

struct DamageContext {
    Address address = 0;
    Field<Address> sourcePointer, targetPointer, definitionPointer;
    Entity source, target;
    Record definition;

    bool ShouldBoostWight(float damage) const noexcept {
        // Require readable player-identification fields before excluding the
        // player. Unknown targets keep native damage rather than risk a boost.
        return std::isfinite(damage) && damage > 0.0f &&
            sourcePointer.readable && sourcePointer.value &&
            targetPointer.readable && targetPointer.value &&
            sourcePointer.value != targetPointer.value &&
            source.type.Named("Gondorian_Undead") &&
            target.playerMarker.readable && !target.playerMarker.value &&
            target.type.nameComplete && target.type.name[0] &&
            !target.type.Named("Player_Character") &&
            !(source.actor.readable && source.actor.value &&
              target.actor.readable && source.actor.value == target.actor.value);
    }

    bool ShouldBoost(float damage, bool includeOlogs = false) const noexcept {
        return std::isfinite(damage) && damage > 0.0f &&
            sourcePointer.readable && targetPointer.readable &&
            source.IsGruntCandidate(includeOlogs) &&
            target.IsCaptainCandidate(includeOlogs) &&
            source.actor.value != target.actor.value;
    }
};

inline DamageContext ReadDamageContext(Address context, Address moduleBase) noexcept {
    DamageContext d;
    d.address = context;
    d.sourcePointer = At<Address>(context, offset::context::source);
    d.targetPointer = At<Address>(context, offset::context::target);
    d.definitionPointer = At<Address>(context, offset::context::definition);
    d.source = ReadEntity(d.sourcePointer.value, moduleBase);
    d.target = ReadEntity(d.targetPointer.value, moduleBase);
    d.definition = ReadRecord(d.definitionPointer.value);
    return d;
}
} // namespace sow
