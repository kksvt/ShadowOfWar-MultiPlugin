# Command circling: current findings

Evidence comes from the inspected build identified in DISCOVERIES.md, shipped
database records, IDA assembly/pseudocode, and local command captures.

## Current implementation and live results

The plugin keeps native command behavior, a scoped candidate override strictly
below 2m, and a scoped bypass of AI_Buddy_Command_Cooldown. The original cooldown
predicate runs first; only a rejection for a validated active ordered pair can
be changed. Other predicates and native synchronization remain effective.

Live captures with up to 18 active orders confirmed bypasses: one session reached
32 rejected evaluations and 32 bypasses, with no reported scope rejection.
Evaluations are not distinct followers or confirmed hits. Engagement improved
according to the tester, but brief circling still occurred before eventual attacks.

The final batch showed seven nearby followers with attackable=0, correct selected
targets, and occasional permission/requested-target matches. Cooldown counters
stayed unchanged through most of that wait, then advanced. This identifies a
separate attackability-false state; it does not identify its producer condition,
prove the exact planner leaf, or capture the eventual transition to attacking.
No further perception/permission bypass is justified by those snapshots.

The subsequent v10.17 capture measured 181-184 occupied slots out of 256 and
matching pair identities throughout. This excludes hard slot exhaustion for
those sampled pairs. Actual evidence persistence was 0.330 seconds. Slot 65
changed from fresh evidence at world 85.344 to cleared evidence at 88.351;
slot 140 had stale evidence at 90.362 and fresh evidence again at 93.369.
Other samples had zero stamp/duration, meaning no recorded refresh rather than
proof that valid evidence expired. Why refresh was absent remains unresolved.

## Native command lifetime and retargeting

The lifetime trace distinguishes three different mechanisms: the shared attack
cooldown, the native command's own deadline, and the plugin's release watchdog.
Equal twelve-second values do not make the first two the same timer.

The Orc command attack pool has 19 entries. Seventeen include
`ActionsOnAttackStop_Source=BuddyClearTarget`; only the two teleport entries omit
it. This action runs when the attack stops, which does not establish that a hit
landed. Its bridge at RVA 0xF132D0 reaches 0xF32DD0, which finds the registry entry
for that source actor, verifies mode 2 and matching target stats, then changes
that entry to available or inactive mode. It does not clear the whole group.
The native command is therefore an individual action order rather than a
persistent focus-fire instruction.

The commanded updater at RVA 0x1AA51BC does not simply cancel every order after
twelve seconds. Deadline expiry or a failed path/movement check can invoke the
`Command_TimedOut` picker. Entry +0x38 records that recovery action is pending;
it is not a generic target-check-failed flag. The updater then watches action
completion or a changed action serial before returning to available mode.
The supplied picker selects the non-looping `Command_TimedOut` smart object and
its attack type. This supports a possible late command attack after a wait;
it does not prove which attack ran in the latest capture.

The native timer writer at RVA 0x11CA8C uses DWORD game milliseconds from
`[[module+0x2702598]+8]+0x18`. Expiry at 0x18E390 compares that clock strictly
greater than the deadline. The plugin instead sets `finishUntil=GetTickCount64()
+15000`. The latest capture includes substantial differences between wall-time
sample intervals and advancement of the logged world clock. Therefore the plugin
can release an order before the native deadline expires when game time is slow
or paused. This is a verified policy mismatch, but the capture lacks individual
release events to prove it caused a particular follower's departure.

| Registry offset | Meaning established by native reads/writes |
| --- | --- |
| +0x20 | Maximum-duration deadline, game milliseconds |
| +0x24 | Bonus-duration deadline, game milliseconds |
| +0x28 | Movement/path sampling deadline |
| +0x2C | Sampled movement/path metric, float; not elapsed time |
| +0x30 | MaximumPathLength, float |
| +0x34 | Saved recovery-action serial |
| +0x38 | Recovery action pending |
| +0x80 | Buddy mode |

