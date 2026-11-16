# Native entity and damage discoveries

## Build identity and evidence

All function/table addresses are module-relative RVAs unless explicitly labeled
as a preferred virtual address. IDA's preferred image base is `0x140000000`.
Add the loaded module base to an RVA; do not retain heap pointers across launches.

Recorded executable SHA-256:

```text
ED0D95CABB033CD66AAA99D699374881846C3CB03EC345F1123D82D45E0BDAEF
```

This identifies the file inspected on 2026-11-11. It does not establish that every
runtime capture used the identical file. Decoded `game.gamedb` had header identity
`0xD186F247` and 2742 categories. No proprietary extracted data is included here.

**Static** means inspected native code or decoded records. **Observed** means
runtime logs support the relationship. **User-tested** means visible behavior
was reported working. A classification heuristic is not an engine-wide guarantee.
These labels do not imply formal verification of another knowledge corpus.

## Combat-stat snapshots

| Stats offset | Read as | Meaning |
| --- | --- | --- |
| `+0x28` | Actor pointer | Bidirectional association with actor `+0x26A8` |
| `+0x38` | Object pointer | Used by corpse/resurrection processing; precise object type unresolved |
| `+0x40` | Pointer | Observed player marker, matching the player actor |
| `+0x48` | Object pointer | Native HUD marker binding object |
| `+0x60` | Record pointer | Character type |
| `+0xA8 -> +0x28` | Actor pointer | Current combat target in command traces |
| `+0xE0` | Record pointer | Active attack slot; read by attack-start path `0x2E297C` |
| `+0x318` | Variable context | Context used for skill/equipment boolean variables |
| `+0x320` | Manager pointer | Chance contribution manager |
| `+0x428` | Float | Maximum health in the snapshot model |
| `+0x42C` | Float | Current health, supported by runtime damage traces |
| `+0x430` | Float | Health fraction in the snapshot model |
| `+0x5A8 / +0x5B0` | Pointer pair | Situational classification-record range |

These are read-only snapshots, not a complete owning ABI structure. Validate both
actor/stats associations before treating a binding as live. Unreadable fields
must not be interpreted as readable zero values.

| Actor offset | Meaning |
| --- | --- |
| `+0xAC / +0xB8` | Position / XYZW quaternion used by camera tracing |
| `+0x2348` | Inventory |
| `+0x26A8` | Associated combat stats |
| `+0x2708` | Persistent roster key |
| `+0x2710` | Signed 32-bit registration index |
| `+0x2700 / +0x23B8 / +0x2C04` | Player-alliance indicators used by command filtering |
| `+0x3914` | HasThreat / player being targeted; not a generic command prohibition |

## Character and member classification

Observed/allowed Orc names include `Orc_Infantry_FacA`, `Orc_Defender_FacA`,
`Orc_Spearman_FacA`, `Orc_Berserker_FacA` and `Orc_Archer_FacA`.
`OlogHai_Infantry` is the supported Olog type. `Troll_Cave` is a graug and is
excluded. `Player_Character` and `Gondorian_Undead` have distinct feature rules.
The explicit damage allowlist and the commands' `Orc_` prefix filter are separate.

Registration index `-1` denotes the tested unregistered actors. A nonnegative
index, including zero, is a registered candidate. Resolve the member rather
than inferring captain status from type alone. `LedgeHanging` and similar
classification records describe situational state, not rank.

Native member resolver `0x18D920(actor)` returns the persistent faction member
corresponding to CE's Uruk pointer. The guarded path reads global `0x26FFAA8`,
manager `+0x6C68`, and checks `key+0x18 == manager+0xA8` before lookup.
Member `+0x318` is a role record; `+0x4B8` is a 32-bit level. Names such as
`Role_Judge` identify role/epithet, not a universal captain enum. Owner checks
avoid known invalid-key paths but cannot make stale pointers universally safe.

## Record names and lookup

Native helper `0x18BDB0` gates the name pointer at record `+0x20` using bit
`0x02` in the byte at `+1`. `ReadRecord` copies at most 191 characters into a
192-byte buffer. Exact matches require a readable terminating NUL.

Native name hashing uses `h = h * 0x397 + fold[byte]`, starting from zero;
the fold table is at `0x1DB50D0`. Record owner is at `+0x18`. Lookup helper
`0x278540` searches category tables. Relevant globals:

| RVA | Category |
| --- | --- |
| `0x2700530` | Inventory/Items |
| `0x2700488` | Inventory/Affix/Definition |
| `0x2700AC0` | Combat/Stat/Variable/Bool |
| `0x2700B20` | Combat/Stat/Variable/Float |
| `0x2701688` | Interface/NavMarkerTypes |
| `0x2702420` | AI/BandTypes |

Boolean reader `0x1E48DC(context, record)` uses stats `+0x318`; validate the
record owner against context `+0xA0`. Float reader `0x362144` validates the
float map owner against context `+0x70`. Stats `+0x68` is not the skill/ring
boolean-map context.

## Damage calculation and filtering

`G_CalculateDamage`, RVA `0x3643E8`, has the working signature:

```cpp
float (__fastcall*)(int64_t context, int64_t a2, char a3, bool* a4)
```

Context `+0x18` is source stats, `+0x20` is target stats and `+0x48` is the
damage-definition record. The exact remaining argument semantics are unresolved.
Changing its returned float affects damage; negative returns can heal.

Observed return sites include `0x2E3BC1`, `0x6A420C` and `0x6EEB7B`. An apparent
attack can cause several calculations. A return-site label or a positive result
alone does not prove a health change was committed. Correlate complete paired
source/target blocks and health observations.

Current plugin policy:

- Multiply finite positive eligible Orc/Olog grunt-to-captain-candidate damage by 16.
- Alternatively multiply `Gondorian_Undead` damage by 16 against positively
  identified non-player targets; the rules do not compound.
- Exclude self-hits, healing, zero, NaN and infinity. Unknown target identity fails closed.

The grunt rule has no hostility or direct-melee restriction: attributed elemental
damage may qualify too. The wight rule also permits allied non-player targets.
Both multipliers are user-tested; exhaustive damage-path coverage is not claimed.


## Read-only defender member audit

CE's AI/Class/Role/Tribe fields map respectively to member +0x188/+0x278/
+0x318/+0x328. Applied traits (+0x198/+0x1A0) and marker abilities
(+0x1B0/+0x1B8) are separate pointer vectors. Visual storage at +0x2D8
contains distinct base/body/head fields at +0x20/+0x28/+0x30.

The shipped English display key IntRes_Terror_Player (hash 0x0A8076A6)
identifies Overwhelming Awe as trait Terror_Player. Ordinary Fearless is
Quirk_Fearless and is not itself evidence of cheating.

[The audit procedure](URUK-AUDIT.md) documents bounded external selection/member
reads and comparison signals. Static mapping and fixture tests are complete;
imported-defender/Overlord identity, live overview selection and the proposed
tribe/throne-room repair still require controlled in-game validation.
