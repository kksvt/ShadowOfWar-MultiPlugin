# Validation and remaining limits

## Current evidence

| Area | In-game evidence |
| --- | --- |
| Grunt/captain and wight damage multipliers | User-confirmed working |
| Doubled mask summons plus Raise Dead | Confirmed at one and two Might bars |
| Ring resurrection bonus | User-confirmed; traces show effective 24/40 caps on ordinary and mask paths |
| Ring ally-hit Might | Positive user report; exact per-hit amount and every ally type not controlled-measured |
| Ordinary and gear-resurrected elemental weapons | User-confirmed working |
| Blackened Servant Armor / Urfael resurrection | User-confirmed working |
| Aggression / revenant movement | User-confirmed improvement; redirects and timer scaling observed |
| Group attack | User-confirmed working; individual command/target/attack/damage traces observed |
| Rally, Olog rally and participant icons | User-confirmed working |
| Purchased upgrade multi-equip | User-confirmed working; persistence and exhaustive combinations not separately tested |
| Defender repair | User-confirmed repair/fleeing/damage suppression on previous classes; expanded Berserker/Defender/Olog coverage fixture-tested, live check pending |
| Read-only defender audit | 38 fixture tests; tested siege menu leaves global selection empty; exact CE-pointer capture required |

The cleanup toggle is implemented; its two settings have not been documented as
a separate controlled test matrix. A general positive report is not proof of
every optional configuration or every possible native game state.

## Build and synthetic checks

The implementation passed MSVC x64 C++17 syntax checks and targeted synthetic
checks for classification, equipment lifecycle, bounded resurrection jobs,
selection/radius rules, targeting, group fan-out, registry relocation, rally
ownership, markers and skill sibling preservation.

A native machine-code harness compared the command gateway with its original
getter and deliberately clobbered registers/floating-point state. It checked
pass-through/override behavior, GPR/flags/XMM/x87/MXCSR preservation, actual call
redirection/restoration, unwind regions and signature-mismatch refusal.

Synthetic tests demonstrate these contracts under their fixtures. They do not
prove arbitrary build compatibility, pathfinding, native combat stacking, save
persistence or full-process lifecycle safety. Local test scripts are development
tools, not substitutes for the in-game observations above.

## Interpretation rules

- Failed/unreadable identity is not evidence of a grunt or enemy.
- A marker means a target or participant is tracked, not that movement or a hit completed.
- Mode 2 confirms command state, not attack success. Check target match and native attack/damage evidence.
- Damage calculations can be repeated or speculative. Correlate actor pairs and health changes.
- Accepted/dead/pre-dispatch corpse counts are not completed resurrections.
- Registration index is a useful tested heuristic, not a comprehensive engine rank enum.
- Native attacker-slot arbitration, resistance, movement and eligibility checks remain active.

## Defender audit scope

The separate [read-only utility](URUK-AUDIT.md) does not establish that a member
is an imported Overlord or definitively cheated. Missing/unreadable data and
ordinary Fearless/immunities are not cheating findings. The overview tribe/throne-
room timing cannot be established by that utility alone. It performs no repair.
The separate DLL repair feature now activates automatically for complete scoped
imported rosters. User testing confirmed Dark tribe/room replacement for ShadowWar
and Tribeless Overlords. The v6 loading capture confirms automatic activation and
off-class Second Wind removal; it is not proof of every native path or save-state
guarantee. See [current repair behavior](DEFENDER-AUTOMATIC.md).

## Open questions

User-reported: too many simultaneously active Gondorian Wights or resurrected
grunts may break their AI. The cause, safe population threshold and recovery method
are unverified. Raised summon/resurrection limits are not a guarantee of correct
AI behavior at those populations, and no global active-undead cap is enforced.

Multi-equipped action/element combinations can still resolve by native priority.
Persistence of all active siblings across save/reload needs a specific test.
Ring hit semantics for every ally, blocked hit and elemental hit are not mapped.
The corpse blocker `0x190EFC` reads object `+0x22B8 -> +0xD0`; its precise flag
meaning remains unknown. Why some enhanced attacks choose ordinary corpse ticks
is not fully explained, although the current override handles the observed path.

Enemy-captain resurrection is not implemented. Native dispatcher `0x1A91294`
uses member resolution `0x18D920`, with distinct registered construction
`0x1A904D0` and unregistered construction `0x1A90520`. This establishes separate
paths, not safe enemy eligibility, allied-undead conversion or persistent roster
updates. Those remain unresolved.

Other executable builds, alternate player states, exhaustive entity taxonomy,
all optional settings and arbitrary DLL hot reload are outside current validation.

## Reproducing a finding

Record the executable hash and relevant configuration. For static inspection,
add the loaded/preferred module base to documented RVAs. For runtime checks,
isolate one feature and keep complete paired actor/target or equipment records.
Use bounded diagnostics; retain unknown fields as unknown. Report observations
and inferred interpretation separately.
