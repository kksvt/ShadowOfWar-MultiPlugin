# Imported defender structural policy

Only complete, owned, unspawned imported members qualify. Readable named
incompatible data is normalized; unreadable/unnamed data defers. This compatibility
policy does not label every story variant invalid elsewhere.

## AI and class

| Character/Models | Expected Combat/Tree |
| --- | --- |
| Orc_Infantry | AI_Infantry |
| Orc_Infantry_Tough | AI_Infantry_Tough |
| Orc_Spearman | AI_Spearman |
| Orc_Spearman_Tough | AI_Spearman_Tough |
| Orc_Archer | AI_Archer |
| Orc_Archer_Tough | AI_Archer_Tough |
| Orc_Defender | AI_Defender |
| Orc_Defender_Tough | AI_Defender_Tough |
| Orc_Berserker | AI_Berserker |
| Orc_Berserker_Tough | AI_Berserker_Tough |
| OlogHai_Infantry | AI_Olog_Infantry |

Matching pairs stay unchanged. Other named classes receive a verified stock
fallback; mismatched named AI, including another ordinary class's AI, is replaced.
Olog family appearance/classes prefer OlogHai_Infantry. SQ_Zog uses the tested
Spearman fallback; ZTestVeryLargeOrcInfantry uses Tough Infantry. Otherwise retain
an identifiable ordinary moveset or use Tough Infantry. Lookups/readback validate
the model/tree/template links.

Model conversion reconciles six base-class bonus picker families while retaining
advanced class. Known incompatible special roles receive a validated ordinary
fallback; Role_Outcast changes the epithet. Conversion does not prove every
scripted/inventory invulnerability removed.

## Tribes and fortress generation

| Allowed tribe record | Family |
| --- | --- |
| TribeDef_Dark | Dark |
| TribeDef_Feral | Feral |
| TribeDef_Machine | Machine |
| TribeDef_Marauder | Marauder |
| TribeDef_Sorcerous | Mystic |
| TribeDef_Terror | Terror |
| TribeDef_Warmonger | Warmonger |
| TribeDef_Slaughter | Slaughter |
| TribeDef_Outlaw | Outlaw |
| TribeDef_Undead | Undead |

All other readable nonempty named tribes are outside this allowlist; prefixes/case
variants do not pass. Readable null uses the missing-tribe path. Unreadable or
unnamed non-null data is unknown, not null.

DLC_TribeDef_Sorcerous_Poison maps directly to ordinary TribeDef_Sorcerous. It
shares Mystic UI labels but has distinct fort-selection data. Other replacements
infer one unambiguous retained TribeAttack_<family>; Mystic maps to Sorcerous,
and Dark/Feral/Machine/Marauder/Terror/Warmonger/Slaughter/Outlaw map to matching
TribeDef names. Missing/conflicting clues or failed preferred lookup use verified
Dark. Valid tribes are not overwritten because their attacks differ. Clues choose
a compatible fallback, not proven original history.

TribeDef_Undead is legitimate for resurrected captains and ordinary Mystic fort
data; TribeDef_ShadowWar is distinct. Pre-assault repair has user-confirmed
throne-room prevention for Tribeless and the DLC Mystic alias. It cannot rebuild
geometry already missing during assault; every exotic tribe is not live-tested.

## Appearance, equipment and scale

Compatible Orc/Olog base/body/head appearances are retained; accessories are not
globally stripped. Incompatible full appearance can invoke guarded native
regeneration 0x1BC3AA8. It clears stored weapon/armour, colour and look lists, then
builds equipment from personality, role, tribe and class kit. It is not an
inventory-preserving refresh. Failed readback remains unconfirmed; retries and
preflight checks cannot guarantee every special-character combination is safe.

AIScaleRange is hash 0x234A89CB: ZTestVeryLargeOrcInfantry has [2,2], Tough Infantry
about [0.9,1.1], SQ_Zog [0.9,1.0]. A class-link change need not immediately resize a
cached preview actor. Tested assaults no longer showed noticed oversized enemies;
no independent runtime-scale setter is established. Appearance+0x178 is colour,
not a safe scale-write target.

Hashes 0x2FDDC794/0xC275C7C6 are StatTribeMembersKilled/Recruited IDs, not geometry
selectors. Native hashing uses fold table 0x1DB50D0 and h=h*919+foldedByte, not
ordinary ASCII lowercasing.

| RVA | Native path |
| --- | --- |
| 0x1BC6718 / 0x1BC63F8 | Tribe / role setters |
| 0x577124 / 0x57710C | Template AI / model getters |
| 0x1BC3AA8 / 0x1C15B0C | Member regeneration / faction caller |
| 0x577190 / 0x1BEA190 / 0x4C5194 | Descriptor init / member descriptor / simple-model generation |
| 0x1C10F00 / 0x1C11B08 / 0x61BA74 | Native inventory/equipment regeneration |

See [repair scope](DEFENDER-REPAIR.md) and [validation](VALIDATION.md).
