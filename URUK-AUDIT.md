# Read-only Uruk audit

This is the first diagnostic step toward validating edited conquest defenders.
It does not repair traits, assign tribes, change appearance, or decide that an
Uruk is definitively cheated. It is separate from the working DLL and does not
change its bindings or hooks.

## Run

Requires Windows and 64-bit Python 3.9 or newer. No third-party packages are
needed. Start Shadow of War, then double-click `run-audit.cmd` from the extracted
folder. Open the Overlord/captain detail card during the siege overview and press
**F8 while the game is focused**. The console prints an audit and the output
JSONL path (by default `%TEMP%/SoW-UrukAudit-<UTC timestamp>.jsonl`). Ctrl+C stops
capture. Shift+X and Z in the plugin remain untouched.

Alternatively, from that folder:

```powershell
py -3 audit_uruks.py --label "ordinary-overlord"
```

Capture an ordinary Overlord first, then a known edited one with a different
label or clearly identified sample. User labels describe the test; they are not
trusted classifications. Compare the reported role/tribe/class to the displayed
captain, and, if available, compare `member` against CE's current Uruk pointer.

### If selection discovery fails

Automatic selection supports roster references and the native hierarchy's
faction/row/seat lookup. The latter is statically traced and fixture-tested,
but the tested siege overview leaves this global selection entirely zero.
Automatic capture therefore does not currently work in that tested menu; use
the exact CE-pointer mode below. An empty source reports `global-selection-empty`.
Other selection sources remain unverified. Resolution must pass signature, owner, key, vector
bounds and member/faction checks. Ambiguous or changing references are rejected.
It does not substitute a random captain when selection cannot be established.
Unresolved captures include `selection.diagnostic`: raw selection bytes,
reference attempts, faction/row/seat values, schema mode and hierarchy lookup
status. `selection_kind=0` alone says nothing about a captain's validity.
An ordinary Overlord and Warchief are sufficient to validate capture first;
a known edited sample is useful only after selection works.

Use CE's current **Uruk pointer** as a fallback, not a pointer from an old run:

```powershell
py -3 audit_uruks.py --member 0x12345678 --once --label "known-edited-overlord"
```

Replace `0x12345678` with the current address. `--once` captures immediately;
without it the explicit-pointer mode waits for F8. `--pid` selects a game process
if necessary, and `--output` selects a JSONL file. The same privilege level as
the game may be needed if Windows denies read access; the utility does not
automatically elevate or request debug privileges.

## What to look for

- Field statuses distinguish a readable null pointer from unreadable data.
- `assessment=review-required` means a listed configuration signal was observed,
  not that cheating or invulnerability is proven.
- `no-listed-signal-observed` is not a clean bill of health: this first policy
  covers only listed signals. Incomplete reads remain explicitly incomplete.
- An unnamed/unreadable member identity is `unknown-member-identity`; it does not
  receive trait/tribe-based findings.
- `fearless` and `overwhelming_awe` are reported as present/absent/unknown, with
  absence accepted only when both relevant lists were completely read.

The initial signals are a readable missing/explicitly tribeless tribe, exact
special-character AI/class/role/full-model identifiers, and exact mercenary-family
trait/marker identifiers. These need verified Orc/Olog conquest context before
any repair policy can act. Ordinary immunity combinations, level, Fearless and
cosmetic names containing words such as Nazgul do not themselves trigger a
cheating finding. No broad substring blacklist is used for appearance.

The program does **not** yet prove that the selected member is the imported
Overlord, distinguish imported versus home-army records, or prove when fortress
generation reads tribe. Its report marks that scope as unverified. This is why
this stage is read-only and needs controlled normal/edited comparisons.

## Field mapping

All offsets are relative to the persistent faction member, not the actor/stats.
CE labels and serializer evidence support these bindings:

| CE label / data | Member field | Native category / interpretation |
| --- | --- | --- |
| AI | `+0x188` | Combat/Tree |
| Class | `+0x278` | Character/Models; serialized as CharacterModel |
| Role | `+0x318` | Faction/Roles/Definitions |
| Tribe | `+0x328` | Faction/Tribes/Definitions |
| Applied traits | `+0x198 / +0x1A0` | Pointer-vector bounds |
| Marker abilities | `+0x1B0 / +0x1B8` | Pointer-vector bounds; separate from applied traits |
| Visual storage | pointer at `+0x2D8` | Equipped appearance storage |
| Base/body/head model | storage `+0x20 / +0x28 / +0x30` | CE's distinct appearance fields |

