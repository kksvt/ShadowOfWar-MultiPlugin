# Class and ability policy

These rules apply only inside the imported-defender scope described in
[Defender repair](DEFENDER-REPAIR.md). Unknown context defers classification.

## Enforced rules

| Trait | Permission / exception |
| --- | --- |
| Quirk_NoLastChance | Assassin, or Role_BlackHeart, Role_Cruel, Role_Merciless; DLC3ToMain_Fixer preset exception |
| Quirk_LifeSteal | Berserker, plus exact native role/preset exceptions |
| Quirk_Summoner_Gang | Commander; DLC3_Architect, DLC3ToMain_Architect and DLC3ToMain_Slayer preset exceptions |

The LifeSteal policy has 19 exact role exceptions and SotP_Volcano_SC preset
support. The implementation and defender_class_trait_audit.json contain the
complete authoritative list; title text alone is not used to infer permission.
Despite its name, DLC3ToMain_Slayer carries Commander specialization.

Ordinary gang ownership, Tracker horn calls and beast/sapper/spider summons are
not Call Reinforcements. Off-class Second Wind and Dying Command are cleaned on
already-flagged defenders; they are not independent cheat indicators.

## Review-only evidence

Slayer/Combat Master with Immune_Frost is not automatically flagged: ordinary
Frost Immune generation excludes Combat Master, but CMT_FreezeImmune reward
records also exist. Their legitimate reachability is unresolved.
Resistant_Combat_CMT is an internal record, not a separate visible class label.

Necromancy remains review-only. Raise_Dead and Necromancer_SelfRes are actual
abilities; Marker_IsNecromancer is eligibility data. Curse_Weapon,
Marker_StatusEffect_Curse and stored/live cursed-skin items are distinct evidence.
Generation pickers require curse and exclude NoStatusEffect, but direct grants
and material state prevent a safe universal visual rule. Role_Crafty,
Role_Warlock and Role_Gravewalker have direct curse/necromancy grants. Paleness
is not a required invariant.

## Natural controls

| Captain | Observed state |
| --- | --- |
| Shag Brain Biter, Nurnen | Cursed weapon/status marker; no necromancer marker or Raise_Dead; no stored orc_Cursed |
| Stakuga Face-Eater, Cirith Ungol | Cursed weapon/status marker, necromancer marker and Raise_Dead; stored orc_Bloody_Face rather than orc_Cursed |
| Ghash Scar-Hide, Seregost | Cursed weapon/status marker and necromancer marker, without Raise_Dead; stored orc_Cursed |

All three were user-confirmed natural. Self-resurrection was absent in all three.
Stakuga's summoned live actor had CursedWeapon, Item_HasACurseTrait and
Item_HasANecromancyTrait, while cursed-skin/Albino/DarkSkin inventory checks were
absent despite visibly cursed skin. Missing stored cursed-skin items therefore
must not trigger repair. Captures across regions retain their own member/faction
identity; current player region is not the member's origin.
