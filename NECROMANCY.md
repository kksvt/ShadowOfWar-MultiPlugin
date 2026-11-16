# Necromancy and equipment

The features below are user-tested. Native trigger qualification and corpse
eligibility remain in effect; raising a selection cap does not guarantee that
many valid corpses exist or successfully resurrect.

## Active undead population limitation

Having too many Gondorian Wights or resurrected grunts active at once may break
their AI. This is a user-reported gameplay limitation; the cause and a safe maximum
population have not been established. Higher summon/resurrection caps do not
guarantee that every spawned ally will behave correctly.

Avoid accumulating very large undead armies. Keeping `cleanupOldGhosts=true`
retains native cleanup of previous wights when recasting; lowering the summon or
resurrection bonuses may also help limit accumulation. Neither is a verified fix
for AI that has already stopped working. The plugin does not enforce a global
active-undead population cap.

## Mask of the High King

The native one-bar action `PC_Summon_Gondorians_Undead_NecroAoE_Lvl1` issues
three summon records; the two-bar `..._Lvl2` action issues six. Records are
`Gondorians_Undead_NecroAoE_1` through `_6`, spawning `Gondorian_Undead`.
Doubling each native rounded count gives six or twelve wights.

| RVA | Native operation |
| --- | --- |
| `0xF15EE0` | Action executor `(action, stats)` |
| `0x572740` | Summon request `(summon, override, stats, actor, variables)` |
| `0x30ECD8` | Count formula evaluation |
| `0x5727B2` | Scoped count-formula return site within the request |
| `0x114A138` | Native float-to-integer count rounding |

The count override requires the player, exact mask action/request names and
return site. Native rounding is applied before multiplication. Other formula
uses retain native results.

### Resurrection alongside summons

Ordinary Raise Dead uses an AOE/corpse path rather than the summon-request list.
`NecroAoE_Attack_Lvl1` and `_Lvl2` both reference `PC_NecroAoE_Lvl1` in the
observed roots. Their `_Ghosts` variants reference `PC_Kill_OldSummonedGhosts`.

| RVA | Operation |
| --- | --- |
| `0x30E758` | Root attack to embedded AttackData core |
| `0x56FE88` | AttackData to AOE record |
| `0x6B8470` | AOEOnStart getter |
| `0x15BD0C` | Create/initialize/queue AOE `(context, casterStats, AOE, flag)` |
| `0x4EA0A4` | AOE tick processing |
| `0x6EA548` | AttackData to resurrection record |
| `0x2E6534` | Corpse/resurrection processing |

The bridge learns ordinary/ghost AOE bindings and validates the player, active
attack, record owners and current native core before adding the normal Raise
Dead AOE. It uses the same cast context rather than spending Might a second time.
With `cleanupOldGhosts=true`, native cleanup is retained and resurrection is
added after successful cleanup-object creation. With false, the validated cleanup
request is replaced with resurrection. Other native lifetime/despawn rules remain.
The cache is mutex-protected; native calls occur outside its mutex.

## Isildur's Ring resurrection bonus

Affix `6Ring_5Lgnd_SetProp_Ringwraith` sets the boolean
`Ringwraith_RaiseAoE_Bigger`. Read it through stats `+0x318` and `0x1E48DC`
with owner validation. It selects `NecroAoE_Attack_Lvl1/Lvl2_Bigger` roots,
including the corresponding `_Fear` variants.

Decoded tick fields **Radius** (index 2, hash `0x4FC25CAA`) and **MaxAIToHit**
(index 7, hash `0x6129249C`) are independent controls.

| Tick family | Normal cap | Stock Bigger cap | Plugin Bigger cap at 4x |
| --- | --- | --- | --- |
| RaiseGrunts Lvl1 | 4 | 6 | 24 |
| RaiseGrunts Lvl2 | 6 | 10 | 40 |
| RaiseMoreGrunts Lvl1 | 7 | 9 | 36 |
| RaiseMoreGrunts Lvl2 | 10 | 12 | 48 |

Full names use `NecroAoE_..._Tick_LvlN`, with `_Bigger` appended for enhanced
records. Normal/Bigger Radius fields are 10000/20000; these raw AOE values have
not been independently calibrated to visible in-game distance.

Runtime Bigger roots can execute the ordinary Lvl1 grunt tick even for a two-bar
cast. The override therefore uses the active root's level, not just tick name,
to supply the stock enhanced cap and multiply it. Ordinary fallback ticks receive
the stock doubled radius. Validated ring+mask casts receive the same level-based
24/40 caps. Already-Bigger tick records retain their native radius.

