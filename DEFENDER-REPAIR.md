# Conquest defender repair

## Configuration

The feature has three user-facing options in hooks.cpp:

```cpp
constexpr bool enableDefenderRepair = true;
constexpr bool forceTerrorPlayerOnFlaggedDefenders = true;
constexpr bool defenderRepairDiagnostics = true;
```

- Enable: installs the preview/binding repair hooks, enables the native effect
  cleanup and suppresses positive calculated damage from registered flagged
  sources. False installs no defender hooks/observers or cleanup callbacks and
  performs no defender damage suppression. It does not undo earlier repairs.
- Force Terror_Player: adds Overwhelming Awe, replaces Fearless and removes its
  leftover inventory effect, and removes Inspiring Presence and its two morale
  items on flagged defenders. False skips this whole fear/flee policy; mercenary
  cleanup, off-class cleanup and flagged-source damage blocking remain active.
- Diagnostics: controls all routine defender repair, inventory, flee/controller
  and blocked-damage observations. False avoids installing the diagnostic-only
  inventory observer hook. Initialization failure messages remain available.

These replace the former individual policy toggles. F9 activation, full loaded
faction traversal, binding reapplication, excess-epic detection, secondary-trait
replacement and native mercenary-only item cleanup are internal defaults.
Player debug invincibility remains a separate gameplay/debug option, unchanged.
Shift+X attack commands and Z rally are unchanged. Grunt/multi-skill routine
logging remains disabled. Defaults above retain diagnostics for the coverage test.

## Use

Rebuild with the included hooks.cpp and companion headers, restart, then hold F9
while opening an eligible defender card in the conquest preview. Release F9.
The native UI formatter runs during loading too; F9 explicitly arms the work.
An eligible card seeds a bounded traversal of its loaded invasion-faction roster,
covering eligible bodyguards whose cards are not exposed. Passes are throttled
to one per 1.5 seconds. This is not a guaranteed enumeration of unloaded members.

F9 also arms the exact invasion-faction name for the session. During the assault,
new member pointers are reconsidered at native member/actor binding. Arming lasts
until module removal/restart or arming another invasion faction; later conquests
of that same faction can also be covered. It is not a decoded assault-session ID.
Disable the feature before DLC campaign play. Changes affect the local imported
copy; defender upload/persistence mechanics have not been independently proved.

Install outside DllMain with game threads quiescent. RemoveHooks must succeed for
all modules before unload. No DLL was installed or game memory changed by the
source-package preparation. Merge changes if your project has newer local edits.

## Eligible classes and detection

Exact standard Character/Models records accepted:

- Orc_Infantry, Orc_Infantry_Tough, Orc_Spearman, Orc_Archer, Orc_Archer_Tough
- Orc_Defender_Tough, Orc_Defender, Orc_Berserker, Orc_Berserker_Tough
- OlogHai_Infantry

The corresponding normal shipped AI mappings include AI_Defender, AI_Berserker,
AI_Berserker_Tough and AI_Olog_Infantry. Orc_Berserker_Tough is the shipped tough
savage model, not proof of cheating; it now receives the same trait checks/repair.
Story/reward variants
such as GP_Bruz and DLC_Slayer, Troll_Cave and unknown classes remain excluded.
Eligibility still requires a named row, no bound actor, exact allowed invasion
faction and matching faction-manager owner. Class alone never flags a defender.

SQ_Zog and ZTestVeryLargeOrcInfantry are normalized to supported ordinary Orc
classes when the guarded structural conversion succeeds. Unknown classes and full
special-character appearances remain unsupported. Import-scope validation is
separate from class support, so F9 can arm a valid imported roster from an
unsupported-class card. Home-world and DLC factions remain outside scope.

## Structural repair

Current normalization covers the narrow AI/class, special-role and invalid-tribe
replacements documented in DEFENDER-STRUCTURE.md. It runs on scoped, unspawned
members in the F9 roster pass and before actor construction. The full construction
hook may select a validated ordinary template; all other arguments and returns are
preserved. Native role/tribe setters retain their engine bookkeeping. Role_Outcast
also changes the displayed epithet to the Outcast.

Invalid tribes are readable null, Tribeless, Easterling, NazgulTwins and ShadowWar.
v5 uses one unambiguous retained tribe attack to select a stock replacement, with
Dark for missing/conflicting clues or a failed inferred-record lookup. Valid tribes,
including legitimate Undead, are preserved. No further tribe was blacklisted merely
because it lacks a tribe-list entry. Full observations distinguish unreadable data
from readable null and exact known-incompatible records.

The v4 live test completed with Dark fortress theming, a working throne-room floor
and no noticed oversized enemies. The new v5 inferred alternatives still require
live testing. An earlier non-reproduced F9 freeze has no confirmed cause. The debug
console disables QuickEdit selection. Archived v3 rollback sources are not current.

