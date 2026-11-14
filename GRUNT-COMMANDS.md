# Grunt behavior and commands

Aggression changes, group attacks, rally, Olog rally and participant markers
have been confirmed working by the user. The current controls are Left Shift+X
for targeting/attack and Z for rally; key configuration remains in `hooks.cpp`.

## Aggression and locomotion

Allied and enemy Orc grunts qualify through a readable `Orc_` type, matching
actor/stats pointers, no player marker and registration index exactly -1.
Captains, player, wights, Ologs and beasts do not qualify for this module.

`I_Am_Undead_WalkOnly` inherits `I_Am_Undead_Fearless` but adds a passive buff
with `LimitRawMoveMag=0.5` and an undead animation tag. Fearless has neither
restriction. Newly assigned WalkOnly items are redirected to Fearless for
qualifying grunts, preserving the separate base `I_Am_Undead` identity item.
Existing items on already-loaded actors are not scanned or replaced.

Native inventory addition `0x1C1224` exposes owner stats at inventory `+0x440`.
The hook preserves all eight native arguments. Combat play update `0x3778D4`
writes its post-play cooldown through timer setter `0x728980`, return address
`0x377B1C`. The final positive cooldown is multiplied by 0.4 only in the scoped
all-grunt group. This does not accelerate attack animations or increase native
simultaneous-attacker capacity.

Stage participant count is `+0x10`; entries begin at `+0x18`, stride `0x90`.
Live stats in the first participant weak reference are at entry `+0x20`
(stage `+0x38`). Circle `+0x38` is instead the target and must not classify the
attackers. Empty, mixed or malformed groups pass through unchanged. Runtime
traces confirmed 0.010->0.004, 0.100->0.040 and 0.500->0.200 second cooldowns.

## Group attack

Hold Left Shift+X to aim; release X to issue the same target to every eligible
nearby friendly Orc grunt. Participation requires distance within 30m of the
player and 40m of the selected live enemy. Enemy grunts and captain candidates
can be targeted, including aware enemies. Friendly captains, busy native buddies
and different buddy setup types are excluded. Ologs are enabled for rally only,
not this attack-participant filter.

The observation cache and order array are each bounded to 256. Previously
unobserved actors cannot be selected; health/allegiance/bindings are revalidated
at issue time, and duplicate actors are excluded. New group requests wait until
the current group finishes.

Each participant has its own registration/readiness state, three-second wait,
fifteen-second command watchdog and temporary-item ownership. One blocked grunt
does not hold up the others. A temporary `SetBuddy_Bodyguard_DefaultOrc` item
registers a native `Bodyguard_DefaultOrc` entry when needed. Available compatible
native entries may be borrowed, but their pre-existing items are never removed.
Entries are reacquired after registration: native vector relocation can invalidate
cached entry pointers. A shared target marker lasts until the group finishes.

### Target acquisition

Prefer a unique validated native outlined enemy. Otherwise use a camera-ray
cone: 14 degrees to acquire, 18 to retain a displayed lock, within 40m. Camera
component actor `+0x2F90 -> +0xA8` supplies position `+0xAC` and quaternion
`+0xB8`. Quaternion order is XYZW, camera forward local +Z. The camera trace
uses Y-up coordinates and centimetres. Body/head aim samples compensate for
actor origins. The fallback is geometric, without a full occlusion test.

White/red outline color is not proof of dispatch. Live traces established
ordered combat targets, native ranged/stealth attack records and ordered-pair
damage calculations; one 1788.273 calculation matched the subsequent health
drop, and another stealth target reached zero. Native resistance, navigation,
combat arbitration and animations still govern the result.

### Native buddy path

| RVA / field | Meaning |
| --- | --- |
| Global `0x26FFC30`, `+0 / +8` | Buddy registry vector bounds |
| Entry stride `0x90` | Actor-keyed buddy entry |
| Entry `+0 / +8 / +0x18` | Actor / setup / mode record |
| Entry `+0x38 / +0x48 / +0x80` | Target-check-failed / native marker holder / mode state |
| Mode values `0 / 1 / 2` | Inactive / available / commanded |
| `0x1AA3784` | Register buddy |
| `0x1AA4534 / 0x1AA45B4` | Native availability / nearest-player dispatch |
| `0x1AA4B90` | Mode transition `(manager, entry, mode, playerActor, targetStats)` |
| `0x1AA49D0` | Native actor blocker; true form also checks spatial conditions |
| `0x1AA51BC` | Commanded update |
| `0x1A9DA00` | Native target/path check |
| Actor `+0x2E90 / +0x2EB8` | Command flag / commanded target stats |

