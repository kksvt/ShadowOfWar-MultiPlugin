# Automatic imported-fort repair (v6)

## Usage

Rebuild with hooks.cpp and all companion headers from the v6 package, then restart.
Open the online-conquest siege preview normally. No F9 input is required. Repair
triggers when the existing native formatter encounters a complete, scoped imported
defender. Opening an Overlord/captain detail card exercises this same formatter if
the overview has not populated it yet. This is not a background scan or a new menu
hook. Diagnostics print:

```
[DefenderRepair] v6 installed; automatic imported roster repair; ...
[DefenderRepair] automatic imported roster pass member=...
```

The three configuration booleans remain enable, force Terror_Player and diagnostics.
Automatic activation is part of enable. There is no new automatic-mode option.
Disable the feature before loading a session in which no repair is wanted. Disabling
does not undo earlier repairs. Existing Shift+X and Z binds are unchanged.

## Activation guards

The existing formatter hook still forwards its native arguments and return. Before
formatting, a non-blocking repair mutex and re-entry guard protect the automatic
pass. The seed must have an exact allowed imported invasion-faction name, matching
current faction-manager ownership, a named row, no bound actor, nonzero readable
backing/appearance pointers and complete trait/marker lists. No foreground window
or keyboard state is consulted. Valid imported data may therefore be processed
while preview information is being prepared, before a card visibly opens.

The loaded roster retains its unique-bucket, stable-root/bounds, per-member identity
and 512-slot bounds checks. Scope is rechecked by the existing repair paths. Home
and DLC campaign factions, already spawned seeds and incomplete seed containers do
not initiate a pass. Imported NPCs may be repaired only because they belong to the
explicit imported scope; the guard does not attempt to identify a screen by name.

Repeated passes for the same faction-key address are throttled to 1.5 seconds. A
different key bypasses that timer. New data reusing the same key may wait for the
next formatter invocation after that interval. The existing pre-construction and
binding hooks continue the armed faction's repair during assault; no new hook or
worker-thread engine call was added.

## Off-class cleanup

On defenders already flagged or remembered for a separate repair reason:

| Trait | Retained when |
| --- | --- |
| Quirk_SecondWind | Advanced class is Tank |
| Quirk_DyingCommand | Advanced class is Commander |
| Quirk_NoLastChance | Assassin or existing verified title/preset exception |
| Quirk_LifeSteal | Berserker or existing verified title/preset exception |

Second Wind and Dying Command remain cleanup-only: neither independently establishes
cheating. Advanced class comes from one unambiguous specialization marker, not
weapon class or title. Missing/conflicting specialization leaves these traits alone.
No Chance/Life Steal retain their stricter role/preset completeness requirements.
The cleanup also runs when force Terror_Player is false.

The native trait remover is followed by verification. After binding, registered
flagged actors also lose all native copies of the off-class effect items:
SecondWind, DyingCommand, Item_Quirk_NoLastChance and Item_Quirk_Lifesteal. Eligibility
is recomputed using the same off-class policy. Clean/unregistered actors and valid
Tank/Commander/title exceptions retain their effects. No raw array writes are used.

Local game data verifies Quirk_SecondWind -> Inventory/Items:SecondWind and
Quirk_DyingCommand -> Inventory/Items:DyingCommand. Broader questionable traits,
including the unresolved Slayer frost-immunity case, were not newly blacklisted.

## Validation and next live check

MSVC C++20 /W4 /WX automatic-formatter, trait repair, spawn observer, runtime item
cleanup and normalization fixtures pass, along with full hooks.cpp syntax checks.
Tests cover no-key activation, original call/return preservation, owner/home/live/
incomplete/re-entry exclusions, whole-roster repair, throttling, new-key handling,
non-Tank removal, Tank preservation, ambiguous-class preservation and all-copy item
removal. Fixtures do not establish live native thread/context safety.

For the next test, do not touch F9: open the siege preview/card and retain the log
through assault. Confirm the automatic roster-pass line, native tribe verification
where applicable, and off-class trait-removal lines for a separately flagged
non-Tank with Second Wind. Valid Tanks should keep it. No DLL was deployed here.
