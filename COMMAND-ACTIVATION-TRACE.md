# v10.23 command-target activation diagnostic

Source package: rebuild and restart the game. Target priority is enabled;
`traceCommandActivation=true` enables this observation-only trace. Older
`commandCooldownDiagnostics`, `traceCommandPerceptionQueries` and
`traceCommandPlanner` summaries are disabled by default. Their gameplay features
and the scoped cooldown bypass remain available.

v10.23 corrects the v10.22 observer's handle correlation. The old observer read
actor+98 and assumed the first resolver's owner entity was the actor. The actual
native actor handle is at +2BFC, and the separate owner entity has its matching
handle at entity+98. Therefore zero second handles and `first-other` from v1
are not evidence of an engine lookup failure. The native calls were forwarded
unchanged; only diagnostic observation/correlation was wrong.

Expect these startup messages:

```text
activation trace v2 installed; two-stage native reactivation; transition-only target summaries
target priority trial v1 installed; transient native selection promotion
```

If the activation trace says unavailable, send that startup output before
attempting another capture. Failure of this optional trace leaves existing
gameplay features enabled. No automatic recovery is introduced by the trace.

## What to do

Use Shift+X normally in a crowded encounter. If followers circle excessively or
a captain appears stuck, alt-tab to pause and tell us. Leave the process MCP
running. There is no minimum incident duration, no new hotkey and no requirement
to reapply an expired order merely to preserve the diagnostic.

Output reaches the debug console and the existing
`%TEMP%\SoW-GruntCommand-active.log`. In a normal Windows session this is
`C:\Users\krzys\AppData\Local\Temp\SoW-GruntCommand-active.log`.
The file is appended across runs; include the newest initialization message when
copying output. Do not attach Cheat Engine just for this capture.

Only targets of verified plugin-owned group commands are enrolled. Up to eight
recent identities are retained for 60 seconds after their last verified command
ownership. This allows late callbacks after command expiry to remain observable.
Actor/stats reciprocal links and actor handles are checked before sampling.
These checks reduce stale correlation; they cannot prove object lifetime.

The existing player-update callback polls state every 100 ms and emits at most
one changed summary per tracked identity per second. Stable targets produce no
periodic heartbeat. Detours buffer fixed-size observations, preserve native
arguments/results, and never log, allocate, wait for the trace mutex, invoke
extra engine functions or modify game memory.

## Reading a summary

Each `activation trace v2 target=...` line includes:

| Field | Meaning |
| --- | --- |
| target / stats / epoch / batch | Observed identity, diagnostic generation and first enrolled command batch |
| handles | First-stage simulation handle / second-stage actor handle, hexadecimal |
| state | Inactive / pending reactivation / activation guard (`-1` means unreadable or identity unresolved) |
| selection | Requested native selection level / suspension byte (`-1` unresolved) |
| request | Native reactivation request entries / returns for this tracked target |
| first | First queued callback entries / returns |
| first hit/miss/other | First resolver returned a simulation object whose owner entity has the tracked native handle / null / a different or unreadable identity |
| second | Second queued callback entries / returns |
| second hit/miss/other | Second resolver returned matching actor / null / another actor |
| activate | Final activation entries / returns; a return does not imply successful activation |
| pendingWait | Observed pending episodes lasting at least 2 seconds of wall time |
| dropped | Cumulative correlation/observation drops or watch-capacity replacement |
| historyTruncated | More new events than the eight-event tail or its text buffer could retain |
| events | Recent ordered events with `(inactive/pending/guard,value)` |

There is no whole-session log limit. Counters aggregate repeated calls; the
eight-event tail is intentionally bounded. Missing events with nonzero drops or
truncation are **not** proof of a missing native callback. An unavailable first
handle (`FFFFFFFF`) also prevents first-stage correlation. Handle ambiguity is
left unresolved rather than guessed.

Observation begins at target enrollment; a pending state already present then
can predate all recorded callbacks. The trace does not intercept raw queue
insertion, so absence of a callback cannot by itself distinguish a job that was
never enqueued from one that was enqueued but not dispatched.

`pendingWait` is an observation, not a timeout repair or proof of a failed queue.
It uses wall time, so time paused/alt-tabbed can contribute. Simulated time and
MCP snapshots must be considered before concluding that AI stalled.

## Native boundaries observed

All addresses below are RVAs for the inspected executable:

| RVA | Observation |
| --- | --- |
| 3F5704 | Reactivation request, including fifth stack float argument |
| 4171B4 | First queued callback, with its native payload unchanged |
| 2FDA00 | First handle resolution, recorded only inside that callback's scoped trace |
| 3F5930 | Second queued callback |
| 161E78 | Second handle resolution, recorded only inside the scoped second callback |
| 3F5954 | Final activation entry/return and flag changes |

The first handle is read through actor+5A0, then component+2BFC, matching the
3F57C4/176EFC producer. The second is actor+2BFC, matching the 161EB0 lookup.
The first resolved object's +28 points to a separate owner entity, whose +98
supplies that native handle through 3747CC. Assembly was checked: the callback's
second stage receives an integer handle, not a pointer to it. Each hook is
guarded by its inspected 16-byte signature. Partial installation is rolled back;
failed removal preserves trampoline storage for remaining forwarding detours.

The earlier ranking trial remains unchanged. It addresses victim scheduling
priority but skips a victim already pending reactivation. This diagnostic is
intended to distinguish missing first/second callbacks, failed handle resolution
and activation calls that leave pending set. It cannot identify an uncaptured
native transition from aggregate counts alone.

## Validation and limits

MSVC x64 C++20 fixtures check exact forwarding (including the float and native
returns), nested scope restoration, both resolution failures, native activation
that leaves flags set, ownership/identity filtering, post-expiry tracking,
pending-state edge detection, stale/ambiguous tickets, nonblocking contention
and unchanged mock game buffers. Existing command fixtures and full hooks
compile with `/W4 /WX`, with and without optional rage recovery.

Live v2 installation and four complete two-stage reactivation cycles were
observed on 2026-11-19, across two of five tracked identities. Both lookups
matched and final activation cleared inactive/pending state; recorded drops and
pending-wait episodes were zero. The user could not reproduce circling in that
crowded run. Four summaries truncated their bounded event tails, so aggregate
counts and available flag edges must be distinguished from complete chronology.
This validates the corrected correlation on those cycles, not a general fix.
Global AI budgets, native selection quotas and command binds
are unchanged. Keep `prioritizeCommandTarget=true` for this test. Defender repair
remains disabled and debug player damage protection remains enabled in the
development configuration, as in v10.21.
