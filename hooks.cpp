#define _CRT_SECURE_NO_WARNINGS
#include "sow_entities.hpp"
#include "sow_summon_probe.hpp"
#include "sow_raise_dead_bridge.hpp"
#include "sow_ring_boost.hpp"
#include "sow_ring_might.hpp"
#include "sow_resurrection_weapons.hpp"
#include "sow_grunt_aggression.hpp"
#include "sow_gear_resurrection.hpp"
#include "sow_grunt_command_probe.hpp"
#include "sow_grunt_command.hpp"
#include "sow_multi_skills.hpp"
#include <polyhook2/Detour/x64Detour.hpp>
#include <polyhook2/Misc.hpp>
#include <memory>

namespace {
constexpr float gruntDamageMultiplier = 16.0f;
constexpr float wightDamageMultiplier = 16.0f;
constexpr bool includeOlogs = true;
constexpr bool doubleWights = true;
constexpr bool restoreRaiseDead = true;
constexpr bool cleanupOldGhosts = true;
constexpr int ringResurrectionMultiplier = 4;
constexpr bool ringBoostWithMask = true;
constexpr float ringMightPerAllyHit = 4.0f;
constexpr bool ringMightDiagnostics = false; // Preserve current project setting.
constexpr bool elementalResurrectedGrunts = true;
constexpr bool resurrectionWeaponDiagnostics = false; // Preserve current project setting.
constexpr bool aggressiveGrunts = true;
constexpr float gruntPostPlayDelayScale = 0.4f; // 60% shorter cooldown, all factions.
constexpr bool allowRevenantRunning = true;
constexpr bool gruntAggressionDiagnostics = false; // Preserve current project setting.
constexpr bool guaranteedArmorResurrection = true;
constexpr bool guaranteedUrfaelResurrection = true;
constexpr bool gearResurrectionDiagnostics = false; // Preserve current project setting.
constexpr bool traceGruntStealthCommand = false; // Cannot share the buff hook with the prototype.
constexpr bool enableGruntCommand = true;
constexpr bool multiEquipSkillUpgrades = true;
constexpr bool multiSkillDiagnostics = true;
constexpr int gruntCommandKey = 'X';
constexpr int gruntRallyKey = 'Z'; // Approach player, no stealth modifier required.
constexpr bool gruntCommandRequireHeldStealthKey = true;
constexpr int gruntCommandStealthKey = VK_LSHIFT; // Match your own stealth binding.
constexpr bool gruntCommandDiagnostics = true;
constexpr bool gruntCommandConsole = true;
bool enableDebugConsole = true; // Preserve the current project's console option.

using DamageFn = float(__fastcall*)(__int64, __int64, char, bool*);
std::unique_ptr<PLH::x64Detour> damageHook;
std::uint64_t damageTrampoline = 0;
thread_local bool handlingDamage = false;

float __fastcall DamageDetour(__int64 context, __int64 a2, char a3, bool* a4) {
    const auto original = PLH::FnCast(damageTrampoline, static_cast<DamageFn>(nullptr));
    if (handlingDamage) return original(context, a2, a3, a4);
    handlingDamage = true;
    const auto base = reinterpret_cast<sow::Address>(GetModuleHandleW(nullptr));
    const auto hit = sow::ReadDamageContext(static_cast<sow::Address>(context), base);
    float damage = original(context, a2, a3, a4);
    if (hit.ShouldBoostWight(damage)) damage *= wightDamageMultiplier;
    else if (hit.ShouldBoost(damage, includeOlogs)) damage *= gruntDamageMultiplier;
    sow::grunt_command::OnDamageCalculation(hit.source.actor.value, hit.target.actor.value,
        damage, hit.definitionPointer.value, a3);
    handlingDamage = false;
    return damage;
}

bool RemoveDamageHook() {
    if (!damageHook) return true;
    if (!damageHook->unHook()) return false;
    damageHook.reset();
    return true;
}
}

// Inspect in a debugger if initialization fails; no console/logging hooks.
bool g_pluginHooksInstalled = false;
bool g_pluginHooksRemoved = true;

