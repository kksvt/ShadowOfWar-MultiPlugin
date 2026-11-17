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

commandCooldownDiagnostics is false in hooks.cpp. Enable it and rebuild/restart
to get at most two lines/second during active orders, in the debug console and
%TEMP%/SoW-GruntCommand-active.log. Normal builds register no planner audit callback.

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
Leaving commanded mode removes its mode-specific state. Target selection resumes
through the native picker/allowed-selector set (0x11BF60 / 0x11BE00) and ordinary
threat scoring (0x11C9AC / 0x2FA99C).

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