AI component `actor+0x2CE8` supplies the completion flag at +0xCC8 and action
serial at +0xCCC. The updater also checks +0x615 before timeout handling; its
precise semantic name remains unresolved.

Mode setter 0x1AA4B90 removes the old mode's items and stats and applies the new
mode's state. Available mode also has `CanFindTargets=0`, so a transition to
available alone must not be described as unrestricted target selection.
Removing a plugin-owned temporary buddy item subsequently unregisters that
temporary command context and permits ordinary AI targeting to resume.
That is a concrete path to other opponents, without requiring a corrupted
target pointer or a group-wide clear operation.

The next useful live measurement is a bounded per-follower lifecycle trace:
native mode-change caller, clear-target action, current attack, game deadlines,
and plugin release reason. In particular it should distinguish native completion,
native recovery, missing registry entry, and real-time watchdog release. It must
not label every updater exit as a timeout or every attack stop as a hit.
No gameplay timer or permission change follows from this research alone.

## Direct CircleTarget / Attack observation (v10.20)

`traceCommandPlanner=true` adds six optional observation detours. They require the
perception trace and its stable focused source, so planner and perception counters
refer to the same follower and epoch. All original arguments, outputs and results
are preserved. The permission predicate is observed only when native code calls
it inside one of these planner methods; the trace never evaluates it itself.

| RVA | Observed method |
| --- | --- |
| 0x493DF4 | CircleTarget start/admission |
| 0x4443E4 | CircleTarget update |
| 0x72FDF4 | CircleTarget cleanup/end |
| 0x445930 | Attack start/admission |
| 0x11E780 | Attack update |
| 0x445880 | Effective native attack-permission predicate |

CircleTarget's virtual start, update and cleanup entries were inspected at VAs
0x141DC6110/0x141DC6130/0x141DC6138. The corresponding Attack entries are
0x141FBD050/0x141FBD070. The circle updater returns 1 when its
selected target is absent, permission passes, or its own wait timer expires;
2 when its positioning record is absent; otherwise it returns the native
movement updater result. Therefore return 1 alone is not proof of permission.
Cleanup clears its +0x178 timer and AI+0x712 circling state.

`planner trace` records start attempts, result-zero admissions, update results
0/1/2/other, cleanup calls, actual nested permission passes/failures, and observed
immediate Attack+0x23 starts at admission and zero-to-nonzero start edges during
updates (`startsBegin/Update`). An admission or start flag does not prove
a hit. `planner events` retains the last four of eight stored transitions with
sequence numbers and event ages; repeated identical update results are counted
without flooding that ring. More than eight transitions between samples overwrite
old ring entries; cumulative counters remain. The sampled circle deadline uses
native game milliseconds and is the last update's value, not a live post-cleanup
read. Raw AI+0x710 is also shown for comparison with the effective predicate.

Focus can begin after a behavior started: CircleTarget update calls without a
captured start still establish that its updater ran. Zero calls only excludes
these observed methods for the focused source and observation window, not for
the whole group. This trace has fixture and compile validation; live installation
and the circling incident remain to be captured. No gameplay fix is claimed.

## Verified predicate context

| Location | Meaning | Native constructor/evidence RVA |
| --- | --- | --- |
| Predicate +0 | Selection-context pointer | 0x30D1A4 |
| Selection +0 | Source stats | 0x2DA938, independently 0xED73E0 |
| Predicate +8 | Temporary target descriptor | 0x30D1A4 |
| Descriptor +0x28 | Target stats | 0x30D1A4 |
| Predicate +0x10 | Target stats copy | 0x30D1A4 / 0xED7420 |

The reader verifies both target copies. Fixtures model native construction and
reject the obsolete direct-source-at-+8 layout. The requested-target weak reference
starts at AI+0x6E0, but its value is AI+0x700, confirmed by 0x11CD9C.

## Optional diagnostics

commandCooldownDiagnostics is true for the current investigation. Rebuild/restart
to get at most seven summary lines/second with the planner trace during active orders, in the debug console and
%TEMP%/SoW-GruntCommand-active.log. Set it false to omit the sampling callback.

