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
#include "sow_defender_repair.hpp"
#include "sow_defender_flee_trace.hpp"
#include "sow_defender_morale.hpp"
#include "sow_defender_spawn_audit.hpp"
#include <polyhook2/Detour/x64Detour.hpp>
#include <polyhook2/Misc.hpp>
#include <memory>

namespace {
constexpr float gruntDamageMultiplier = 16.0f;
constexpr float wightDamageMultiplier = 16.0f;
constexpr bool debugPlayerInvincibility = true; // Blocks positive calculated player damage; disable after debugging.
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
constexpr bool multiSkillDiagnostics = false;
// Defender repair: only these three feature options are configurable.
constexpr bool enableDefenderRepair = true;
constexpr bool forceTerrorPlayerOnFlaggedDefenders = true;
constexpr bool defenderRepairDiagnostics = true;
constexpr int gruntCommandKey = 'X';
constexpr int gruntRallyKey = 'Z'; // Approach player, no stealth modifier required.
constexpr bool gruntCommandRequireHeldStealthKey = true;
constexpr int gruntCommandStealthKey = VK_LSHIFT; // Match your own stealth binding.
constexpr bool gruntCommandDiagnostics = false;
constexpr bool gruntCommandConsole = false;
bool enableDebugConsole = true; // Preserve the current project's console option.

using DamageFn = float(__fastcall*)(__int64, __int64, char, bool*);
std::unique_ptr<PLH::x64Detour> damageHook;
std::uint64_t damageTrampoline = 0;
thread_local bool handlingDamage = false;

void LogFlaggedFleeState(sow::Address actor,sow::Address base,const char* event) {
    if(!defenderRepairDiagnostics)return;
    const auto inventory=sow::At<sow::Address>(actor,0x2348);
    static constexpr unsigned char hasSignature[]={0x48,0x83,0xec,0x28,0x48,0x8b,0xc1,0x48,0x85,0xd2,0x74,0x28};
    static constexpr unsigned char hashSignature[]={0x0f,0xb6,0x01,0x84,0xc0,0x74,0x27};
    static constexpr unsigned char findSignature[]={0x45,0x33,0xc9,0x4c,0x8b,0xd9,0x41,0x83,0xe8,0x01};
    if(!inventory.readable || !inventory.value ||
       !sow::defender_repair::Signature(base+0x24DF7C,hasSignature) ||
       !sow::defender_repair::Signature(base+0x423E70,hashSignature) ||
       !sow::defender_repair::Signature(base+0x278540,findSignature))return;
    using HasFn=bool(__fastcall*)(sow::Address,sow::Address);
    const auto has=reinterpret_cast<HasFn>(base+0x24DF7C);
    const auto state=[&](const char* name){
        const auto item=sow::ring_might::Lookup(0x2700530,name);
        return item?int(has(inventory.value,item)):-1;
    };
    const std::array<int,6> values={state("Item_Terror_General"),state("Fear_Terrified"),state("Script_Item_NoFleeRemoval"),
        state("Item_Inspired_Morale_Buff"),state("Item_Aura_ExtraMorale"),state("Item_Quirk_Fearless")};
    if(sow::defender_flee_trace::enabled){sow::defender_flee_trace::StateChanged(actor,event,values);return;}
    char message[384]{};
    std::snprintf(message,sizeof(message),"[DefenderFlee] event=%s actor=%p terrorGeneral=%d terrified=%d noFleeRemoval=%d inspiredMorale=%d moraleAura=%d fearlessItem=%d\n",
        event,reinterpret_cast<void*>(actor),values[0],values[1],values[2],values[3],values[4],values[5]);
    OutputDebugStringA(message);if(GetConsoleWindow()){std::fputs(message,stdout);std::fflush(stdout);}
}

float __fastcall DamageDetour(__int64 context, __int64 a2, char a3, bool* a4) {
    const auto original = PLH::FnCast(damageTrampoline, static_cast<DamageFn>(nullptr));
    if (handlingDamage) return original(context, a2, a3, a4);
    handlingDamage = true;
    const auto base = reinterpret_cast<sow::Address>(GetModuleHandleW(nullptr));
    const auto hit = sow::ReadDamageContext(static_cast<sow::Address>(context), base);
    float damage = original(context, a2, a3, a4);
    const bool positiveNativeDamage=std::isfinite(damage) && damage>0.0f;
    const bool playerTarget = hit.targetPointer.readable && hit.targetPointer.value &&
        (hit.target.type.Named("Player_Character") ||
         (hit.target.actor.readable && hit.target.actor.value &&
          hit.target.playerMarker.readable && hit.target.playerMarker.value == hit.target.actor.value));
    // Keep the native calculation and its side effects, preserving negative
    // damage (healing). Apply before logging the final damage to command tracing.
    const bool blockedDefender = enableDefenderRepair && sow::defender_repair::DamageBlocked(hit.source);
    if (((debugPlayerInvincibility && playerTarget) || blockedDefender) && std::isfinite(damage) && damage > 0.0f) {
        damage = 0.0f;
        if(blockedDefender) {
            sow::defender_repair::Log("positive damage blocked from flagged actor",hit.source.member,"",true);
            LogFlaggedFleeState(hit.source.actor.value,base,"source-attack");
        }
    }
    else if (hit.ShouldBoostWight(damage)) damage *= wightDamageMultiplier;
    else if (hit.ShouldBoost(damage, includeOlogs)) damage *= gruntDamageMultiplier;
    // A non-attacking Overlord never appears as a source. Observe registered
    // targets too, without changing their incoming damage or AI behavior.
    if(defenderRepairDiagnostics && positiveNativeDamage &&
       sow::defender_repair::DamageBlocked(hit.target) && hit.target.actor.value!=hit.source.actor.value)
        LogFlaggedFleeState(hit.target.actor.value,base,"target-hit");
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
    if (damageHook || sow::defender_spawn_audit::hook || sow::defender_flee_trace::removeHook || sow::defender_repair::hook || sow::defender_repair::spawnHook || sow::summon_probe::actionHook ||
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
            // Prevent mouse selection in the debug console from blocking output.
            const HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
            DWORD mode = 0;
            if (GetConsoleMode(input, &mode))
                SetConsoleMode(input, (mode | ENABLE_EXTENDED_FLAGS) & ~ENABLE_QUICK_EDIT_MODE);
            freopen("CONOUT$", "w", stdout);
            printf("Debug Console initialized\n");
        }
    }
    const auto base = reinterpret_cast<sow::Address>(GetModuleHandleW(nullptr));
    damageHook = std::make_unique<PLH::x64Detour>(base + sow::offset::calculateDamageRva,
        reinterpret_cast<std::uint64_t>(&DamageDetour), &damageTrampoline);
    if (!damageHook->hook()) { damageHook.reset(); return; }
    if (debugPlayerInvincibility)
        std::puts("[DebugPlayer] calculated damage protection enabled");
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
    if (!sow::defender_repair::Install({enableDefenderRepair, defenderRepairDiagnostics, forceTerrorPlayerOnFlaggedDefenders}))
        std::puts("[DefenderRepair] installation failed; repair inactive");
    if(enableDefenderRepair && sow::defender_repair::hook){
        const bool normalization=sow::defender_normalize::Install();
        if(!normalization)std::puts("[DefenderNormalize] initialization failed; structural fields unchanged");
        if(!sow::defender_spawn_audit::Install()){
            sow::defender_normalize::Remove();
            std::puts("[DefenderSpawnAudit] installation failed; normalization disabled; existing trait repair retained");
        }
    }
    if(enableDefenderRepair && defenderRepairDiagnostics && !sow::defender_flee_trace::Install())
        std::puts("[DefenderTrace] inventory trace installation failed; basic flee observations retained");
    if(enableDefenderRepair &&
       !sow::defender_morale::Install(forceTerrorPlayerOnFlaggedDefenders, true))
        std::puts("[DefenderMorale] native cleanup initialization failed; inventory unchanged");
    g_pluginHooksInstalled = true;
    g_pluginHooksRemoved = false;
}

// The caller must prevent execution in all detours before removal/unloading.
void RemoveHooks() {
    const bool spawnAudit = sow::defender_spawn_audit::Remove();
    const bool normalization = sow::defender_normalize::Remove();
    const bool morale = sow::defender_morale::Remove();
    const bool fleeTrace = sow::defender_flee_trace::Remove();
    const bool defenderRepair = sow::defender_repair::Remove();
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
    g_pluginHooksRemoved = spawnAudit && normalization && morale && fleeTrace && defenderRepair && skills && command && commandProbe && gear && aggression && weapons && might && ring && bridge && summon && damage;
    if (g_pluginHooksRemoved) g_pluginHooksInstalled = false;
    if (enableDebugConsole) FreeConsole();
}
