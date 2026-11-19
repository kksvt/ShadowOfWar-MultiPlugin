# First live external stimulus capture

Console attachment: 2955e9c1-6a77-4465-bac2-79a9fae1cdf1.
External capture: CommandStimulus-20261118T200141Z-23580.jsonl, read from the
user's diagnostics directory. 78 complete samples were available when analyzed.

The first eight samples, world time 139.805 through 142.191, show target actor
0x57FB1A0 with valid actor/stats links, health 3134.114 through 3142.175,
stats+0x4A0=0, actor+0x2F19=1, and both persistent stimulus handles zero.
All eight registry snapshots are complete-best-effort with no target-effective
stimulus match. Envelope counts vary from 22 to 26; the whole registry is not
empty. The focused source 0x58014B0 has +0x2F19=0 over those samples.

At world 142.553 target health is zero, stats+0x4A0=1 and actor+0x2F19=0.
Subsequent samples mostly describe the post-death state or stale focus while
world time is paused; they must not be counted as continuing live-target stalls.

The console trace records a native ranged command resist variant and an observed
immediate attack start for the focused source. Other nearby sampled followers
have attackable=0. This is not proof that the focused archer is the actor the
user visually saw circling; it does correlate target state with the same command.

The missing persistent stimuli and suppression flag precede death, so this
capture provides useful new evidence. The state matches the known +0x2F19
stimulus-clear path and does not implicate stats+0x4A0 before death. It does not
identify the caller that set +0x2F19 or prove the transition was erroneous.
In particular synchronized/command attack victim handling is a plausible next
static investigation, not a confirmed explanation. Forcing the target active
could violate native attack synchronization and is not justified by this sample.

No plugin or capture-script code was changed for this analysis.