| Field | Interpretation |
| --- | --- |
| totalPredicate / exactCalls | All observed cooldown predicates / exact named record |
| pass / deny / bypass | Native success / native rejection / scoped override |
| badContext / untracked / entity / command / registry / link | Scope rejection counts |
| nearSampled / nearRead | Bounded nearby planner samples / fully readable samples |
| nearNoPermission / nearNotAttackable | State in those samples, not proven causes |
| selectedMatch / requestedMatch | Selected target / requested weak target matches order |

Counters are cumulative across orders. A zero call count means no observation of
that predicate, not proof the timer was irrelevant. A positive bypass establishes
hook operation, not a completed attack. Permission can clear during an attack.

## Population and perception investigation

### There is a native 256-slot actor limit in this subsystem

Actor activation at RVA 0x3F9B1C calls 0x2D5090 and stores its result at
Actor+0x2BF8. The allocator scans 256 bytes at module+0x27C3D60, marks the first
free slot, and returns 0xFFFFFFFF if all are occupied. Assembly uses LEA for that
occupancy array: it is inline storage, not a pointer read from that address.
Teardown at 0x4A1918 calls 0x4A26F4 to free the slot and resets the index to -1.

The perception matrix pointer is stored at module+0x26FFC48. It has 256 rows,
stride 0x4808 (18440), each containing a row-owner pointer and 256 pair entries
of 0x48 (72) bytes. The read lookup at 0x11ACDC requires both actor indices below
256, a nonzero source row owner, and a nonzero pair target pointer. The mutable
lookup at 0x222B90 additionally permits an empty pair to be initialized.

This is a concrete reason excessive active actors could lose subsystem support.
It is not a measured limit on every rendered entity, prop, projectile, corpse or
Uruk roster entry. Slot occupancy and allocation failures were not captured in
the circling logs. Missing lookup in 0x11B834 returns before refreshing the target
perception outputs, so cached state can remain stale. Raising a single constant
would be unsafe: several matrices, masks and lookups share the fixed capacity.

The allocator is called at activation in the traced path; no retry while the
same actor stays active was established. Therefore temporary circling that resolves
with the same participants is not, by itself, strong evidence of slot exhaustion.
A population-dependent update-budget cutoff was not identified in this pass.

### Visible and attackable represent different cached perception evidence

For the ordinary actor-target branch, 0x11B834 reads the selected target actor's
pair entry and writes these fields on the source AI component:

| Output | Exact native expression |
| --- | --- |
| AI+0xB8, logged visible | pair+0x43 != 0 OR pair+0x44 != 0 |
| AI+0xB9, logged attackable | pair+0x43 != 0 |
| AI+0xBC | pair+0x40 state byte |
| AI+0xC0 | pair+0x42 != 0 |

Assembly confirms both calls to the byte getter 0x185FE0 and the separate output
assignments. These expressions apply to this actor-target branch; other target
kinds have different handling. The ordinary Attack planner admission at 0x445930
calls 0x114A308, which tests the +0xB9 path before actual attack-pool selection.
Consequently granting AI+0x710 permission does not satisfy that admission test.

The two evidence flags have different writers:

- 0x119268 / 0x1AEC0C8 feed 0x43F4E4, which sets pair+0x43 and records a timestamp
  at +0x20 and a persistence duration at +0x34.
- 0x43F5FC expires +0x43 for enemy-type pairs after timestamp+duration, subject to
  its refresh flag. This is cached evidence, not a fresh physical test on every
  planner call.
- 0xF370B0 can establish awareness through 0xF36F00 and set pair+0x44 without
  setting +0x43; 0xF36D70 clears +0x44. Thus knowledge of the target and the state
  consumed by Attack admission can legitimately diverge.

The visual-stimulus preparation path is 0x4403B4 -> 0x4404B0 -> 0x119078. It uses
a per-target cache, transformed target sample point, range test and a query
constructed by 0x15DEAC (with a separate special-case route). Its result handling
feeds the first writer above. This path is distinct from the availability caller
0x2E5197 watched by the earlier command_los experiment. Zero eligible queries in
that experiment do not rule out this perception-query path.