"Class" and visual model must not be conflated. `TribeDef_Tribeless` is an
explicit shipped identifier, in addition to the possible null tribe pointer.
A readable tribeless Overlord is a structural concern; the reported throne-room
failure/fix still needs an isolated in-game reproduction.

**Overwhelming Awe maps to `Faction/Traits/Definitions:Terror_Player`.** Its
display key `IntRes_Terror_Player` hashes to `0x0A8076A6`, whose shipped English
string is Overwhelming Awe. Ordinary Fearless is `Quirk_Fearless`; the distinct
mercenary definition `Easterling_Fearless` also exists. These are identifiers,
not a request to force or remove anything in this probe.

## Selection lookup evidence

Addresses below are RVAs for the inspected executable; preferred image base is
`0x140000000`. CE's Army View pattern currently resolves to `0x10BF93` within
member-property formatter `0x10BD90`, where RBX is the member. Its Open World
pattern resolves to `0x10B482`; that path obtains a member through `0x18D920`.
No hooks are installed at either site by this utility.

Native `0x16BCC0` copies UI-selection state from global
`0x26FFAA8 -> +0x6C80`. The utility probes the value's reference positions at
`+0` and `+0x28` as candidates, not as established enum meanings. Each candidate
must resolve through the exact faction manager and member-faction relationship.

The hierarchy member-property path `0x188CE30` calls `0x16E128` with a faction
key at selection `+0`, row byte `+8` and seat byte `+9`. These two bytes are not
a roster index. `0x1E5088` obtains the row definition from the faction's row
array; `0x1E4CD8` matches that record in bucket-object `+0xF8/+0x100`, whose rows
have stride `0x28`. Each row's `+8/+0x10` vector contains u32 roster indices.
The utility resolves the seat's index, checks the member's faction and row
record (`+0x268`), and rejects ambiguity or changed row/seat reads. It supports
the inspected ordinary row-array schema; alternate/overridden schema forms
remain explicitly unsupported. This path is an additional candidate, not a
claim that every siege menu uses this selection descriptor.

`0x18D93C(manager, reference)` resolves a faction key and member index. In its
native path, map `manager+0x98` has slots at +0, count at +8, owner at +0x10;
`0x18DE04` matches a key and returns its 0x290-stride slot+8. `0x18DDD4` follows
that object's +0x20, then `0x18DDE4` indexes its +0x18/+0x20 member vector.
The utility reproduces this with bounded external reads and a linear slot scan.
It does not invoke native hashing/resolver functions, which can assert on bad
owner tags. Selection signatures and references must match before accepting a
result. Identity tokens and selection bytes are re-read to detect changes.

Bounds are 4096 map slots, 8192 members per pool, 512 traits/markers per list,
and 192 bytes per record name. These are conservative diagnostic limits, not
claims about every valid game configuration. A rejected bound remains unknown.

## Validation and next step

36 synthetic tests pass, including ordinary/edited fixtures, exact Awe/Fearless
identification, roster ownership and bounds, ambiguous/changed selection,
changed member identity, unreadable fields, and cosmetic false-positive checks.
Hierarchy fixtures cover row/seat versus roster-index encoding, wrong member
rows, seat bounds, unsupported schemas and unresolved capture diagnostics.
Windows process discovery, read-only handle/module enumeration, wrong-process
rejection and the no-game error path were exercised. Run `py -3 -m unittest
test_audit_uruks.py` from the utility folder to repeat the fixture tests. No live
game process was running during this implementation, so selection and field
interpretation in the siege overview still need a live capture.

The next input is an ordinary Overlord audit using its current CE Uruk pointer.
The separately traced detail-card path `0x18EACD0` obtains the current widget
through `0xDAC2B0` (priority `+0xA78`, then `+0xA70`, then `+0xA80`), then
resolves the widget's member reference at `+0x3A20` through `0x170794`.
The owning widget/controller's live root is not yet established; no guessed
pointer traversal or memory scan has been added to the utility.
After capture is reliable, compare an ordinary and known edited Overlord. If the UI
path does not resolve, the exact CE-pointer fallback still tests the field and
trait reads. Once those comparisons are reliable, tribe repair can be tested
separately before launching the assault. Automatic repair remains unimplemented.

