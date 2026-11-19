# Grunt behavior and commands

## Group attack

Hold Left Shift+X to aim; release X to command eligible nearby friendly Orc grunts
against one live enemy. Every participant is dispatched immediately. Sources must
be within 30m of the player and 40m of the target. Captains, enemy sources, busy
native buddies and incompatible setups are excluded. Ologs join rally only.

Prefer a unique validated native outline. Otherwise use a camera-ray cone:
14 degrees to acquire, 18 to retain, up to 40m. Body/head samples compensate for
actor origins. This fallback is geometric, not a full occlusion test. Enemy
awareness/player HasThreat does not prohibit the separate-key command.

The cache/order array is bounded to 256. Health, allegiance and actor/stats links
are revalidated; duplicates are excluded. Per-unit registration waits are three
seconds, followed by a fifteen-second command watchdog. Temporary
SetBuddy_Bodyguard_DefaultOrc items are removed only when plugin-owned; compatible
available native entries can be borrowed. Reacquire entries after registry writes
because the native vector can relocate. New requests wait for the current group.
A shared Combat_Marked marker identifies the target, not proof of a hit.

## Full eligibility override below 2m

forceCloseCommandAttack=true detours the entire native candidate-eligibility
function at RVA0x37A2C4. A valid friendly commanded Orc grunt in its own or
unrestricted command context, strictly below 200cm from the same live enemy,
receives score1.0 and success without running that gate. Distance is 3D actor
origin distance; exactly 2m does not qualify.

Plugin scope still requires live source/target, correct allegiance, valid stats
links, Buddy_Commanded role, active order and compatible context. Within scope,
all restrictions inside the native gate are bypassed, including readiness,
availability, current-target, combat-state and animation gating. Stored actor
flags are not rewritten. Outside scope the original arguments/result/output are
preserved; a missing score output does not get forced success.

This is **not every check in the engine**. Earlier role admission and downstream
attack selection, path, animation and damage processing remain. Subsequent testing found that it does not reliably eliminate circling. Grunts already fighting someone else can still fail to
approach/prioritize the selected target; there is no forced global retargeting.

sow_close_command_attack.hpp validates the entry signature before installing.
Startup reports full close eligibility override installed. Heavy gate tracing is
not installed simultaneously on that function. Current defaults leave
traceCommandCrowding and ignoreCommandedGruntsForAttackLos false; inactive research
modules are not prerequisites for the close override.

## Scoped target-cooldown bypass

bypassCommandTargetCooldown=true installs sow_command_target_cooldown.hpp at
RVA 0xED7420. The original predicate runs first. Only a native rejection of
AI_Buddy_Command_Cooldown can become success, after validating the predicate's
linked cooldown with native getter 0xEC0040 and the exact active ordered pair.
Source/target stats and actor links, health, allegiance, Orc-grunt identity,
command flag/target, registry owner, Bodyguard_DefaultOrc setup and mode 2 must
all match. Other cooldowns and untracked pairs retain the original result.

This bypasses that record's availability predicate, including its timer/active
blocking check. It never clears or edits the target's cooldown table. It does not
force an actual attack or bypass the other planner, path, animation, resistance
or synchronization checks. No additional distance restriction is imposed on this
predicate; the active command's existing source/target selection scope still applies.

Normal builds retain target priority: stall/enrage recovery is compiled out.
forceCloseCommandAttack remains true. The cooldown bypass has been observed
changing native rejections to success with groups of up to 18 active orders.
This confirms hook operation, not a hit for each evaluation or a complete fix.

The corrected v2 context reader uses predicate-context +0 -> selection-context +0
for source stats, and predicate-context +0x10 for target stats, verifying its
+0x08 target descriptor. The earlier direct-source reading was incorrect.

Startup reports command target cooldown v5 installed. commandCooldownDiagnostics
(true for this investigation) prints at most seven summary lines/second with the planner trace during orders to the
debug console and %TEMP%/SoW-GruntCommand-active.log. It distinguishes predicate
calls, exact-record passes/denials, bypasses and scope rejections, plus bounded
nearby planner state and a read-only slot/perception sample. These counts are evaluations, not hits or distinct followers.
The detailed interpretation is in the research report. Install() without an
argument retains diagnostics for compatibility with the preceding hooks.cpp.

