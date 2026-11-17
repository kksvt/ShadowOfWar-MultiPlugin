# External read-only Uruk audit

The separate audit_uruks.py / run-audit.cmd utility uses 64-bit Windows Python
3.9 or newer without third-party packages. It reads the selected process and
writes JSONL; it does not install a DLL, write game memory, repair traits or call
native gameplay functions. It does not elevate itself.

Prefer the plugin's [support export](URUK-EXPORT.md) for routine reporting. The
external utility is useful for an independently supplied member pointer or
control capture. Automatic overview selection was empty in tested contexts and
must not be treated as reliable; unresolved selection never chooses an arbitrary
member as a fallback.

## Capture

Supply a current CE member pointer, not an actor or stats pointer:

```powershell
py -3 audit_uruks.py --member 0x12345678 --once --label "captain-control"
```

Use --pid for explicit process selection and --output for a destination. Without
--once, F8 captures and Ctrl+C exits. The default output is a timestamped
SoW-UrukAudit JSONL file in the temporary directory. Pointers expire across
reloads/restarts; do not reuse them as persistent identities.

Field layouts are in [Discoveries](DISCOVERIES.md). Stored level is zero-based:
displayed level equals raw level plus one. Capture the member's own faction even
when the player is in another region. Imported-defender and Overlord identity
are not independently established merely by a successful pointer read.

Assessments are evidence summaries: review-required is not proof of cheating,
and no-listed-signal is not a clean certificate. Null, unreadable and readable
unsupported records are different outcomes. Compare natural controls before
turning an uncertain signal into a repair rule.

CE allocation errors/crashes were reported alongside the plugin. Their exact
cause is unresolved; neither universal CE incompatibility nor hook count alone
has been established as the explanation.
