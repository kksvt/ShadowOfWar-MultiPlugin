# Read-only command stimulus capture

Requires Windows, 64-bit Python 3.9+, the inspected Shadow of War game build,
and the existing v10.20 command diagnostics. No additional Python packages.
No DLL rebuild is needed. This opens the game with VM_READ and limited query
rights only; it does not inject, hook, call native game functions, or write memory.

1. Extract all files together. Start the game with your current plugin.
2. Open a terminal in this folder and run:

   `py -3 capture_commands.py`

3. Leave it running, return to the game, and issue Shift+X commands normally.
   It prints `Following batch=...` when a new focused source appears in the log.
4. After circling occurs, wait a few seconds, then press Ctrl+C in the terminal.
   Send the JSONL file whose path it printed, plus the usual command log.

Output defaults to `%LOCALAPPDATA%/SoWPlugin/Diagnostics/CommandStimulus-*.jsonl`.
Existing log content is skipped at startup to avoid following pointers from an
old session. Start the capture before issuing the command you want recorded.
It samples every 0.5 seconds and continues for up to 30 seconds after the latest
focused query. A sample's `log_age_seconds` identifies stale focus information.
Restart the capture after restarting the game.

Optional arguments: `--pid 1234`, `--log "C:\path\SoW-GruntCommand-active.log"`,
`--output "C:\path\capture.jsonl"`, `--interval 1` (0.2 through 5 seconds).
The output filename must not already exist. If Windows denies read access,
run the terminal at the same elevation as the game. The script refuses a game
whose inspected helper signatures differ. The v1.1 capture checks two helpers
that the plugin does not hook, so the installed CircleTarget detour is supported.

Samples include source/target identity links, health, raw state switches,
persistent stimulus handles, planner flags and matching active registry entries.
Registry reads are bounded and bounds/content are rechecked. `incomplete`,
`unreadable-*`, `invalid-bounds`, and `changed-during-read` are not evidence of
absence. Even `complete-best-effort` is a non-atomic external snapshot, not proof
that no transient stimulus existed between samples. Unknown flag names stay raw.
The scan considers up to 4096 envelopes; larger vectors are reported invalid
rather than partially scanned. Actor slots are used only for optional cache reads.

Synthetic-memory tests have passed; live behavior still needs verification.

## v1.2 actor-selection capture

The same command now also captures the native selection manager, matching
source/target entries, requested level and suspension/force flags, plus the
active configuration record. It reads raw integer budget fields from ordinary
record descriptors. Unsupported schemas/types and unreadable values remain
explicit; raw stored values are not guaranteed to equal effective native getters
under all override mechanisms. No native getters are called.

The 256-entry selection table is read and rechecked as a snapshot. Entries are
matched by actor identity, not an assumed slot. An empty match list is not
evidence of absence when snapshot status is unreadable or changing. Candidate
ranking lists are transient native data and are not captured by this version.
It also records raw inputs to native actor selection; they are not interpreted
as a definitive demotion reason. The session header is flushed immediately.
