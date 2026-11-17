# Uruk support exports

With enableUrukDump=true, the first observed nonempty owned invasion roster
produces a JSONL snapshot. This is a bounded observation of loaded data, not a
signal that the entire import has completed. It attempts capture before the
particular observed mutation; it cannot promise a pristine pre-repair roster.

## Automatic and manual capture

Automatic stamps track up to 64 roster identities/bounds. Roster changes may
produce another file; ordinary profile browsing or trait edits do not. Polling
is approximately once per second, automatic writes at least ten seconds apart,
and failed writes retry after five seconds.

F8 captures all owned loaded rosters while the game is focused. In menus, hold
F8 while opening a profile card if necessary: input is polled from existing game
callbacks, not an independent keyboard thread. A busy capture may require retry.
Export can operate with repair off when the command-update callback is available.

Files are written to:

`%LOCALAPPDATA%/SoWPlugin/Diagnostics/Uruks-UTC-PID-sequence.jsonl`

The same subdirectory under the temporary directory is the fallback. The console
and OutputDebugString report the path even with verbose diagnostics disabled.
Writing is synchronous and may briefly pause the game; it is bounded, not free.

## Contents and limits

Each export includes module/build context, capture reason, roster ownership and
completion information, raw and displayed level, identity/name hashes, traits,
markers, picker slots/cache flags, AI/class/role/tribe, model/preset/personality,
location/builders, appearance, equipment and available live inventory. Unreadable
and truncated fields are explicit. Record names are not localized profile names.
Opaque invasion Cloud ID and provenance status are included; account display
names and authentication data are not.

Bounds: 512 members, 4096 roster buckets, 8192 pool slots, 512 entries per trait
list and approximately 16 MiB per file. The most recently formatted member/faction
is prioritized, then invasion rosters, then other owned rosters. Loaded bodyguards
are eligible; unloaded defenders are not guaranteed to appear.

Stored equipment has 25 slots at member+0x50, with instance item+0x38. Stored look
vectors are +0x130/+0x138, capped at 512. Live inventory uses actor+0x2348 and
stack-vector bounds +0x740/+0x748, capped at 4096; item/count are +0/+0x20.
Actor links must verify before reading live data. cursed_skin_verified is
positive evidence only: missing items are not proof of an invalid captain.

The exporter itself performs no game-memory writes or native gameplay calls.
Earlier repair may already have changed the data. For an original comparison,
disable repair before a fresh load and retain an export callback.

## Online Vendetta

Native setup/import paths include 0x1B3685C, 0x18A0808, 0x1B40CAC and 0x1B5D368.
Six region invasion records are supported. Creation can arm the imported faction
without a siege preview; home and DLC campaign rosters remain excluded. Static
mapping and fixtures pass, but end-to-end hacked Vendetta repair is not yet
user-verified.

For a support report, provide the JSONL, configuration/build, mission type and
capture stage, plus what happened in game. Automatic and post-load F8 captures
are useful together; reproducing the exact ranked fortress is not required.
