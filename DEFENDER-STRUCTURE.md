# Imported defender structure repair

## Current implementation

The loose sources and v6 package contain the tested v4 normalization baseline plus
tribe inference from retained attacks. v3 remains an archived rollback, not the
current implementation. Configuration stays limited to enable, force Terror_Player,
and diagnostics. Complete scoped imported members automatically arm repair when
the native formatter processes them; no F9 is required. See DEFENDER-AUTOMATIC.md.

Normalization checks identity, faction ownership, row, unspawned state and readable
member containers. It handles SQ_Zog -> Orc_Spearman/AI_Spearman and the oversized
debug Infantry class -> Orc_Infantry_Tough/AI_Infantry_Tough, provided the existing
appearance is an ordinary Orc. Known incompatible AI on supported classes receives
that class's ordinary AI. Model/template lookups must agree before changing links.
Unknown classes and full beast/Nazgul/Balrog appearance conversions remain unsupported.

Known special roles use the native Role_Outcast fallback. Role is also the epithet
record, so this changes the displayed title to “the Outcast.” Retaining the original
cosmetic title separately is not implemented.

## Tribe repair

Readable null tribe pointers and these exact records receive replacement:

- TribeDef_Tribeless
- TribeDef_Easterling
- TribeDef_NazgulTwins
- TribeDef_ShadowWar

An unreadable/unnamed non-null pointer is not treated as a null tribe. Existing
valid tribes are preserved even when their attacks suggest a different tribe.
Legitimate TribeDef_Undead never independently triggers repair or cheating status.

Before calling the existing native tribe setter, v5 selects a stock replacement
from the member's complete trait list:

| Retained attack | Replacement tribe |
| --- | --- |
| TribeAttack_Dark | TribeDef_Dark |
| TribeAttack_Feral | TribeDef_Feral |
| TribeAttack_Machine | TribeDef_Machine |
| TribeAttack_Marauder | TribeDef_Marauder |
| TribeAttack_Mystic | TribeDef_Sorcerous |
| TribeAttack_Terror | TribeDef_Terror |
| TribeAttack_Warmonger | TribeDef_Warmonger |
| TribeAttack_Slaughter | TribeDef_Slaughter |
| TribeAttack_Outlaw | TribeDef_Outlaw |

Each mapping is verified in the shipped database through tribe -> picker -> entry
-> trait links. One distinct match selects that tribe; duplicate copies of the same
trait are not ambiguity. No match or multiple distinct matches selects Dark. If the
inferred record lookup fails validation, Dark is tried. If that also fails, the
member is unchanged. An incomplete trait list already prevents member readiness.
Scope/identity is revalidated after lookup and before the native setter.

The clue estimates a suitable ordinary tribe, not proven historical identity:
edited Orcs can have edited attacks too. Valid records are never overwritten just
because they have no attack or conflicting attacks. Diagnostics print the choice,
reason, and native pointer verification. No new hooks or public settings were added.

## Offline tribe findings

There are 19 shipped tribe definitions. Undead is explicitly assigned by ordinary
Raise Dead rewards and belongs to Mystic_Fort. Bruz belongs to the Warmonger list.
DLC_TribeDef_Sorcerous_Poison belongs to MysticP_Fort. These are preserved.

Gondor and Sauron have no tribe-list membership in this database, but absence alone
does not prove unsafe geometry. They are research candidates, not newly blacklisted.
NazgulTwins itself occurs in Black/Warmonger lists, so list membership alone cannot
serve as a universal validity whitelist either. Runtime geometry selection still
needs further tracing before expanding the invalid-record policy.

ShadowWar's only shipped incoming tribe link leads to GP_FakeDarkTalionRoleDef.
It is separate from legitimate Undead despite sharing the Fort_Title_Undead label.
The export is defender_tribe_fallback_audit.json; work/audit_tribe_fallbacks.py
reproduces its mappings and list memberships without reading the game process.

## Live evidence and limits

The user completed the v4 test with Dark fortress theming, a present throne-room
floor and no noticed oversized enemies. The assault capture confirms Bringer's
ordinary Tough Infantry runtime class and the Overlord's ordinary Defender class,
Outcast role and Dark tribe through binding. Giant's combat size was not separately
established by an identifiable construction pair in that capture.

This supports the existing pre-assault replacements for that tested fortress. It
does not validate every tribe, class or fort. The v5 inferred alternatives have
offline and mocked tests, not live confirmation yet. One earlier F9 freeze did not
recur after a restart; its cause remains unconfirmed. The console disables QuickEdit.

AIScaleRange (hash 0x234A89CB) is [2,2] on the debug giant and approximately
[0.9,1.1] on ordinary Tough Infantry. Already-created preview models may remain large
after a class-pointer change. No direct scale write or preview-rebuild call is used.
The previously suspected tribe fields 0x2FDDC794/0xC275C7C6 are killed/recruited
statistics IDs, not established geometry selectors.

## Native references and validation

Build-specific RVAs: native tribe setter 0x1BC6718, role setter 0x1BC63F8,
combat-tree template/model getters 0x577124/0x57710C, full construction hook 0x1036D0,
member binding hook 0x40ED18. The construction hook may substitute a validated
template; it preserves all other arguments and the native return.

MSVC C++20 /W4 /WX fixtures cover all nine mappings, ambiguity, duplicate clues,
lookup failures, idempotence, valid/Undead preservation, live/home/master guards,
AI/class pairing and creation-hook routing through a mock native entry. Full
hooks.cpp syntax passes. Mocks do not establish native engine safety. No DLL was deployed.
