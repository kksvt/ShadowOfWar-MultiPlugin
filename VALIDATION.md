# Validation and remaining limits

## User-confirmed behavior

- Damage multipliers, mask one/two-bar doubling alongside Raise Dead, ring
  enhanced resurrection caps, gear resurrection and elemental grants work.
- The ring ally-hit Might effect was observed working; exact per-hit accounting
  has not been isolated in a controlled test.
- Aggression changes, group targeting, rally including Ologs, participant markers
  and purchased-upgrade multi-equip were confirmed in game.
- The close-command override initially seemed helpful, but follow-up testing
  did not reliably eliminate circling. Engaged grunts can still ignore an order.
- Flagged imported defenders received trait cleanup, Awe, mortal vulnerabilities
  and outgoing calculated-damage suppression; affected warchiefs fled after the
  native morale/behavior cleanup.
- Structural normalization and throne-room prevention succeeded on tested
  unsupported Mystic and tribeless forts. Tested assaults had valid geometry,
  corrected theme and no observed oversized defenders.
- Automatic/manual exports were used for real captures. Recent repair builds
  were reported without noticeable lag spikes; this is not a universal latency
  guarantee.

## Static and fixture coverage

MSVC C++20 warning-as-error syntax checks cover the combined hooks source.
Targeted fixtures cover command scope/range, original fallback ABI and score
outputs, group/rally ownership, structural allowlists, trait/picker capacity and
readback, identity matching, journal validation/atomic writes, and background DLL
worker lifetime. Fixtures establish tested contracts, not every engine state.

The close override skips the whole native candidate function only in its narrow
scope. Outside it, original arguments, return and score behavior are retained.
Heavy command tracing and the experimental LOS bypass are disabled by default.

The stalled-order fallback passes targeted timer, hysteresis, pause/progress reset,
borrowed-registration preservation, native release and existing-rage fixtures.
Full hooks pass MSVC C++20 /W4 /WX syntax checks. Native rage mapping/lifetime
were traced in the shipped database and IDA. A subsequent live capture confirmed nine command releases with the native rage
buff present for all nine. Subsequent attack behavior remains unverified.

The initial cooldown scope fixture had mirrored an incorrect context layout.
The corrected v2 fixture independently models constructors 0x2DA938/0x30D1A4,
rejects that old layout, and checks weak-reference target-value sampling.
The corrected scoped target-cooldown bypass passes exact record/link, active-pair,
mode/registry identity, liveness/allegiance, original-result/ABI and no-memory-write
fixtures. Existing close-gate, group/rally and stall fixtures also pass. Live captures confirm scoped bypasses, including larger groups, but not complete
elimination of circling. A later capture showed sustained attackable=0 despite
correct target tracking and occasional attack permission. No causal producer
condition was identified. Recovery is now compile-time optional, and planner
audit output is enabled only for the current slot/perception investigation.

Read-only perception fixtures cover the last native row/column, full/free slot
counts, invalid indices, pair identity, separate evidence flags, strict expiry
timing, missing/non-finite clocks and unchanged input buffers. The native clock
chain was checked against 0x2FE5A8 assembly. Live slot/perception capture showed
181-184/256 occupied slots, valid sampled pairs, 0.330-second persistence, and
missing/expired first evidence flags during circling. Slot exhaustion did not
explain those pairs; the refresh gap's cause remains unresolved.

Focused observation fixtures verify separate listener/sensor/stimulus layouts,
stable focus and generation changes, native job-vector growth, all native
arguments/returns/output preservation, transition rejection and disabled
forwarding. Six native signatures and boundary call sites were checked in IDA.
Live v1 trace installation and a healthy ranged pipeline were confirmed. Its
stalled pair had zero matched calls, but an alternate target field was not covered.
The revised trace covers both native kinds and source-wide activity. Fixtures
verify metadata/envelope indirection, native return preservation, simple gate
branches, cache version/pending/interval, and unchanged input buffers.
Live eligibility-extension results remain pending.

Full hooks compile with and without SOW_ENABLE_COMMAND_RAGE_RECOVERY. The recovery
fixture explicitly opts into the macro; its existing behavior is retained. Normal
builds omit the recovery implementation and installation.

## Still unresolved

v10.23 activation-trace fixtures verify native request arguments (including the
fifth float), original return values, nested callback correlation, both resolver
failure paths, final activation that leaves flags set, ownership/identity checks,
tracking after command expiry, pending-state edge detection, contention and
stale/ambiguous ticket rejection. Mock game buffers remain unchanged apart from
explicit fake-native writes. Existing command fixtures and full hooks compile
with `/W4 /WX`, both with and without optional rage recovery. Live installation
and callback correlation were subsequently observed in four completed cycles,
with matching lookups and no recorded drops or pending waits. That crowded run
did not reproduce circling; it does not establish a general fix. See
[the trace guide](COMMAND-ACTIVATION-TRACE.md).

v10.21 target-priority fixtures cover owned command scope and exclusions,
temporary list promotion/restoration, unchanged actor and manager data, native
cutoff preservation, duplicate/slot-reuse/malformed input rejection, exception
cleanup and original forwarding when disabled. Initial live samples showed
selection retention and target stimuli; later captures exposed a separate
pending-reactivation stall. Circling also reproduced with the trial disabled,
with native target suspension. This does not establish a general fix.

The v10.20 direct planner fixture checks exact arguments/results for CircleTarget
start/update/end and Attack start/update, nested native permission scope,
untracked-source and stale-epoch exclusion, chronological transitions, failed
admission, native nonzero start-flag semantics, sampled circle deadline, unchanged
inputs and disabled forwarding. New signatures and vtable method mappings were
checked in IDA. Live installation and incident interpretation remain pending.

Native assembly verifies a 256-slot actor allocator, pair-table lookup guards,
and separate cached flags feeding visible and attackable. Visual obstruction and
update starvation remain hypotheses, not verified causes. High population may
still affect scheduling even when the sampled actors have valid slots.

- Large wight/revenant populations can break AI. No safe population limit is known.
- Candidate eligibility is one layer: earlier admission and downstream path,
  animation and attack selection can still prevent an attack. There is no proven
  general fix for switching an already-engaged grunt's target.
- Necromancy has legitimate counterexamples to simple stored-skin checks.
  Slayer/frost-immunity generation is ambiguous. These remain review-only.
- Mortal vulnerabilities do not guarantee instant death from every damage source.
  The damage hook does not cover every hazard, unattributed effect or scripted death.
- Model regeneration can reset stored equipment, colour and look. Preview caches
  may remain stale. Not every exotic model combination has been tested.
- Tribe correction prevents tested bad throne-room construction; it does not
  rebuild missing geometry inside an already-running assault.
- Journal inheritance across a real invasion reload with the same Cloud ID still
  needs controlled verification. Rejected ambiguous identities are intentional.
- Hacked Online Vendetta coverage has static/fixture support, not live end-to-end
  confirmation. Exports are synchronous and may briefly pause the game.
- Hooks require the inspected game build and quiescent lifecycle operations;
  arbitrary hot unload is unsupported.

## Useful feedback

Record the build/configuration, observed source and target, mission/capture stage
and behavior. Attach an automatic export and, when useful, a post-load F8 snapshot.
Short self-resolving command hesitation is still useful evidence; no minimum
stall duration or exact ranked-fort replay is required.
