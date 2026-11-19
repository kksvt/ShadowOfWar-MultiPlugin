# Actor selection and missing captain stimuli

This pass followed the native setter of actor+0x2F19. It changes the leading
hypothesis from synchronized ranged-victim handling to actor selection/AI budget
policy. No plugin or capture-script code was changed.

## Verified caller chain

- 0x4428B0 sets actor+0x2F19=1, removes the actor from the AI actor list, and
  clears its persistent stimuli through 0x440F48.
- Its only ordinary direct code caller found is 0x3F6004. This works through
  per-actor manager entries, saves/restores actor state, and accepts a boolean
  controlling suspension/recovery. It is not itself an identified attack event.
- 0x3F6004 is called by 0xF2FF00 and wrapper 0x3F5FDC. The wrapper is reached
  from 0x3F5F0C, whose callers include actor selection routines 0x5C1BC8 and
  0xF2D5D0. These are static callers, not a recorded runtime call stack.
- 0x5C1E0C builds candidate records from 256 native actor slots, sorts them and
  computes a cutoff using the sum of three configuration getters. It passes
  the partitioned candidates and configuration to 0xF2FF00.
- 0xF2FF00 applies selected states through 0x3F6004. One observed branch derives
  the suspension boolean as candidate state != 2.
- 0xF2D060 computes candidate distance and camera-frustum-related tests, along
  with actor-dependent state and ranking flags. 0xF2DBD0 sorts by candidate
  state/flags and distance. This is not simply a raw actor-count cutoff.
- When the selection system's controlling byte is off, 0x5C1BC8 iterates actors
  and applies state 2 with suspension false. This supports treating the path
  as AI activation/selection policy rather than a ranged attack implementation.

The 256 candidate-array bound here is separate from the configurable selection
budgets. It must not be confused with the independently mapped 256 perception
slots, nor interpreted as the number of fully active combat AIs.

## Database corroboration

Shipped records in AI/Count/Configuration contain MaxAmbientAI,
MaxHighPriority, MaxMedPriority, MaxLowPriority, MaxSimulatedAnimations,
RunSimulation, PrioDefinition, PrioTokenDefinition and MaxLODConfiguration.

For example Default has ambient 100 and priority values 10/10/30; Assault has
ambient 160 and priority values 5/5/5 plus MaxSimulatedAnimations=75. These are
database values, not confirmed active configuration values in the captured
session. In particular, this does not establish a live 15-actor limit there.
The exact mapping of the three getter descriptor slots to named fields has not
been independently finalized, so do not hard-code a budget patch from these
examples. The getters' fallback defaults are 15 each; configured values can differ.

## Relation to the live capture

The external capture shows the living captain with +0x2F19=1, null persistent
stimulus handles and no target-effective envelope before death. The source is
not in that state during those samples. This is consistent with the native
selection/suspension path and supplies a concrete reason why a present, living
captain can stop feeding perception without a failed line-of-sight job.

We have not captured the captain's selection entry, ranking, active configuration
or transition caller. We therefore cannot yet conclude that actor crowding
caused the transition, that the policy is erroneous, or that the ranged attack
caused it. The attack database categorizes the observed ranged resist variant
as a synchronized action, but that alone does not connect it to this setter.

## Next discriminating evidence

Capture the captain's actor-selection entry, desired/current state, active
configuration record and relevant priority/token state alongside +0x2F19 and
the existing stimulus snapshot. Determine whether the policy intentionally
demotes a commanded target, whether a forced actor state is active, and whether
it restores full AI when conditions change.

If that confirms budget demotion, investigate an owned temporary priority or
selection exemption for the commanded target. Raising all global budgets or
clearing +0x2F19 directly would affect much more than the failed order and can
bypass required cleanup/restore work. Neither is implemented or recommended
without the selection-state evidence.
