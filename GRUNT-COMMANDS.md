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

Startup reports command target cooldown v2 installed. commandCooldownDiagnostics
(default false in hooks.cpp) prints at most two lines/second during orders to the
debug console and %TEMP%/SoW-GruntCommand-active.log. It distinguishes predicate
calls, exact-record passes/denials, bypasses and scope rejections, plus bounded
nearby planner state. These counts are evaluations, not hits or distinct followers.
The detailed interpretation is in the research report. Install() without an
argument retains diagnostics for compatibility with the preceding hooks.cpp.

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

[The focused research report](COMMAND-CIRCLING-RESEARCH.md) documents the shared
12-second target cooldown, separate planner permission/perception gates, and why
release plus rage can change targets. Live captures confirm cooldown bypasses and a separate attackability-false
waiting case; the latter has not been attributed to an exact native condition.