Database records for Buddy_AttackingTarget use Buddy_Attacking_Default, with the
command-target entry Buddy_Vs_CommandTarget: horizontal/vertical FOV 180/90 degrees,
NoFOVRadius 100cm and NoFOVHalfHeights [-100,100]cm. Its perception duration record
declares TargetVisiblePersistence 0.5 seconds. These are record declarations,
not confirmation that each live follower used those evaluated values. The short
persistence window makes delayed/failed refresh a plausible mechanism; physical
occlusion by allied bodies and particular sensor scheduling delays remain
unproven. The later v10.17 capture measured a runtime duration of 0.330 seconds.

### What the existing captures establish

The final pasted batch had seven nearby followers with visible=1, attackable=0.
The newer local active-log tail repeats that pattern with five nearby followers,
including a sample with permission=1 and requestedMatch=1 at 1.46m. Cooldown
evaluation counters stayed flat during much of that wait. This is consistent with
an earlier perception admission gate preventing actual attack selection from being
reached, independently of the working cooldown bypass. It does not prove the exact
rejecting branch was executed in each sampled follower.

Earlier logs lacked pair evidence. The v10.17 capture confirms valid slots and
pair identities with absent/expired +0x43 evidence while +0x44 remained set.
Visual-query requests and results were not yet traced in that capture.
Do not infer physical line of sight solely from the current visible log label.

The current read-only diagnostic records, for the nearest tracked pair:

1. Occupied native slots out of 256; source/target Actor+0x2BF8.
2. Source row owner and pair target identity, with explicit missing/invalid status.
3. Pair+0x40/+0x43/+0x44 and +0x20/+0x34 versus the same native world clock.
4. Existing AI+0xB8/+0xB9, selected/requested target, permission and current attack.

Those reads distinguish absent matrix support from expired evidence without
changing perception or attack behavior. sow_command_perception_audit.hpp validates
identities again after reading; fields are still a best-effort snapshot, not an
atomic engine view. pair=ready is required before interpreting its fields.
sourceSlot/targetSlot=4294967295 denotes the native invalid index (-1).
slotsRead=0 denotes unavailable occupancy data, not an empty actor pool.
pastExpiry=-1 means clock/timing is unavailable, invalid or precedes the stamp;
1 means world time exceeded stamp+duration. Refresh flag +0x46 and enemy pair
type +0x41 also govern actual expiration. A full pool does not invalidate an
already valid pair. A no-slot actor can remain without a slot after occupancy
falls, so current occupancy alone cannot prove historical allocation failure.
The optional focused trace now observes six native boundaries without changing
decisions. Its configuration and counter meanings are in GRUNT-COMMANDS.md.

### Focused query trace boundaries

Dispatch 0x123BF4 calls preparation 0x4404B0 through sensor callbacks. Return 2
rejects that preparation path; return 3 follows query preparation or its special
case. Dispatch can append a 312-byte job at listener+0x80/+0x88. Completed jobs
are traversed by 0x43FFBC, which calls 0x123B34. That result-processing function
can stop at callback gates before 0x119400 fans out the completion callbacks.
The visual completion at 0x119268 calls classifier 0x64C668 and rejects old=4 /
new!=4 or old!=0 / new=0 before evidence refresh 0x43F4E4.

The observer measures dispatch, preparation, vector growth, delivery,
classification outputs and refresh entry. For kind 1, the stimulus target at +0x48 is an
actor object accepted by 0x274A3C, not stats; the listener starts with source
actor, whereas the preparation sensor stores source actor at +8. Fixture
construction follows those separate native layouts.

