# v1.2 selection capture: final command

Console attachment: 273d5628-0128-414e-8b30-915639368c48.
External JSONL: CommandStimulus-20261118T201955Z-17992.jsonl.
409 complete samples across six batches were read. Analysis of a target's
command stops at its first death; later samples may contain reused addresses
and must not be interpreted as that captain returning to life.

## Final batch 6 / focus 11

Target actor 0x23538AC70, focused source 0x2335164D8.

| World seconds | Target health | Selection level | Suspended | Target stimuli |
| --- | --- | --- | --- | --- |
| 302.815 | 9418.48 | 0 | 1 | No matching target stimulus |
| 303.324 | 9418.48 | 1 | 0 | Persistent and combat-related stimuli present |
| 304.844 | 9428.85 | 1 | 0 | Persistent and combat-related stimuli present |
| 305.334 | 9428.85 | 0 | 1 | No matching target stimulus |
| 309.865 | 9450.22 | 0 | 1 | No matching target stimulus |
| 310.134 | 0 | No selection entry | Actor flag reset | No matching target stimulus |

The selection snapshots at these transitions are complete-best-effort. The
305.334 and subsequent pre-death registry scans are also complete-best-effort.
The focused source remains unsuspended throughout the pre-death window. The
target's +0x2F19 follows the selection suspension flag, +0x2F18 follows level,
and selected target stats clear to zero while suspended. The target is still
alive with over 9400 health during the extended final suspension.

This is live evidence of the selection/suspension state accompanying stimulus
loss, not just an assumed link from static analysis. It does not record the
policy caller or ranking decision that caused the suspension. An active budget
configuration alone is not proof that numerical quota exhaustion triggered it.

The active configuration is named Assault. Raw ordinary runtime fields read
MaxAmbientAI=160, MaxHighPriority=5, MaxMedPriority=5, MaxLowPriority=5 and
MaxSimulatedAnimations=75. These are stored fields, not a guarantee that every
native effective getter or override yields those values. The capture does not
justify calling the total active combat limit exactly 15.

The console records initial PullRiderOff_Warg_L activity and later a ranged
command attack start. This reinforces that multiple native behaviors occurred;
the user-visible circling should not all be attributed to a single planner leaf.
The perception pool is at 256/256 in the final samples, but sampled source/target
pairs still resolve. Occupancy alone does not prove allocation failure caused
this suspension.

## Captain choosing another enemy

Earlier successful command windows also show a target-selected-stats value
different from the focused follower's stats. This is a counterexample to treating
that mismatch alone as the cause. During final suspension the captain's selected
target becomes null, so its lack of targeting may be a consequence of suspension.
The observations do not exclude other target-choice effects on native combat.

## Next step

No longer stall capture is required to establish this transition. A target-scoped
trial preserving native AI activation/priority during an owned command is now
better motivated than further broad attack permission bypasses. Research the
native priority/exemption mechanism and restoration/ownership before implementing
such a trial; do not directly clear +0x2F19 or increase all global budgets.
No plugin or capture-script code was changed while interpreting these logs.