## Access and distribution

The process handle requests only VM_READ and QUERY_LIMITED_INFORMATION. The
utility contains no WriteProcessMemory, injection, remote function calls, native
hooks, or network activity. It writes only local reports. `audit_catalog.json`
contains selected record identifiers/comparison rules, not an extracted game
database, executable or localization file.

## Captured field checks

Explicit CE-pointer captures successfully read a home-world captain and an
invasion Overlord, with complete trait/marker lists and stable identity tokens.
The latter has `InvasionFaction_MtnPass` and `MtnPass_Row_Overlord`; this supports
the user-supplied context but is not yet an automatic scope authorization.
Stored level is zero-based: reports expose `level_raw` and `level_display`
(raw + 1); raw zero is a valid displayed level one.

`DLC_Slayer` / `DLC3_Slayer` are not automatically forbidden. The shipped
`Faction/PresetMembers:DLC3ToMain_Slayer` references `Character/Models:DLC_Slayer`,
`Combat/Tree:AI_Spearman` and `DLC3_SlayerRoles`, with Commander/Hunter markers.
These identifiers alone cannot establish editing. The captured defender's eight
immunity records and Marauder tribe with `TribeAttack_Slaughter` are review
observations, not currently implemented detection rules or proof of cheating.

## Mercenary epic traits and conservative repair policy

The captured user-labelled hacked Overlord contains eight exact DLC4 `East_ET_`
records: DamageBoostVsRage, DefenseBoostVsRage, HealthOnKill,
DamageBoostEnemyHealthLow, DamageBoostSelfHealthHigh, DefenseBoostSelfHealthHigh,
DefenseBoostSelfHealthLow and DamageBoostSelfHealthLow (each prefixed `East_ET_`).
The exact 20 shipped `Faction/Traits/Definitions:East_ET_*` identifiers are now
in the comparison catalog. Their definitions reference DLC4 UI bundles/textures
and corresponding inventory items; this is evidence beyond a name substring.
These captured records trigger mercenary-context review findings. Magnitudes,
stacking and runtime activation have not been measured. No repair is implemented.

The desired policy tolerates minor edits. Necromancy, extra tribe attacks,
Deadly, attack speed and lifesteal alone must not automatically trigger repair.
`Quirk_Deadly` has ordinary role picker references and is selected by the shipped
`DLC3ToMain_Fixer` reward preset. This is static evidence of legitimate routes,
not proof that any specific captain acquired it legitimately. The hacked sample
contains Deadly, attack speed and lifesteal, whereas the previous defender
contains the latter two without Deadly or the eight captured mercenary epics.
The shared name hash across these different samples is not a unique identity.

## Configurable repair requirements

User-approved scope: blacklist the exact Easterling mercenary trait identifiers
for the optional defender-repair feature, while tolerating minor unrelated edits.
Provide an explicit master enable/disable setting; disabled must skip scanning
for repair, all mutations and native repair calls. Keep detection/reporting
separate from mutation. The setting is a requirement, not an implemented feature.
Use verified imported Orc/Olog defender context before applying the blacklist;
a manual toggle does not by itself prove campaign or faction scope.

The second captured hacked Overlord (Feral tribe, Role_BeastButcher, Defender
class/AI, Beastmaster specialization) produces exactly the same eight mercenary
epic findings as the first, with complete lists and no incomplete fields. Its
tribe attack is Feral and does not need tribe-mismatch repair. This confirms
reporting against a second live sample, not runtime removal or buff reversal.
Future implementation must remove native applied effects as well as stored
trait membership and verify both; editing a pointer vector alone is insufficient.

## Optional repair prototype

A separate source-only module is now available: see DEFENDER-REPAIR.md. It is
disabled by default and has no automatic integration into existing hooks.cpp.
When explicitly enabled it attempts native mercenary-trait removal only on
unspawned, owner-matched invasion Orc members formatted by the native detail UI.
Its mock-native tests pass; in-game behavior remains unverified. The Python
audit itself still performs no mutations. Earlier statements that repair is
unimplemented refer to the audit, not this separate experimental module.