It keeps one tracked pair per focus window and records counters using try-locks,
without IO in detours. The perception audit uses at most five lines/second;
the optional direct planner trace adds two summaries. These
counts do not correlate individual job IDs or measure exact queue latency;
generic dispatch/queued jobs can include other stimuli for the same pair.
Missing classification after delivery can reflect callback gates or another
callback type. Transition rejection is not a physical obstruction diagnosis.
The first trace capture confirmed a healthy ranged-order pipeline with seven
queued/delivered/classified/refreshed jobs. In a subsequent circling order, the
focused pair's matched counters remained zero for about 14 seconds while its
evidence expired. This does not prove dispatch absence: actor-target selection
can use a second stimulus field which the original trace did not match.

The current trace observes native eligibility 0x1225D8, recognizes kind-1 +0x48
and kind-2 +0x70 actor targets, and includes source-wide activity independent of
target matching. The gate reads metadata plus an envelope whose +0x90 points to
the stimulus. Its early checks cover priority, actor/target modes, callbacks,
relationship mask and height, followed by per-listener cached version, pending
bit and retry interval. The new helper samples these simple conditions after a
native rejection without calling getters, writing state or replacing the return.
Those snapshots are candidate explanations, not exact taken-branch events.
Actor sensing enabled/interval/next-update fields are read alongside them.
Live results from this eligibility extension are pending.

## Principal finding: a shared target cooldown

Combat/Cooldown:AI_Buddy_Command_Cooldown has MinTime=12, MaxTime=12 and
TimerType=1 (world clock). The Orc command pool AI_Orc_MarkTargetCommand_Attacks
contains 19 attacks. Seventeen both declare CooldownStartTarget for that exact
cooldown in their AttackData and require PredicateCooldownAvailableTarget for
that same record. The two exceptions are teleport/teleport-no-path entries.

This includes frontal/back/knockdown executions, immune/resist variants, ranged
attacks, cannot-path responses and timed-out responses. It is target-owned,
not a separate allowance for each commanded follower. Thus one command attack
can make the same target unavailable to other command attacks for twelve seconds.
The fallback variants themselves are not a universal escape from this cooldown.

Native confirmation:

| RVA | Verified behavior |
| --- | --- |
| 0xED7420 | CooldownAvailableTarget uses target stats from attack query +0x10 and its cooldown table at +0x5A0 |
| 0x493298 | Tests that target's cooldown record; active blockers and remaining timer can reject it |
| 0x493320 | Returns nonnegative remaining milliseconds against the cooldown's selected clock |
| 0x1A78B34 | Evaluates the duration and records an expiry in milliseconds |
| 0xF13920 | Native StartCooldown action resolves a record in the receiving stats' cooldown table and starts its timer |

There is no attacking-follower identity in the target predicate's timer key.
The 2m override at 0x37A2C4 does not intercept this attack predicate.

The command's native MaximumDuration is also twelve seconds, with a three-second
BonusDuration and distance condition defined by the setup. The plugin additionally
has a fifteen-second watchdog. Consequently a follower can spend much of its
command lifetime waiting for a target cooldown opened by another follower. This
is a concrete group-command bottleneck, not evidence that every stall is a
cooldown stall. Existing captures did not log remaining cooldown time, so they
cannot prove this was the first rejecting condition in a particular attempt.

## There are separate planner gates after candidate admission

Passing a fight-circle candidate gate is not equivalent to starting an attack.
The native path is approximately:

1. Choose a target and update per-source perception state.
2. Admit a fight-circle candidate and allocate a stage.
3. Grant the source permission to attempt the staged attack.
4. Select the planner's Attack step, then evaluate nodes/pools/actual attacks.
5. Start the action if its conditions and animation/state constraints permit it.

The full close override changes step 2. It does not replace steps 3-5.

| RVA | Additional gate or state |
| --- | --- |
| 0x6F5000 | Stage request accepted only with a participant record and no outstanding raw+0x388 request; scheduled requests can be in the future |
| 0x6F53C8 | Grants attack permission through actor+0x2CE8: flag +0x710, requested target value +0x700 (weak ref starts +0x6E0), attack-type +0x708 |
| 0x445880 | Planner attack-permission test; ordinary branch reads that +0x710 flag, with a separate actor-specific branch |
| 0x445930 | Attack step admission additionally tests target kind, perception-derived attackability +0xB9, timing and attack selection |
| 0x493DF4 | CircleTarget step rejects when 0x445880 permits attacking; without permission it can start circling if its other conditions hold |
| 0x4443E4 | CircleTarget update keeps evaluating permission and movement positioning |
| 0x11E780 | Attack update can keep approaching/reselecting when no attack has been picked, or exit on lost permission/target |
| 0x4612D8 / 0x461074 / 0x275730 | Select actual nodes, pools and attacks; actual attack predicates remain effective |

