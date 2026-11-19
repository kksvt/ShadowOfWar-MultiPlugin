# Second stimulus lifecycle investigation

This is static research against the inspected executable, plus an audit of the
v10.20 observer's identity matching. No plugin source changes, runtime writes,
native game calls or new gameplay tests were performed.

## Findings established in this pass

### Actor stimulus removal has concrete state switches

Assembly for RVA 0x2F63F8 reads actor+0x26A8 (stats) and returns false when a
non-null stats object's +0x4A0 byte is nonzero. Actor stimulus updater 0x440F80
responds to false by clearing both persistent stimulus handles via 0x4410E0.
This establishes an upstream suppression mechanism independent of distance,
fight-circle permission and the follower's line-of-sight preparation result.
The semantic name and writers of stats+0x4A0 remain unresolved.

Wrapper 0x440F48 selects 0x4410E0 directly when actor+0x2F19 is nonzero;
otherwise it runs 0x440F80. Actor transition routine 0x4428B0 sets +0x2F19,
removes the actor from a native AI actor list and invokes this wrapper. This is
an actor lifecycle/state transition, not a measured global population ceiling.
The transition's precise gameplay name is not established.

0x440F80 also clears stimuli when the actor's virtual method at vtable+0x108
returns true. Under global byte module+0x2A34C0D it additionally checks actor
or linked actor +0x2700. These branches must not be blindly forced past: the
virtual predicate and global switch have not been semantically identified.

### Recreation is driven by actor state updates

0x44CEB8 compares a cached actor state with 0x44D02C and calls 0x7AA29C on
change. 0x44CF1C similarly compares a cached predicate derived from 0x171A78.
0x7AA29C chooses to clear or update stimuli based on actor+0x2F10 and that
predicate. 0x171A78 tests a state resolved through actor+0x2FD0 as well as a
counter at +0x2F0C. These are exact reads, not proven death/invisibility names.

0x44D02C tests actor+0x1198 and +0x2C34. 0x44D008 tests +0x2C38 and
stats+0x67F through actor+0x26A8. The latter is cached in the same update but
is not independently shown to trigger stimulus recreation there.

This narrows a missing-stimulus investigation to actor state transitions and
record selection; the actor updater should not be described as a periodic
automatic repair of every missing registry record.

### Command binding itself can refresh stimulus selection

Xrefs show commanded-target setter 0x1A8DC08 calls 0x440F48. Thus entering or
leaving native command context can affect the commanded follower's own emitted
stimulus configuration. This does not establish that it removes the enemy
captain's stimulus, and the source and target sides must remain separate in
diagnostics. A single missing target-pair counter cannot identify which actor
underwent a lifecycle transition.

### Actor handles are linked weak references

0x44112C unlinks an old reference and invokes 0x43FEE8 to link the new one.
The value is at reference+0x20. 0x43FEE8 inserts into a list rooted in the
stimulus object. Envelope removal reaches its owned object's virtual destructor
through 0x6BC54C. A stale-handle explanation is not established simply because
0x441178 returns an existing record-matching handle: this is a native reference
mechanism, not an untracked raw cache. Full destructor invalidation remains to
be verified before claiming stale handles are impossible.

### The current target matching covers the native actor-kind switch

0x4403B4 selects stimulus+0x48 for kind 1 and +0x70 for kind 2. The v10.20
reader follows those branches. Some actors are routed through 0x440790 by a
runtime-type predicate at 0x440478 rather than ordinary 0x4404B0 preparation.
That can explain missing *ordinary preparation* counters, but not by itself
zero matched gate calls: gating and dispatch precede the preparation branch.
Other stimulus kinds use a generic path and are intentionally not classified
as ordinary actor-target jobs by the current reader. Their relevance is not
proven by source-wide activity alone.

## Effect on the working hypotheses

The final capture's dominant failure still has no captain-matched gate calls,
but source-wide calls continue. A late different follower refreshes against the
same captain. Therefore permanent disappearance or permanent untargetability
throughout the whole command is not established. A transient lifecycle change,
record replacement, or an observation/identity gap remains possible.

The native path is now narrower than a generic "AI is overloaded" explanation:

1. Determine whether the captain has its persistent handles and corresponding
   live stimulus envelopes during the failure.
2. Capture stats+0x4A0, actor+0x2F19/+0x2F10, selected stimulus record and
   actor liveness/health alongside registry state.
3. If missing, attribute removal/recreation through the actor updater and
   unregister caller; if present, audit matching against the raw envelope and
   actual kind before blaming gate evaluation.

Forcing attackability would bypass a flag that can legitimately be false after
these state changes. Forcing CircleTarget cannot make an absent target stimulus
reappear. Neither is justified as a causal fix from the current evidence.

## Research limits

No captured runtime values exist for the newly identified switches. Static
inspection cannot tell which changed during the reported incident. The root
cause remains unproven. This report deliberately does not assign speculative
gameplay names to the state flags or claim that unregistering is itself a bug.
