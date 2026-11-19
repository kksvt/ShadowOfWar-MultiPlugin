# Static research stopping-point assessment

Final targeted pass: state-field immediate searches; actor activation/recovery;
stimulus envelope construction; owned stimulus virtual destruction; linked weak
reference invalidation. Plugin source remains unchanged.

## Additional verified paths

- Actor transition 0x3F5954 clears +0x2F19, inserts the actor into the native AI
  actor list and invokes 0x440F48 to refresh stimuli. Its counterpart 0x4428B0
  sets that flag and removes the actor. This flag must not be labeled a one-way
  death flag on the available evidence.
- 0x190F1C is the positive counterpart of 0x2F63F8: it reads stats+0x4A0.
  The ordinary actor preparation router tests it too. The flag therefore has
  effects beyond persistent-stimulus creation. Exact semantic naming remains
  unresolved.
- 0x43E494 constructs envelopes from native record data, including required
  actor-reference flags, interval, height and lifetime. Both the envelope vector
  and the due-listener vector have allocation-growth paths; neither has been
  shown to impose another fixed 256-entry ceiling.
- 0x43E670 allocates a 0x1090-byte stimulus object; 0x43EF20 installs its vtable
  at VA 0x141DB5380. Its deleting destructor is 0x43FB88, which calls 0x43FBBC
  and the base cleanup reached through 0x43FBF0. Reference cleanup 0x43FC18
  iterates the linked weak-reference list and clears reference+0x20 before
  notifying dependents. Ordinary destruction is therefore designed to clear
  the actor's handle. A stale handle after ordinary destruction is no longer
  a leading explanation; corruption or an exceptional path is not ruled out.

## What static analysis cannot settle

The captured incident has no active-stimulus registry snapshot and no values
for the target's newly mapped state switches. At least three situations remain
compatible with existing logs: target stimulus removal/state transition,
stimulus replacement/identity correlation gap, or a transient absence followed
by recreation. No instruction listing can recover which values occurred in
that past incident.

Additional static work could assign semantic names to flags, inspect further
callers, and find more native restrictions. The search space is not literally
exhausted, and this report makes no claim that all reasonable lookups anywhere
in the executable have been completed. The directly relevant lifecycle paths
have now been followed far enough that runtime observation has higher expected
value than another broad static pass.

Runtime observation does not inherently require plugin modification. An
external read-only process-memory capture can sample the known stimulus manager,
actor handles, relevant fields and health alongside the existing source/target
identities. It must validate bounds, identities and relocation and distinguish
unreadable data from absence. Native event hooks would be justified later if
polling cannot resolve a short-lived transition.

Recommended boundary: obtain that runtime evidence before any new gameplay
bypass, forced registration or permission manipulation. The root cause remains
unproven; no new gameplay fix is claimed.
