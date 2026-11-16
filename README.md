# Shadow of War gameplay plugin

A Windows x64 DLL plugin using PolyHook2 and build-specific native game paths.
The documentation describes the current implementation and findings, without
prototype histories or superseded installation instructions.

## Features

| Feature | Current behavior |
| --- | --- |
| Grunt damage | 16x finite positive damage from eligible Orc/Olog grunts to captain candidates |
| Gondorian wight damage | 16x finite positive damage against identified non-player targets |
| Mask of the High King | Doubled wight summons alongside ordinary Raise Dead |
| Old-wight cleanup | Configurable native recast cleanup |
| Isildur's Ring | 4x enhanced resurrection caps, including validated mask casts; native ally-hit Might proc configured to +4 |
| Blackened Servant Armor / Urfael | Equipped affix resurrection chance contribution raised to 100%, retaining native eligibility |
| Raised-grunt weapons | Selected Ringwraith element on ordinary, mask and supported gear resurrections |
| Grunt aggression | All-faction Orc-grunt walk-only redirection and post-play cooldown scale 0.4 |
| Group attack | Nearby eligible friendly Orc grunts receive the same selected enemy target |
| Rally | Nearby eligible friendly Orc/Olog grunts approach the player, with temporary participant icons |
| Skill multi-equip | Selecting an upgrade preserves already-equipped, purchased siblings |
| Defender repair | F9-armed invasion repair; guarded structural normalization, tribe inference, mercenary-effect cleanup, optional Awe and flagged-source damage blocking |

Healing and unrelated damage remain outside the damage multipliers. Registered
membership is a captain-candidate heuristic, not a universal rank definition.
Native eligibility, navigation and combat priorities continue to apply.

## Controls

These bindings reflect the current configured build; key constants remain
configurable in `hooks.cpp`.

| Input | Action |
| --- | --- |
| Hold Left Shift + X, release X | Aim and issue the group attack |
| Press Z | Rally eligible allies toward the player; no Shift modifier |
| Hold F9 while opening a defender card | Arm loaded invasion-faction repair and assault binding reapplication |

Installation messages need not contain key names. Attack participation is within
30m of the player and 40m of the target. Rally selects within 30m and ends at
4m arrival distance or after 20 seconds.

## Documentation

- [Discoveries](DISCOVERIES.md): build identity, entity layout, record names and damage.
- [Necromancy and equipment](NECROMANCY.md): summons, resurrection, ring effects and weapons.
- [Grunt behavior and commands](GRUNT-COMMANDS.md): aggression, group targeting, rally and native APIs.
- [Skill multi-equip](MULTI-SKILLS.md): ownership-preserving selection and elemental compatibility.
- [Defender repair](DEFENDER-REPAIR.md): three settings, supported classes, native effect cleanup and diagnostics.
- [Structural normalization](DEFENDER-STRUCTURE.md): current AI/class/role/tribe repair, retained-attack inference and validation limits.
- [Validation and limitations](VALIDATION.md): evidence status, reproducibility and unresolved behavior.
- [Read-only Uruk audit](URUK-AUDIT.md): separate defender-comparison utility; live overview selection is not yet verified.

## Read-only defender research

A separate `uruk-audit` Python utility captures candidate member records for
normal/edited Overlord comparisons. It performs no repairs and is not installed
by the gameplay DLL. See [its procedure and limits](URUK-AUDIT.md).

## Build and lifecycle

Use MSVC x64, C++17 or newer, with the project's PolyHook2 dependencies. Feature
configuration and integration live in `hooks.cpp`; include the accompanying
`sow_*.hpp` files, including `sow_multi_skill_data.hpp` and `sow_command_gate.hpp`.
The latter generates its gateway at runtime; no separate assembly source is
required in the plugin project. Headers named `probe` retain compatibility names
and helpers; the standalone observation-only command probe is disabled when the
working command module is enabled.

Offsets target the inspected executable, not every game release. Initialize
outside the loader lock and prevent game-thread execution in detours during
installation/removal. Check installation/removal results; do not unload while a
hook cannot be removed. Changing settings requires a rebuild and full restart.
The current loader itself has not been proven safe for arbitrary hot reload.

This documentation contains no game executable, extracted database, live heap
addresses or machine-specific project paths. It does not assign a license to
the plugin or its dependencies.
