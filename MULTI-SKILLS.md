# Purchased skill upgrade multi-equip

Selecting a purchased upgrade preserves sibling upgrades that were already
equipped. Purchased-but-inactive siblings stay inactive, and locked upgrades
stay locked. The user has confirmed this selection behavior working in-game.
Base/default/prestige records retain native activation behavior.

## Native implementation

UI activation reaches `0x17D9E4C`, obtains player inventory and calls
`0x1BC8C6C(inventory, skillRecord)`. That wrapper resolves the unlock item using
`0x18A2278` and calls `0x1BC8C94`. The selection routine checks ownership,
collects desired and sibling effect items, removes siblings through `0x4F163C`,
then adds selected effects through `0x1C01E0`. Active-state query `0x1BCD6DC`
checks unlock ownership and active-item presence.

The plugin detours the full activation wrapper, validating its entry signature.
It snapshots only active effect items of purchased siblings, runs the original
activation and restores missing snapshotted effects using native inventory add
`0x1C01E0(inventory, item, 1)`. Restoration requires successful activation,
the same live player inventory and retained unlock ownership. No unlock token,
point balance, prerequisite or shared database record is modified. Unknown
records, NPC/unbound inventories and failed activations pass through.

Generated `sow_multi_skill_data.hpp` contains 143 child mappings from
`Interface/Menu/MenuTreeNode/SkillChild` SkillData/BaseSkillData links. Each maps
a skill to its parent, unlock token and up to four SkillItem effects. Parent
groups contain at most three mapped children, bounded by a 12-effect snapshot.
No name-prefix guess is used to determine sibling relationships.

A reentrancy guard prevents recursive preservation. Native explicit deactivation
paths are not hooked; the ordinary menu determines which actions it exposes.
There is no automatic selection of every owned upgrade or custom unequip shortcut.

## Effect conflicts and elemental weapons

Multiple equipped effects do not guarantee that incompatible action replacements
combine into one action. Native decision trees can still choose one branch by
priority. Exhaustive combinations have not been tested.

The resurrected-grunt weapon extension still grants one weapon element per grunt.
When several Ringwraith elements are active it uses the last selected still-active
Ringwraith elemental upgrade. A single active flag always wins. After process
restart, when the last click is unknown, fallback order is Fire, Poison, Curse;
select the desired elemental upgrade again to set the preference.

This compatibility handling prevents multiple active flags from disabling the
existing resurrection-weapon extension. It does not force several elemental
weapon types simultaneously onto one grunt.

## Integration and validation

Configuration is `multiEquipSkillUpgrades` and `multiSkillDiagnostics` in
`hooks.cpp`. The module is `sow_multi_skills.hpp`, with generated mapping data
in `sow_multi_skill_data.hpp` and the selection observer in
`sow_resurrection_weapons.hpp`. Failed installation retains native skill selection
without disabling other features. Removal includes the new hook's result.

Diagnostic lines identify the selected skill, event and count of restored sibling
effects. Output uses the existing console and OutputDebugString, bounded to 128
lines per installation. A restored count is an effect-item count, not a count
of purchased or newly unlocked skills.

Native inventory/save handling carries the resulting items; no custom save file
is introduced. The positive in-game report confirms selection behavior, but
save/reload persistence and all conflicting combinations have not been separately
established. See [validation](VALIDATION.md).
