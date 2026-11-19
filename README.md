# Shadow of War gameplay plugin

Windows x64 plugin using PolyHook2 and build-specific native paths. Configuration
is in hooks.cpp; changing it requires rebuilding and restarting the game.

## Features

| Feature | Current behavior |
| --- | --- |
| Damage | 16x finite positive Orc/Olog grunt damage to captain candidates; 16x wight damage to identified non-player targets |
| Mask of the High King | Six/twelve wights at one/two Might bars alongside Raise Dead; configurable old-wight cleanup |
| Isildur's Ring | 4x enhanced resurrection limits including mask casts; ally-hit Might proc configured to +4 |
| Blackened Servant Armor / Urfael | Equipped resurrection-chance contribution raised to 100%, retaining native eligibility |
| Raised-grunt weapons | Selected Ringwraith element on ordinary, mask and supported gear resurrections |
| Aggression | All-faction Orc-grunt walk-only redirection and post-play cooldown multiplier 0.4 |
| Group attack | Nearby friendly Orc grunts receive one target; candidate override below 2m; scoped target-cooldown bypass; compile-time experimental rage fallback |
| Rally | Allied Orc/Olog grunts approach the player, with temporary participant markers |
| Multi-equip | Selecting purchased upgrades preserves already-equipped purchased siblings |
| Defender repair | Automatic imported-roster normalization, trait/effect cleanup, outgoing-damage suppression, optional Awe/mortal vulnerabilities |
| Support export | Automatic imported-roster JSONL snapshots and additional F8 captures |

The scoped cooldown bypass has been observed allowing native rejections in live
group commands. It does not eliminate every delay: later captures showed nearby
followers tracking the target while native attackability remained false. Its
cause is unresolved. Grunts engaged elsewhere can still delay obeying an order.
**Too many active wights or revenants may break their AI.** No safe population cap
is known.

## Controls and defaults

| Input | Action |
| --- | --- |
| Hold Left Shift+X; release X | Aim and issue the group attack |
| Z | Rally allies, without Shift |
| F8, game focused | Uruk snapshot; in menus, hold while opening a profile card if needed |
| Open conquest overview/cards | Automatic imported-roster repair; no repair hotkey |

Attack sources must be within 30m of the player and 40m of the target. Rally
selects within 30m and stops tracking at 4m arrival or after 20 seconds.

Current defaults: bypassCommandTargetCooldown=true; experimental rage recovery is
excluded unless SOW_ENABLE_COMMAND_RAGE_RECOVERY is defined.
forceCloseCommandAttack=true, traceCommandCrowding=false,
ignoreCommandedGruntsForAttackLos=false. Routine grunt/skill logs are off.
Forced Awe, defender diagnostics and support exports remain configured;
enableDefenderRepair=false for the current local command trial. Re-enable it for
imported-defender repair. The v10.23 diagnostic defaults to
`traceCommandActivation=true`, with the older cooldown/perception/planner
summaries disabled. It observes both native reactivation callbacks, handle
resolution and final activation, emitting changed command-target summaries to
the console and existing command log. See [the activation trace guide](COMMAND-ACTIVATION-TRACE.md).
Set `traceCommandActivation=false` to omit its six observation hooks on the next
rebuild/restart. Define
SOW_ENABLE_COMMAND_RAGE_RECOVERY in the
project preprocessor settings to compile and enable the optional release/enrage
fallback; its delay and runtime options then appear in hooks.cpp.

The v10.21 experimental `prioritizeCommandTarget=true` gives the current owned
enemy first consideration in native actor selection, addressing observed target
suspension. It preserves global budgets and restores its temporary candidate
list after each native policy call. Another actor can lose selection priority.
This remains enabled for the activation investigation; it is not a general
circling fix. Crowded captures distinguish native target suspension from a
separate pending-reactivation stall.
See [the trial notes](COMMAND-TARGET-PRIORITY-TRIAL.md) for scope and live testing.

**Development default:** debugPlayerInvincibility=true suppresses finite positive
calculated player damage. Disable for ordinary play. It does not prevent every
scripted death or environmental failure. Other options remain in hooks.cpp.

## Documentation

| Document | Contents |
| --- | --- |
| [Discoveries](DISCOVERIES.md) | Build identity, entity/member layouts, records and damage |
| [Necromancy](NECROMANCY.md) | Summons, resurrection, ring/gear effects and weapons |
| [Commands](GRUNT-COMMANDS.md) | Aggression, targeting, close eligibility, rally and native paths |
| [Circling research](COMMAND-CIRCLING-RESEARCH.md) | Shared target cooldown and separate native planner gates |
| [Activation diagnostic](COMMAND-ACTIVATION-TRACE.md) | Current two-stage callback trace and capture instructions |
| [Multi-equip](MULTI-SKILLS.md) | Purchased siblings and elemental conflicts |
| [Defender repair](DEFENDER-REPAIR.md) | Scope, detection, cleanup, Awe and mortal conversions |
| [Structural policy](DEFENDER-STRUCTURE.md) | Exact AI/class/tribe compatibility and appearance effects |
| [Ability policy](DEFENDER-ABILITY-POLICY.md) | Class/title exceptions and conservative necromancy evidence |
| [Reload provenance](DEFENDER-RELOAD.md) | Local journal, identity matching and persistence limits |
| [Support export](URUK-EXPORT.md) | Automatic/F8 files, inventory fields, bounds and Vendetta status |
| [External audit](URUK-AUDIT.md) | Separate read-only Python utility |
| [Validation](VALIDATION.md) | Evidence and unresolved limitations |

## Build and lifecycle

Use MSVC x64, C++20 and the project's PolyHook2 dependencies. Include hooks.cpp
and the companion sow_*.hpp headers together. Generated command gateways require
no assembler in the plugin project. Offsets target the [inspected build](DISCOVERIES.md),
not every release.

Initialize outside the loader lock. Installation/removal require quiescent game
threads. Check removal results; do not unload while detours execute or removal
failed. Arbitrary hot reload is unsupported. Optional failures report their status
and can retain the working features.

Source packages contain no game executable or extracted full database.
Diagnostic research notes can include example process addresses; these are not
stable offsets. These docs assign no license to code or dependencies.