A member is flagged by any exact blacklisted mercenary trait, more than three
distinct epic traits, or the supported class/title mismatches below.
Epic classification uses 58 shipped UIGroup_Epics records;
the threshold of three is the user's policy, allowing the known three-epic case.
Ordinary immunities, level, Fearless, necromancy, extra tribe attacks, Deadly,
attack speed alone do not flag a member. Session identity tokens
remember flagged members for retry after their suspect traits have been removed.

## Repair policies

110 exact Easterling/DLC4 mercenary trait identifiers are removed using native
trait removal. Two additional combinations independently flag a defender and
have their stored trait and native inventory effect removed:

- Quirk_NoLastChance outside Assassin, unless the role is exactly Role_BlackHeart,
  Role_Cruel or Role_Merciless, or the preset is DLC3ToMain_Fixer.
- Quirk_LifeSteal outside Berserker, unless a verified title/preset grants it.
  The 19 role exceptions are exported in sow_defender_class_policy.hpp and
  defender_class_trait_audit.json. They include Bloodlust, Bloodthirsty, Bloody,
  BrainBiter, Cannibal, Devourer, FleshGlutton, FleshLover, LifeDrinker, PainLover,
  PainSeeker, Ravenous, Stout and Warmonger, plus five Nemesis Forge variants.
  SotP_Volcano_SC is a direct-grant preset exception.

The exception export follows the actual role/preset picker-grant field and selected
trait entries, not reverse references from exclusion lists. Exceptions are excluded
from class/title cleanup even when another trait flags the defender; the existing
capacity planner may still sacrifice an allowed bonus to make room for Awe.
Exactly one recognized ordinary
specialization is required. Missing/ambiguous markers, unreadable role names or
unreadable non-null presets do not establish a mismatch. These are current-state
rules, not a reconstruction of all past title changes or acquisition history.
Dying Command remains cleanup-only outside Commander on already flagged members.
Second Wind is not yet an automatic detection rule.

Slayer (Marker_Specialization_CombatMaster) plus Immune_Frost produces a review-only
diagnostic. Resistant_Frost does not. The normal PG_Frost_Immune reward excludes
Combat Master markers, matching community observations, but shipped data also has
Traits_CombatMaster -> CMT_FreezeImmune XP rewards at levels 10-30, 31-59 and 60,
which select Picker_Resist_Frost_Immune -> Immune_Frost. Whether that alternative
path remains reachable in this build is unresolved. Frost immunity alone therefore
does not flag a Slayer or cause frost-item removal. Known direct-grant presets are
also excluded from this diagnostic. No title-probability marker alone flags a class.

After binding, invalid No Chance and Life Steal effects are removed through native
inventory removal (Item_Quirk_NoLastChance and Item_Quirk_Lifesteal). Class/title
permissions are checked again, preserving allowed effects on otherwise flagged
actors. No new hooks or user-facing booleans are introduced.

When forced Terror is enabled, native addition grants Terror_Player and native
removal replaces Quirk_Fearless. Primary inventory cleanup also removes
Item_Quirk_Fearless, Item_Inspired_Morale_Buff and Item_Aura_ExtraMorale after
binding. Removing stored traits alone had left these active effects behind;
clearing the Fearless item was user-confirmed to allow fleeing. This does not
force or replace the engine's flee planner, and receiving new effects is possible.

Addition is planned before sacrifices against the 64 total-trait cap and a separate
faction subset limit. Both hash-based and optimized attribute paths are supported.
The replacement planner prefers secondary combat bonuses, then exact traits from
Bonuses/Epics/Hates/Immunities groups. It may sacrifice multiple such traits when
needed. Identity, class, base-strength and unrelated record categories are not
arbitrary replacement candidates. Incomplete plans/reads or native refusals are
logged rather than bypassed with direct vector edits. Native outcomes are verified.
Unexpected unrelated trait loss disables further repair; failed addition has
best-effort, capacity-checked restoration, not an atomic rollback guarantee.

## Inventory-effect audit

The shipped primary item-grant field (0xA15E0193) gives 127 mappings from 105 of
the 110 blacklisted traits to 106 items. Five have no direct primary item grant;
no absence of downstream/event effects is inferred. See defender_effect_audit.json
for identifiers and source traits. Full proprietary database data is not included.

42 items are mercenary-only relative to ordinary trait links and are removed via
native inventory removal with ownership verified afterward. 64 are also referenced
by ordinary traits and are reported but preserved because provenance is ambiguous.
The explicit forced-Terror policy takes precedence for the three Fearless/morale
items. Secondary/reaction grants, triggered procs, summons and indirect effects
are not claimed to be exhaustively neutralized. Native ownership removal is a
stronger check than trait/UI membership, not proof of every possible active effect.

Logs summarize removedItems, sharedRetained, lookupUnknown and failed. In the user
raid, six defenders each lost eight mercenary-only items (48 total), all lookups
resolved and no removals failed. A repeat binding removed zero already-cleaned
items. Shared normal bonuses/weapons were retained. That validates the tested
items/classes, not all generated entries or newly accepted classes.

## Damage protection

