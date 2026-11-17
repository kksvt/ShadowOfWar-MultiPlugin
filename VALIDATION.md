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
audit output is disabled by default.

Full hooks compile with and without SOW_ENABLE_COMMAND_RAGE_RECOVERY. The recovery
fixture explicitly opts into the macro; its existing behavior is retained. Normal
builds omit the recovery implementation and installation.

## Still unresolved

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