sow_command_perception_audit.hpp reads the inline native 256-slot occupancy array,
validates source-row/pair identity, and reads both evidence flags and their timing.
The sample is for the nearest tracked pair, not all followers. pair=ready is
required before interpreting evidence fields; missing/unreadable fields are not
evidence of a cleared native flag. slotsUsed=256 alone is not a failure if the
sampled actors both have valid slots and a matching pair. pastExpiry=-1 means
timing cannot be interpreted; 1 means the threshold was passed, not proof the
engine should already have cleared it (refresh46 and pair type also matter).
The world clock is read directly through the chain used by native 0x2FE5A8.
No additional native calls, hooks or writes are used. Set
commandCooldownDiagnostics=false after testing to remove the sampling callback.
Compare pair flags with cached AI flags only when selectedMatch=1; otherwise the
AI may still describe a different selected target. Sampling is not an event trace.

### Focused query-boundary trace

traceCommandPerceptionQueries=true adds six signature-checked observation hooks,
installed only with commandCooldownDiagnostics and the cooldown hook active.
Startup must also report perception query trace v2 installed. Optional trace
failure retains existing command behavior and the slot audit. Set this option
false to remove the six observation hooks on the next rebuild/restart.

The extra perception query summary follows one source/target until its tracked
order ends, preferring a nearby non-attackable source when selecting a new focus.
It includes focused pair flags so it can be compared independently of the nearest
follower shown in the other samples. Counters reset when focus changes, identified
by focus and batch. The first sample establishes the focus and normally has zeros.

| Counter | Native boundary and meaning |
| --- | --- |
| dispatch | 0x123BF4, stimulus dispatch for the focused pair |
| queued | Increase in its native 312-byte job-vector count across that dispatch |
| queueUnknown | Invalid/changing vector bounds; append count unavailable |
| prep2 / prep3 / prepOther | 0x4404B0 result; 2 rejects preparation, 3 follows the query-preparation/special-case path |
| delivered | 0x123B34, queued work reaches native result processing, before its callback gates |
| classify | 0x64C668, visual perception classifier ran; old/new/progress are native outputs |
| transitionReject | Native callback condition would reject old=4/new!=4 or old!=0/new=0 |
| refreshCalls | 0x43F4E4 was called; actual stored evidence is checked separately |
| droppedGlobal | Counter updates skipped because the plugin trace lock was busy |

These are boundary counts, not matched job IDs, ray-hit classifications or exact
queue latency. Dispatch/queued work can include other stimuli for the same pair.
prep3 alone does not prove a physical ray was submitted. Delivered work with no
classification narrows the gap to native callback/identity gates; it does not
identify which gate failed. A transition rejection is a state-classification
result, not proof another follower blocked a ray. Work queued before focus began
may finish inside the observed window. No observations prove an event never ran
outside that window, especially when droppedGlobal increases.

Originals run exactly once. Hooks record only plugin counters under nonblocking
try-locks; logging remains on the existing bounded player-update audit. There are
no native calls added by the trace, game-memory writes, or changed decisions.

### Pre-dispatch eligibility and representation checks

The additional native gate at 0x1225D8 is observed without replacing its result.
It receives listener metadata and a 160-byte envelope, not the raw stimulus.
Metadata+0 identifies source actor; envelope+0x90 points to the stimulus. The
native actor-target selection at 0x4403B4 uses stimulus+0x48 for kind 1 and +0x70
for kind 2. The old trace matched only +0x48, so its zero dispatch count did not
fully exclude a matching blind spot. The revised trace handles both known kinds
and also counts all activity for the focused source, independent of target.

The perception eligibility line contains:

- gateSource/sourcePass: all gate checks for this source, and native passes.
- gatePair/pairPass/pairFail: checks whose kind-selected actor matches the order.
- sourceDispatch/sourceQueued/sourcePrep/sourceDelivered/sourceClassify/sourceRefresh:
  activity for any target, allowing unmatched representations to be spotted.
- lastKind/rawA/rawB: latest source stimulus representation; these can describe
  an unrelated stimulus, not the stored failed-gate detail.
- detail: at most one post-call snapshot/second after a matched native rejection.
  Priority, actor/target mode, callback availability, relation value/mask, height,
  pending bit or interval can explain simple branches. Dynamic relationship
  overrides and unsupported/unreadable cases remain unresolved. snapshot-passes
  means the sampled simple checks pass despite the observed native rejection;
  it is not a forced native success or an exact branch trace.
- sensing/sensingInterval/nextUpdate: source actor's native scheduling fields,
  read once per summary, with a separate sensingRead validity flag.

The gate's relationship cache lookup and bitmask are read directly; no additional
native getter is invoked. Snapshot timing/version/pending fields come from the
same offsets the native gate tests. Counts can include debug eligibility passes
and are not unique requests. All fields are bounded observations, not proof of
why an unobserved query never ran. No canonicalization of every possible actor
alias is claimed.

