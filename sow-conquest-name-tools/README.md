# Capture and re-enter the current fortress defender name

Windows x64, 64-bit Python 3.9 or later, standard library only. No plugin rebuild,
restart, Cheat Engine, injection or game-memory writes are required.

## 1. Capture NOW, with this assault still loaded

Extract both scripts into the same folder. Open your own PowerShell in that folder:

```
py -3 .\dump_defender_name.py
```

It auto-detects ShadowOfWar.exe. If needed, specify `--pid 28312` (only while that
PID still belongs to this game instance). It writes `defender-name.txt` containing
only the exact stored name in UTF-8, plus `defender-name.txt.json` with raw UTF-16
bytes, Unicode code points, source address and session evidence. Keep both files.
Do not leave/restart the assault until capture reports success. A font/display
problem in the terminal does not change the saved characters; console display is
ASCII-escaped deliberately. No trimming or Unicode normalization is performed.

Run this from a normal Windows terminal, outside the Codex sandbox. If Windows
denies access and the game is elevated, use a terminal with matching elevation.
Only PROCESS_VM_READ and PROCESS_QUERY_LIMITED_INFORMATION are requested. A failed
capture does not overwrite an existing name file. Name provenance/visual agreement
still needs verification on the live game; the current Codex process cannot open
that process (Windows access denied).

## 2. Paste later into Friendly Conquest

```
py -3 .\paste_defender_name.py
```

The name is copied to the Windows Unicode clipboard (replacing its current text).
Open Friendly Conquest yourself, click its player-name search box and press/release
F10, without Shift/Ctrl/Alt/Windows held. The script checks the foreground process
is ShadowOfWar.exe, sends Ctrl+V and exits. It does not submit the search or navigate
menus. Empty the field first, or add `--replace` to request Ctrl+A before input.
The script can verify the game window, not which in-game text field you clicked.

If the game ignores clipboard paste, empty the field and retry using Unicode key
input instead:

```
py -3 .\paste_defender_name.py --method unicode
```

This sends UTF-16 key events, including surrogate pairs for emoji. Whether the game
accepts Ctrl+V or Unicode input in this particular field requires live testing.
No successful input is assumed from a successful SendInput return alone: check the
field before submitting. A different path to the name file can be the first
argument. `--clipboard-only` just copies the name and exits. Ctrl+C cancels waiting.

## Native evidence and validation

Steam 1.21's invasion UI helper RVA 0x188F3AC checks:

```
GM = *(module + 0x26FFAA8)
session = *(GM + 0x6D30)
active-name gate = byte(session + 0x47C)
UTF-16 name source = session + 0x340
```

When that gate is nonzero it passes the source to RVA 0xD0BD90, which formats/
truncates the UI name at 25 code units. UI_Invasion_Moneywall is one inspected
consumer. The dump reads the bounded source directly, without calling either
routine. The entire 47-byte helper signature must match. The string must terminate
within 128 UTF-16 units and remain stable across two reads and root/gate checks.
Malformed data, unsupported executables and absent sessions fail explicitly.
This captures the stored UI-source name; it cannot establish whether an upstream
server/platform conversion has already shortened that name.

Seven local tests passed: exact Chinese/emoji/whitespace round trip, signature and
session gates, unreadable/unterminated/malformed strings, changing roots, x64 INPUT
layout, UTF-16 surrogate key events, file validation and clipboard NUL termination.
No actual clipboard/input action was run during testing. Live name capture and
search acceptance remain pending. No defender/player data is bundled here.