The plugin calls the mode transition for each chosen actor directly, avoiding
the normal summon action and nearest-bodyguard selector. The native commanded
mode installs its playbook/perception overrides and chooses attacks normally.
HasThreat at player `+0x3914` does not veto this explicit command.

### Scoped fort-assault exemption and register contract

Buddy blocker `0x1AA49D0` calls getter `0x440F68` at `0x1AA4A02`.
The getter reads owner `+0x1D8`, then controller byte `+0x48`. The owner is
reached through global `0x26FFAB0 -> +0x778 -> +0x30`. The controller is tied
to `FortAssault.Setup` through `0x7E0248 -> 0x4A4154`; `0x57580C` writes its
active flag. Other meanings/consumers of that flag are not fully characterized.

Only this five-byte CALL is redirected, after validating the original call,
getter and following instructions. The gateway replays the getter, then preserves
volatile GPRs, flags, x87, MXCSR and XMM0..15 around a scoped eligibility helper.
Native `RDI` identifies the checked actor. The helper can exempt only a validated
friendly actor under plugin readiness evaluation or a live tracked order.
The global siege flag is never changed; both native function entries and other
callers remain untouched. Other native blockers and target checks still run.

The leaf getter preserves registers that some callers reuse, including RCX/R8;
an ordinary C++ passthrough detour is not equivalent to that native contract.
The gateway supplies Windows x64 alignment/shadow space and registered unwind
metadata. It saves legacy floating-point/XMM state, not AVX upper lanes. Owned
call-site restoration precedes freeing gateway memory and unwind tables.

## Rally

Press Z once to recall the plugin's own temporary attack orders and ask eligible
allies within 30m to approach the player. Ordinary allied Ologs of exact type
`OlogHai_Infantry` qualify alongside Orc grunts. Registration index must be -1;
Olog captains, enemy Ologs and graugs do not qualify.

Each confirmed native follow task ends within 4m of the player or after 20 seconds.
It follows the live player rather than a fixed copied position. It is a temporary
approach command, not a permanent escort toggle. Repeating Z clears/reissues
owned rally tasks; an accepted group attack clears owned rally tasks first.

Existing buddy entries, follow bands and queued follow assignments are excluded.
This preserves native squad leaders and scripted tasks. Cleanup requires the same
live actor, component, exact owned band, player leader and band-type record, with
no newly queued task. Changed/queued tasks cause tracking to yield without clearing
them. Destroyed/dead actors receive no native follow-cleanup call.

| RVA / field | Meaning |
| --- | --- |
| `0x1A8DD04` | NPC.SetFollowLeader `(actor, leaderActor, bandType)` |
| `0x608A28` | Guarded native band assignment implementation |
| `0x1A8B6CC` | NPC.ClearFollow `(actor, true)` |
| `0x361548` | Component clear; also processes queued tasks |
| Actor `+0x2D10` | Follow component |
| Component `+8 / +0x10 / +0x18` | Current band / queued bounds |
| Band `+0x28 / +0x70` | Leader actor / band-type record |
| `0x608E28` | Find band by leader/type |
| `0x2702420` | BandTypes table; lookup `OrcBuddyFront` |

`OrcBuddyFront` links FollowLeader band data to FollowPlayer movement data.
Rally calls the native follow API directly without registering bodyguards or
enabling their teleport-to-catch-up setup. Native navigation and priorities remain.

## HUD markers and lifecycle

`Combat_Marked` supplies the shared attack-target marker. `Followers` supplies
per-participant rally icons from `followers.tex`, bound to Torso with no input
prompt. A rally icon is created only after assignment is confirmed, then destroyed
when tracking ends. Missing marker records/allocation do not prevent following.
Native marker handles are never taken over.

| RVA | Marker operation |
| --- | --- |
| `0x60CE2C` | Create unique marker holder |
| `0x3576A4 / 0x357C2C` | Set type / bind target stats `+0x48` object |
| `0x2F47B0 / 0x4EE4C8` | Enable / destroy unique holder |

Player update hook `0x427A8C` performs input and lifecycle work; actor cleanup
`0x360988` invalidates observations/orders. No keyboard worker thread is used.
The current log is bounded to 2048 lines per installation; per-actor attack traces
and rally distance samples distinguish assignment from movement or completed hits.