The fixture uses the native predicate constructors, including their pointer
indirection. Combined source and targeted scope/fallback/regression fixtures pass. Live testing reported improved engagement, but residual circling occurred while
nearby followers had attackable=0. Its underlying cause remains unresolved.

## Stalled-order fallback

Define SOW_ENABLE_COMMAND_RAGE_RECOVERY in project preprocessor definitions to
compile the fallback. Without the macro, the header contains no implementation
and hooks.cpp has no recovery installation or options. With the macro,
releaseStalledCommandedGrunts=true enables it by default;
enrageReleasedCommandedGrunts=true also requests native rage after release.
Both are configured in hooks.cpp. gruntCommandStallMs sets the idle delay in
milliseconds (default 3000). Zero triggers on the first qualifying sample;
thresholds at or above the 15-second order watchdog may never be reached.
Rebuild/restart after changing it. This is an experimental heuristic, not a
universal detector of inability to attack.

Only plugin-owned, issued Bodyguard_DefaultOrc orders qualify. The original
source/target must remain alive with valid links, friendly/hostile allegiance,
native mode 2, and the commanded-target link still matching. Borrowed buddies
and rally orders are excluded. Sampling runs at most every 100ms per follower.

Enter strictly below 150cm and allow drift strictly below 200cm. After the configured idle
delay without a sampled active attack record or positive ordered-pair damage
calculation, transition through native mode 1, verify exit, and remove only the
plugin-owned command item. Active attack/reaction records, the native command
blocker, state-record flag +0x66, unreadable state, progress, distance exit or a
sample gap over 500ms reset the timer. Damage calculation is supporting progress
evidence, not proof of a committed hit. A short attack between samples may be
missed; an attack that retains a nonzero record indefinitely suppresses fallback.
The exposed state gates are not an exhaustive catalogue of incapacitation.

Rage uses Combat/Buffs:AI_Rage_NonCatalyst, the Enrage Allies category's native
effect. Native application is RVA 0x36A35C(stats, buff, null source, zero flags),
also used by 0x37DD48 and 0xEFFB60. Existing rage-family effects are not refreshed.
The engine owns expiry: Trait_RageDuration_NonCatalyst returns 5 seconds for
IsNotFactionMember, otherwise 10, with an EndlessRage multiplier. There is no
plugin timer that might cancel a later legitimate native refresh, and no new
persistent inventory item or captain trait.

Buff lookup derives the Combat/Buffs table from the verified Chameleon affix
Buff link, then resolves the exact rage record. The native field getter supports compiled and compressed record storage; its
Buff schema hash/type are verified first. Failed lookup or signature mismatch
leaves release-only behavior. When the signature is valid, unresolved lookups
retry every two seconds on the existing player update callback. The
startup line explicitly distinguishes release + native rage from release only.
Aggregate recovery totals are logged at most once per second when changed;
per-follower detail follows the normal disabled-by-default command diagnostics.

The native effect changes morale and combat playbook/roles. Live testing confirmed rage application but did not establish a reliable cure
for circling.
Clearing an order releases target priority, so ordinary AI may choose another
enemy. The fallback does not make distant engaged grunts switch targets.

## Aggression and rally

Aggression covers allied/enemy Orc grunts with matching actor/stats links, no
player marker and registration index exactly -1. Newly assigned
I_Am_Undead_WalkOnly is redirected to I_Am_Undead_Fearless while retaining the
base undead identity. WalkOnly adds movement restrictions/animation tags absent
from Fearless. Existing inventories are not retroactively swept.

Positive post-play cooldowns are multiplied by0.4 only for validated all-grunt
stages; empty/mixed/malformed groups retain native timing. This does not speed up
attack animation or globally expand simultaneous attacker capacity. The inventory
add hook preserves all eight native arguments.

Z uses native follow for friendly Orc/Olog grunts within30m, without bodyguard
registration or teleport setup. Existing/queued/script-owned tasks and busy buddies
are preserved. Tracking ends at4m, after20seconds, on destruction/identity changes,
or changed ownership. ClearFollow runs only for the owned assignment with no other
queued task. Followers markers show confirmed participants; missing markers do
not prevent following. Rally cancels tracked custom attacks.

## Native layout and paths

Addresses are RVAs. Positions are centimetres; camera coordinates are Y-up.