Registered flagged actors have finite positive calculated outgoing damage zeroed
against every target; healing is preserved. Actor/member/faction/seed/name tokens
are revalidated on each check. Immutable shared registry snapshots use the C++20
atomic shared_ptr specialization, with a C++17-compatible fallback. The registry
clears on install/removal and is bounded to 512 entries. Unknown actors, unattributed
hazards/status effects and paths bypassing the damage function are not guaranteed
covered. Native calculation side effects and knockback are not canceled. Clean
actors keep native damage. Global debugPlayerInvincibility is independent.

## Diagnostics

Every member-detail observation now includes all stored trait and marker record
names, including unflagged and skipped observed members. Preview roster snapshots
show the pre-repair lists; bind-before and bind-after snapshots allow comparison
across assault import and stored repair. Each member-detail line has a snapshot
number, followed by DefenderTraits lines with that same number, member pointer,
list kind, zero-based index, record pointer and exact name. The header retains
phase, actor, name hash, class, role and row so reused addresses can be distinguished.
Empty valid lists print count=0; incomplete/invalid lists are explicitly unavailable,
not empty. Individual names that become unreadable after the list read are marked.
The existing 512-entry per-vector safety bound remains; there is no lifetime dump
limit. These are stored member traits/markers, not an exhaustive active inventory
or buff dump. Both OutputDebugString and the allocated debug console receive them.
The existing defenderRepairDiagnostics option controls the dumps; no new option
or activation key is needed. Only members reached by the existing preview/binding
observations are captured; unloaded members are not promised.

When enabled, DefenderTrace entries have sequence, elapsed milliseconds and thread
ID. State snapshots report only changes, including stored Inspiring Presence,
inventory Fearless/terror/morale ownership and the native morale override list.
Raw controller fields are observations, not asserted planner-state meanings.
Item observers log before/after ownership, requested counts, native result and
best-effort caller/stack RVAs. Missing event traces are not proof no grant occurred.

Defender repair/trace logs have no lifetime entry caps. Cache, roster and read bounds
remain for correctness. Summary lines are separate from per-member details. The
probe does not mutate state; native repair and cleanup are distinct functional paths.
With diagnostics disabled, those repair paths still operate without the probe hooks.

## Native paths and limitations

Preferred base 0x140000000, inspected build only. Full formatter 0x10BD90 receives
the member in RCX; actor binding 0x40ED18 receives actor/member/initialize in RCX,
RDX/R8b. Stored repair runs before binding on unspawned eligible members; targeted
inventory cleanup follows binding for registered flagged actors. This covers paths
that bypassed the earlier narrower spawn finalizer. Native returns are forwarded.
A non-blocking mutex serializes repair; contended/reentrant callbacks can pass through,
so coverage is not unconditional. Whole-faction traversal is bounded to 512 pool slots.

Trait removal 0x353454 and addition 0x3566B0 handle native bookkeeping. Addition invokes
subset and total-cap cleanup, which the planner preflights. Native inventory removal
0x1C0840 clamps the requested count via 0x7FD964; INT_MAX requests all ordinary copies.
Prologue signatures gate calls/hooks. No raw trait/inventory array writes, synthetic
records, global aura stripping or forced AI flee calls are used.

Full beast/Balrog appearance conversion and unknown-class replacement remain
future work. One fortress's successful tribe/room result does not establish a
complete geometry compatibility whitelist; see DEFENDER-STRUCTURE.md.
The standalone Python audit is separate and read-only; automatic
siege-menu selection there is unresolved, so use the current CE-pointer mode.

## Validation

MSVC x64 C++17 repair fixtures and C++20 /W4 /WX cleanup/observer fixtures pass.
New tests verify admission and mocked full repair of Berserkers, Defenders and Ologs,
while Bruz/reward/Graug models remain excluded. The three-option configuration tests
cover master disablement and forced-Terror-off behavior. Full hooks.cpp syntax passes.
Class/title fixtures cover every exported role exception on clean and otherwise
flagged members, independent mismatch detection, class/preset exceptions, unknown
context, and diagnostic-only frost handling. Native inventory mocks verify invalid
effect removal, title-effect preservation and failed-removal reporting. These new
rules have not yet been exercised in the live game.
Ordinary tough-variant trait fixtures pass. Separate normalization fixtures verify
SQ_Zog/debug-Orc conversion, native role/tribe calls, fallback validation, unknown
exclusions, scope guards, idempotence and best-effort rollback after a failed write. Tribe fixtures
distinguish readable null, unreadable, ordinary named and explicitly tribeless records.
Structural fixtures distinguish Balrog,
Caragor/Warg, Nazgul AI and full Balrog visuals from cosmetic lookalikes, unknown
named classes and unreadable pointers. The tested v4 structural paths have live confirmation; new v5 inference requires live validation.

Live user tests confirmed stored repair, native item cleanup, restored fleeing after
Fearless-effect removal and positive damage suppression on previously supported
classes. Newly expanded class coverage and consolidated settings still need an
in-game pass. For that test leave diagnostics true, arm F9 and share the roster and
effect-cleanup summaries; set diagnostics false afterward for quieter normal use.
