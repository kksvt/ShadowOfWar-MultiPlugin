# Imported defender repair

## Scope and settings

Only three public policy settings exist: enableDefenderRepair,
forceTerrorPlayerOnFlaggedDefenders and defenderRepairDiagnostics; current defaults
are true. Export/player debug options are separate. Disabling stops future edits,
not restoration of removed traits.

Complete owned imported members observed through formatting/construction arm
repair automatically. No key is required. Loaded-roster/bodyguard traversal is
bounded to 512 pool slots and throttled to 1.5 seconds; unloaded members are not
invented. Faction arming lasts until replacement/removal/restart and alone is not
a mission ID.

Stored edits require validated manager/owner, imported faction, identity, named
row, complete bounded data and an unspawned member. Home/DLC factions are excluded;
disable repair for DLC campaign play. Post-binding effects are revalidated against
registered flagged actors. The plugin edits local state, with no remote upload.

## Findings and cleanup

Findings include 110 exact blacklisted Easterling/DLC4 traits, more than three
distinct epics (58 shipped epic-group definitions), supported
[class/title mismatches](DEFENDER-ABILITY-POLICY.md), and imported structural
incompatibilities under the [strict AI/class/tribe policy](DEFENDER-STRUCTURE.md).
Three epics are tolerated by policy. Ordinary immunities, Fearless, high level,
Deadly, speed, necromancy or multiple tribe attacks alone do not prove cheating.
Unknown/unreadable data defers rather than becoming a readable null or verdict.

Runtime tables in sow_defender_*.hpp are authoritative. Accompanying defender_*.json
audits retain exact identifiers/grant evidence; illustrative legacy catalogs do
not supersede the runtime allowlists.

Native deactivation is followed by removal of resolved/unlocked source picker
slots, preventing the same source from simply rebuilding the removed trait.
Post-bind cleanup removes exact mercenary-only effects. The item audit separates
42 mercenary-only definitions from 64 shared ordinary-trait definitions; shared
provenance is retained. Indirect scripts/procs are not exhaustively neutralized.

No Chance/Life Steal and supported off-class reinforcement, Second Wind and Dying
Command effects are rechecked after binding. Legitimate class/title permissions
remain, subject to Awe's separate capacity planner. See ability policy.

## Awe, morale and vulnerabilities

Forced Awe gives Terror_Player, removes Quirk_Fearless and cleans residual
Item_Quirk_Fearless, Item_Inspired_Morale_Buff and Item_Aura_ExtraMorale. This
restored fleeing in tested captures; it does not replace the native flee planner
or guarantee every defender flees.

Capacity planning respects the 64-total-trait cap and subset limits. It can
sacrifice secondary bonuses, then eligible exact bonus/epic/hate/immunity records.
Native refusal, incomplete reads and failed readback remain failures. Unrelated
loss disables further repair; restoration is best effort, not atomic rollback.

Force-Awe also converts these immunities on flagged defenders:

| Immunity | Mortal definition | Picker / option |
| --- | --- | --- |
| Immune_Combat | MortalVulnerable_Combat | Picker_Resist_Combat_MortalVulnerable /1 |
| Immune_Ranged | MortalVulnerable_Ranged | Picker_Resist_Ranged_MortalVulnerable_Perm /0 |
| Immune_Stealth | MortalVulnerable_Stealth | Picker_Resist_Stealth_MortalVulnerable_Perm /0 |
| Immune_Fire | MortalVulnerable_Fire | Picker_Resist_Fire_MortalVulnerable_Perm /0 |
| Immune_Poison | MortalVulnerable_Poison | Picker_Resist_Poison_MortalVulnerable /1 |
| Immune_Beast | MortalVulnerable_Beast | Picker_Resist_Beast_MortalVulnerable /1 |

Immune_Frost, Immune_Curse, DEMO_ImmuneToCurse and
Trait_DLC3_Resist_Light_Immune are removal-only: exact mortal counterparts do not
exist. Immune_Combat concerns executions/combat finishers, not universal melee.

Forced cache slots use verified option indices and zero flags. Ordinary
Combat/Poison/Beast pickers can downgrade for Overlords; Combat's shipped fallback
option points to Vulnerable_Fire. Native saves serialize picker identity, not the
forced cache result. [Provenance](DEFENDER-RELOAD.md) enables conditional reapplication.

Residual immunity cleanup removes exact parent/immune leaves; Beast uses
ParentItem_Immune_Monsters. Shared resistance and Item_LastChance_NoPerfect remain
because replacement mortality can need them. Mortal parents can select resistant
leaves while weaknesses are unknown, so trait cards do not promise instant kills.

## Damage protection and native paths

Registered flagged actors have finite positive G_CalculateDamage output zeroed
against all targets, preserving healing. Identity is revalidated; registry bound
is 512. Unattributed/scripted paths, knockback and other calculation bypasses are
not universally covered.

| RVA | Purpose |
| --- | --- |
| 0x8C92BC | Formatter observation |
| 0x1036D0 / 0x40ED18 | Full construction / member-actor binding |
| 0x353454 / 0x3566B0 | Trait deactivate / activate |
| 0x7A7378 / 0x7CD0CC | Source-slot erase / push |
| 0x1C0840 | Inventory remove; INT_MAX requests ordinary copies |

A nonblocking mutex serializes repair; contention/reentrancy can defer work.
Pre-construction processing covers later initialize=false bindings. There are no
raw trait/inventory-vector rewrites. Diagnostics are deduplicated; complete bounded
trait dumps belong in [JSONL exports](URUK-EXPORT.md), not repeated console output.
