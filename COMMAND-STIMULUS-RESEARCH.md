# Upstream perception research, 2026-11-18

This follows the v10.20 capture analysis. No gameplay source, hook installation,
or game-memory write was made during this investigation. Addresses below are
RVAs for the inspected build. Static paths are established; which path occurred
in the captured incident remains unresolved.

## Dispatch inputs, not just dispatch rejection

0x122354 builds a due-listener vector, then walks every active stimulus envelope
and tests it against each due listener through 0x1225D8. Accepted pairs call
0x123BF4. At the end it processes listener work through 0x43F5FC.

The due-listener vector is reset at 0x6DF464 and appended at 0x690DFC. Its entries
are 0x40 bytes, and its allocation grows when capacity is reached. It does not
impose an additional fixed 256-listener cap. The separate actor-pair allocator's
256-slot limit remains real.

Listener eligibility 0x6920B0 checks actor+0x23BD, bit 0 of +0x2F1C, and +0x2F19.
The scheduler requires a positive interval at actor+0x2DD8 and a due timestamp at
+0x2C68. Getter 0x114BD18 reads +0x2DD4 before queueing the listener. These are
observed field conditions; semantic names for the other flags are unresolved.
The captured source processes other stimuli, so total listener inactivity does
not explain that sampled window.

The active stimulus manager is global module+0x26FFC38. Its begin/end fields
at +0/+8 delimit 0xA0-byte envelopes. This layout is separate from both the
0x40-byte listener vector and the per-source 0x138-byte job vector.

An absent captain envelope produces zero captain-specific gate calls even if
the follower's listener is scheduled. Thus the existing zero-call counters do
not establish gate rejection, CPU starvation or missing line of sight.

## Removal before the gate

Assembly at 0x12359D..0x12368E verifies pre-dispatch housekeeping:

- Envelope+0x80 is a double deadline. A nonzero value older than world time
  takes the envelope-removal path.
- Envelope+0x7D and +0x7E enable required-reference checks. They test stimulus
  weak-reference values +0x48 and +0x70, respectively. A cleared required value
  removes the envelope. A null reference is not itself proof of actor death.
- For kind 1/2, the corresponding live actor position updates stimulus+0x78.
- 0x123B14 clears the pending byte in all 256 per-listener cache entries at
  stimulus+0x90+i*0x10+0xC when its native refresh condition permits. This is
  not a function that grants attackability.

0x43E2CC removes an envelope by moving the final envelope into its slot and
shrinking the vector. Addresses and indices can therefore change; a diagnostic
must identify records by actor/reference and stimulus identity, not retain an
envelope address across frames.

Explicit unregister 0x43E254 removes immediately or queues removal when the
manager is updating (+0x4C). 0x575678 also removes positive-deadline envelopes
in bulk; its mission-level purpose is not yet established. These are possible
paths, not evidence that any one ran for the captured captain.

## Actor-side persistent stimuli

0x440F80 selects and maintains two actor-owned stimulus handles. 0x4410E0
clears both. Observed values sit at actor+0x21B0 and +0x21D8, with weak-reference
headers beginning +0x2190 and +0x21B8. These offsets come from native calls,
not an assumed Entity layout.

0x440F80 has early conditions that clear these stimuli, including a virtual
actor-state predicate and failure of 0x2F63F8. The exact gameplay meaning of
these predicates has not been proven; they must not be named death checks.

0x441178 preserves an existing matching stimulus, unregisters a mismatched one,
and calls registration 0x1237A0 when a nonzero record is selected. Registration
can reject requests because of actor-state predicates, native duplicate
suppression, relationship conditions for applicable records, or unavailable
record data. The available pseudocode contains compiler-specific register
contracts: do not turn these helpers into speculative normal-ABI calls.

This establishes a real upstream registration/lifecycle layer absent from the
current trace. It does not prove a stale-handle bug or justify unconditional
re-registration.

## What the next capture needs

The narrow next diagnostic is a bounded read-only snapshot of the active
envelope registry for the focused captain, alongside its two actor handles,
source and target health/liveness, selected record, expiry and required-reference
flags. It should distinguish unavailable/incomplete reads from a verified absent
record, revalidate vector bounds, and never retain envelope addresses.

If the captain's envelope disappears while its actor remains valid, observe
registration/removal boundaries and their callers. If it remains present yet
matched gate counts remain zero, first audit target identity/kind correlation
and observation scope. If gates resume but evidence remains false, investigate
preparation, delivery and classification. This sequence distinguishes failures
without another broad permission or CircleTarget bypass.

No additional in-game test is claimed, and no new fix is claimed by this report.