// Existing void entry points retained for the project's dllmain.cpp.
// Install/remove only with game threads quiescent. Initialization should be
// arranged by a loader outside DllMain's loader lock; see README.md.
void InitHooks() {
    if (damageHook || sow::summon_probe::actionHook ||
        sow::summon_probe::requestHook || sow::summon_probe::countHook ||
        sow::raise_dead_bridge::coreHook || sow::raise_dead_bridge::creationHook ||
        sow::ring_boost::limitHook || sow::ring_boost::radiusHook ||
        sow::ring_boost::tickHook || sow::ring_boost::processHook ||
        sow::ring_might::applyHook || sow::resurrection_weapons::constructorHook ||
        sow::resurrection_weapons::workerHook || sow::grunt_aggression::addHook ||
        sow::grunt_aggression::updateHook || sow::grunt_aggression::timerHook ||
        sow::gear_resurrection::installed || sow::grunt_command_probe::registerHook ||
        sow::grunt_command_probe::dispatchHook || sow::grunt_command_probe::availableHook ||
        sow::grunt_command_probe::buffHook || sow::grunt_command::buffHook ||
        sow::grunt_command::outlineHook || sow::grunt_command::updateHook ||
        sow::grunt_command::cleanupHook)
        return;
    if (enableDebugConsole) {
        if (!AllocConsole()) enableDebugConsole = false;
        else {
            freopen("CONOUT$", "w", stdout);
            printf("Debug Console initialized\n");
        }
    }
    const auto base = reinterpret_cast<sow::Address>(GetModuleHandleW(nullptr));
    damageHook = std::make_unique<PLH::x64Detour>(base + sow::offset::calculateDamageRva,
        reinterpret_cast<std::uint64_t>(&DamageDetour), &damageTrampoline);
    if (!damageHook->hook()) { damageHook.reset(); return; }
    if (!sow::summon_probe::Install(doubleWights)) {
        RemoveDamageHook(); return;
    }
    if (!sow::raise_dead_bridge::Install(restoreRaiseDead, cleanupOldGhosts)) {
        sow::summon_probe::Remove(); RemoveDamageHook(); return;
    }
    if (!sow::ring_boost::Install(ringResurrectionMultiplier, ringBoostWithMask)) {
        sow::raise_dead_bridge::Remove(); sow::summon_probe::Remove();
        RemoveDamageHook(); return;
    }
    if (!sow::ring_might::Install(ringMightPerAllyHit, ringMightDiagnostics)) {
        sow::ring_boost::Remove(); sow::raise_dead_bridge::Remove();
        sow::summon_probe::Remove(); RemoveDamageHook(); return;
    }
    if (elementalResurrectedGrunts &&
        !sow::resurrection_weapons::Install(resurrectionWeaponDiagnostics)) {
        sow::ring_might::Remove(); sow::ring_boost::Remove();
        sow::raise_dead_bridge::Remove(); sow::summon_probe::Remove();
        RemoveDamageHook(); return;
    }
    if (aggressiveGrunts && !sow::grunt_aggression::Install(gruntPostPlayDelayScale,
        allowRevenantRunning, gruntAggressionDiagnostics)) {
        sow::resurrection_weapons::Remove(); sow::ring_might::Remove();
        sow::ring_boost::Remove(); sow::raise_dead_bridge::Remove();
        sow::summon_probe::Remove(); RemoveDamageHook(); return;
    }
    if (!sow::gear_resurrection::Install(guaranteedArmorResurrection,
        guaranteedUrfaelResurrection, gearResurrectionDiagnostics)) {
        sow::grunt_aggression::Remove(); sow::resurrection_weapons::Remove();
        sow::ring_might::Remove(); sow::ring_boost::Remove();
        sow::raise_dead_bridge::Remove(); sow::summon_probe::Remove();
        RemoveDamageHook(); return;
    }
    if (multiEquipSkillUpgrades) {
        sow::multi_skills::onActivated=&sow::resurrection_weapons::OnSkillActivated;
        const bool skills=sow::multi_skills::Install(multiSkillDiagnostics);
        sow::resurrection_weapons::multiSelection=skills;
        if(!skills) std::puts("[MultiSkills] installation failed; native selection retained");
    }
    // Optional probe failure does not disable the working gameplay features.
    if (enableGruntCommand) sow::grunt_command::Install(gruntCommandKey, gruntCommandDiagnostics,
        gruntCommandRequireHeldStealthKey, gruntCommandStealthKey, gruntCommandConsole, gruntRallyKey);
    else if (traceGruntStealthCommand) sow::grunt_command_probe::Install();
    g_pluginHooksInstalled = true;
    g_pluginHooksRemoved = false;
}

// The caller must prevent execution in all detours before removal/unloading.
void RemoveHooks() {
    const bool skills=sow::multi_skills::Remove();
    if(skills)sow::resurrection_weapons::multiSelection=false;
    const bool command = sow::grunt_command::Remove();
    const bool commandProbe = sow::grunt_command_probe::Remove();
    const bool gear = sow::gear_resurrection::Remove();
    const bool aggression = sow::grunt_aggression::Remove();
    const bool weapons = sow::resurrection_weapons::Remove();
    const bool might = sow::ring_might::Remove();
    const bool ring = sow::ring_boost::Remove();
    const bool bridge = sow::raise_dead_bridge::Remove();
    const bool summon = sow::summon_probe::Remove();
    const bool damage = RemoveDamageHook();
    g_pluginHooksRemoved = skills && command && commandProbe && gear && aggression && weapons && might && ring && bridge && summon && damage;
    if (g_pluginHooksRemoved) g_pluginHooksInstalled = false;
    if (enableDebugConsole) FreeConsole();
}