| RVA / field | Meaning |
| --- | --- |
| `0x19D0A4` | MaxAIToHit getter |
| `0x4EAB70` | Radius selection |
| `0x19B740` | Collector construction |
| Collector `+0x108 / +0x10C` | Limit / accepted count |
| Collector `+0x10 / +0x18` | Candidate vector bounds |
| Candidate stride `0x28`, `+0x20` | Entry size / stats pointer |
| `0x2FBB54` | Dead-state check used by diagnostics |
| `0x190EFC` | Downstream object restriction check; precise flag meaning unresolved |

Live traces confirmed 24/40 and Radius 20000 for ordinary Bigger and mask roots.
Candidate/dead/pre-dispatch counts are not completed-resurrection counts.

## Might from ally hits

The ring reuses the native DLC ally-hit proc:

`5Armor_5Lgnd_Might_OnFollowerHitEnemy` ->
`ProcDef_Might_OnFollowerHitEnemy` -> `Quest/AutoObjectives:Ally_HitEnemy` ->
`Combat/ResourceGain:MightGain_AllyHitEnemy`.

The controlling float `DLC4_MightGain_OnFollowerHitEnemy` defaults to zero;
the donor affix's referenced FloatIndex has value 4.0. The plugin requests the
same proc and adds +4 while the exact player ring affix is applied.

| RVA | Operation |
| --- | --- |
| `0xFE68F0` | Five-argument equipment affix application |
| `0x1A105D8` | Additive float-variable update / dependent notification |
| `0x7A2AF0 / 0x7A2900` | Reference-counted proc registration / removal |

Tracked grants pair only the plugin's contributions and references on removal.
Other equipment sources are preserved. User testing reported the effect working;
per-hit amount and every possible ally/blocked-hit combination were not measured.

## Guaranteed equipment resurrection

| Equipment | Exact affix | Chance type |
| --- | --- | --- |
| Blackened Servant Armor | `4Armor_5Lgnd_SetProp_Ringwraith_Black` | `C_Resurrect_AllyDeath` |
| Urfael | `1Sword_5Lgnd_SetProp_Ringwraith` | `C_Resurrect_MeleeKill` |

The shared affix detour exposes before/after observers. Native chance-variable
value is evaluated at the application level through `0x64AA40`, then the module
adds `1 - nativeAffixChance` to manager stats `+0x320`. Helpers `0x1A2B314` and
`0x1A2B654` add/subtract the contribution; `0x17DB90C` and `0x17DB958` resolve
chance type and variable. No fixed assumption about a displayed 70/75% chance is
needed. Bonuses are tracked by equipment instance/stats and removed before native
unequip processing or during cleanup. Existing gear is not retroactively scanned.

Only native trigger chances are changed. Corpse eligibility, delays, melee-kill
qualification and other restrictions remain. There is no enemy-captain override.

## Elemental weapons on raised Orc grunts

Ringwraith upgrades `PSS_CBMeter_DarkTalion_Child1/2/3` set corresponding
`SkillB_CBMeter_DarkTalion_Child1/2/3` booleans for Fire/Poison/Curse. The plugin
adds `FlamingWeapon`, `PoisonedWeapon` or `CursedWeapon` to the replacement actor
after native resurrection. These inventory items supply elemental buffs and FX,
not just a cosmetic appearance.

Supported data are `GruntResurrection` and `GruntResurrection_GiveCurseWeapon`.
The ordinary player AOE scope covers regular and mask Raise Dead. Exact gear
ticks `RingwraithGear_Resurrect` and `RingwraithGear_Resurrect_AllyDie` can resolve
the live equipped player through tracked gear grants when the proc's AOE owner
is the corpse. Arbitrary non-player AOEs do not receive that fallback.

| RVA / field | Operation |
| --- | --- |
| `0x1A91294` | Select registered/unregistered resurrection job path |
| `0x1A90520` | Grunt job constructor `(job, corpseActor, data)` |
| `0x1A92558` | Worker returning replacement actor or zero |
| Job `+0x38 / +0x40` | Resolved corpse actor / resurrection data |
| `0x1E6B88` | Actor-level native item ADD and refresh |
| `0x4F12B4` | Actor-level native item REMOVE and refresh |
| `0x24DF7C` | Inventory item-presence check |

The add chain is `0x1E6B88 -> 0x1C01E0 -> 0x1C1224`; removal is
`0x4F12B4 -> 0x4F163C -> 0x1C0840`. They are not interchangeable.

Pending jobs are bounded to 256, expire after 30 seconds, validate reused job
addresses and retain associations for possible native retries after zero results.
Native calls occur outside the job mutex. Add only if missing, then verify item
presence. Do not grant to the corpse itself. Registered captains, Olog/monster
jobs and summoned wights are outside this extension; already raised actors are
not retroactively re-equipped.

With multiple equipped Ringwraith elements, raised grunts use one element:
the last-selected still-active upgrade, with a Fire/Poison/Curse fallback after
restart when no last click is known. See [multi-equip](MULTI-SKILLS.md).