A visually circling follower is not necessarily running the literal CircleTarget
planner step: attack approach and preferred-position movement can also look like
circling. Existing snapshots do not establish its exact planner leaf.

The source's perception-derived +0xB9 value is produced by 0x11B834 from target
selection/perception information. Getter 0x114A308 reads it. The normal branch of
0x2E5080 also derives candidate raw+0x43E from it when raw+0x43D is true. Thus
forcing the candidate result does not change the planner's later perception test.
These values must not be described as simple physical collision flags.

One retained own-context capture has raw+0x43D/43E/43F=1/0/1, no active attack,
and a distance of 0.23m, followed by allocation resuming later. The scoped ray-filter
trial recorded zero eligible queries. Those observations support investigating
the alternative readiness/perception path; they do not prove another grunt was
physically obstructing a ray or prove the +0xB9 branch was used in that sample.

## This is a specialized command attack, not ordinary group melee

Buddy_Command_PlaybookOverride supplies Buddy_Commanded. Its stage has
NumAttackersMin=NumAttackersMax=1 and requests AttackType=Command_StealthKill.
This is per stage/context; it is not a proven global one-attacker cap across
all custom orders. Raising this count globally is not justified by the data.

The attack node requires Buddy_Any inventory eligibility. The Orc pool requires
IsOrc. Actual attacks retain additional conditions: target distance, direction,
path, grounded state, damage/resistance/immunity variants and shared cooldown.
The frontal execution, for example, requests distance [0,200], a front-direction
predicate and a path. A resist variant has its own target-type/resistance and
vertical-offset requirements. They belong to synchronized-action categories.
Player-side awareness checks omitted by the plugin do not erase these conditions.

The native picker does use randomness among eligible attacks. The cooldown itself
is fixed at twelve seconds: equal minimum/maximum adds no duration randomness.
Timing, selection and interruptions can make the visible result seem random,
but randomness alone does not explain an empty valid-attack set.

## Why release plus rage can pick a distant enemy

Commanded mode disables CanFindTargets and supplies Buddy_Attacking perception,
IsAttackingMarkedTarget, Buddy_Command_PlaybookOverride and command movement items.
Leaving commanded mode removes its mode-specific state. After temporary buddy
unregistration, ordinary target selection can resume through the native
picker/allowed-selector set (0x11BF60 / 0x11BE00) and threat scoring
(0x11C9AC / 0x2FA99C). Available buddy mode itself still disables CanFindTargets.

AI_Rage_NonCatalyst adds VS_Player_Frenzy and FrenzyRoles, overrides morale and
uses Trait_Hatred alignment. It contains no pointer to the captain selected by
our plugin. It is therefore not a target-lock mechanism. Releasing the order and
then enraging intentionally gives ordinary native targeting a chance to choose
another opponent, including a distant one. This explains a capability of the
fallback; the recent aggregate logs do not identify the exact new target of each
released follower.

Live captures confirmed native rage application. They do not establish subsequent
target choice or completion of an attack. Recovery is compile-time optional.


## Validation and retained evidence

The offline audit verifies exact cooldown values, 17/19 command-pool membership,
stage count, native command duration and target-search restrictions. Field names
were matched using the executable folding table at 0x1DB50D0 and hash multiplier
919. Native paths were checked in IDA. Fixtures cover scoped bypass/original result,
context construction, weak references, close gate, group/rally and optional recovery.
The research bundle retains extracted evidence and pseudocode, not the full game
database or executable. Residual circling remains an acknowledged limitation.