| Location | Meaning |
| --- | --- |
| Stats+0x2E0 | Combat object; distinct from stats+0xA8 target link |
| Combat object+0x2E8 | Actor; actor+0x26A8 resolves stats |
| Combat object+0x310 | Current target stats |
| Combat object+0x340/+0x348 | Owned override contexts, first nonzero wins |
| Override+0xA8 | Playbook record |
| Combat object+0x370 | Participant record |
| Combat object+0x43D/+0x43E/+0x43F | Readiness/availability inputs, role-dependent |
| Actor+0x2E90/+0x2EB8 | Command flag / commanded target stats |
| Global0x26FFC30, bounds+0/+8 | Buddy registry, stride0x90 |
| Entry+0/+8/+0x18/+0x80 | Actor / setup / mode record / mode state |
| Modes0/1/2 | Inactive / available / commanded |
| 0x1AA3784 / 0x1AA4B90 | Registration / mode(manager,entry,mode,player,targetStats) |
| 0x1AA49D0 / 0x1AA51BC / 0x1A9DA00 | Blocker / commanded update / target-path check |
| 0x11D618 / 0x11DA18 / 0x37A2C4 | Play picker / role admission / final eligibility |
| 0x425C3C | Allocation; count+0x10, source stats+0x38+i*0x90 |
| 0x3778D4 / 0x728980 | Play update / timer setter; post-play caller0x377B1C |
| 0x1C1224 | Inventory add; owner stats at inventory+0x440 |
| Actor+0x2F90 -> +0xA8 | Camera actor, position+0xAC, XYZW quaternion+0xB8, forward+Z |
| 0x1A8DD04 / 0x1A8B6CC | Follow / ClearFollow |
| Actor+0x2D10; component+8/+0x10/+0x18 | Follow component; band / queued bounds |
| Band+0x28/+0x70 | Leader/type; OrcBuddyFront in table 0x2702420 |
| 0x60CE2C / 0x3576A4 / 0x357C2C | Marker creation / type / bind stats+0x48 object |
| 0x2F47B0 / 0x4EE4C8 | Marker enable / destroy holder |
| 0x427A8C / 0x360988 | Player update/input / actor cleanup |

## Ownership and siege integration

The command override owns a per-source context. Buff application0x452ED8 invokes
0x1A6565C through stats+0x2E0, constructs data via 0x19BF98 and registers it through
0x37CC84. Removal0x1A65B94 verifies the owning record before0x2C07E0; +0x348 has
cleanup0x2C0780. Other contexts normally exclude the reserved source. A reservation
mismatch alone is not stale-pointer evidence. Live exits confirm native cleanup.

The siege exemption redirects only the call at 0x1AA4A02 to getter 0x440F68. It
exempts verified friendly sources under readiness evaluation/active tracking,
without changing the global siege flag or other callers. The unusual native
register contract requires a gateway, not a normal C++ passthrough. It preserves
GPRs, flags and legacy FP/XMM state with unwind metadata, not AVX upper lanes.

Input/lifecycle work uses the existing player update, not a keyboard thread.
Actor cleanup invalidates tracked pointers. Native navigation, immunity/resist
reactions and other priorities still apply. Unhook only with quiescent detours.

## Native circling investigation

The v10.23 diagnostic build enables `prioritizeCommandTarget=true` and
`traceCommandActivation=true`. The former promotes the owned victim in the
transient native actor-selection list, preserving global budgets and the native
cutoff. The latter observes reactivation requests, both queued callbacks, handle
resolution and final activation. See [the diagnostic guide](COMMAND-ACTIVATION-TRACE.md).

Older cooldown/perception/planner summaries are disabled in this build. The new
trace buffers events in detours and emits changed target summaries from player
update, at most once per second per watched identity. Stable state is quiet;
recent identities remain tracked for 60 seconds after command ownership ends.

Live captures distinguish two conditions: a victim suspended by native selection
with no reactivation pending, and an unsuspended victim still marked pending.
The scheduler's class helper promotes actors with a commanded target, without
automatically promoting that victim. It sorts candidate class before other
ranking fields, making crowded group commands a plausible source of victim
starvation. Exact transient rank and causal transition sequences remain unproven.

Known/visible and attackable are different cached perception flags. Three close
followers can retain the correct victim while native attackability is false.
Passing the existing cooldown or close-candidate gate does not satisfy every
later planner condition. Native suspension removes persistent visibility stimuli,
which fits the absent evidence in the captured cases.

The priority trial has successful selection-retention samples but is not a
general circling fix; the pending-reactivation case still needs tracing. A
visually circling follower may be approaching, waiting or cheering without
running an accepted literal CircleTarget planner. Do not infer its exact leaf
from movement alone.

The 256 perception slots are distinct from the 256-entry native actor-selection
table and its active configuration quotas. Neither number is a demonstrated
safe population cap. Large wight/revenant populations may still break AI.
No global budget change, direct flag recovery or new gameplay bypass is part
of the activation diagnostic.
