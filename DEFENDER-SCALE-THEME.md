# Scale and fortress tribe investigation — 2026-11-16

## Delivery

`sow-defender-normalization-v4.zip` is based on the active v2 package, not the v3
rollback. It retains v2 normalization and adds the exact `TribeDef_ShadowWar`
identifier to the imported-defender tribe fallback policy. Existing normalization
uses the native tribe setter to replace it with `TribeDef_Dark`, verifies the member
pointer, and retains the usual identity, armed-faction and unspawned-member guards.
Legitimate `TribeDef_Undead` remains excluded from this policy.

The native setter has previously succeeded in the user's v2 preview captures.
The subsequent v4 live test confirmed Dark fortress theming and a rendered
throne-room floor for the supplied fortress. A verified member pointer does not prove that world streaming has
adopted the new tribe. One earlier v2 F9 freeze remains unexplained; restarting did
not reproduce it. QuickEdit is disabled in the new console initialization, without
claiming it was the cause.

No new hooks, public options or appearance-memory writes are introduced. Existing
Shift+X/Z binds and debug damage protection are preserved. The v2 reference ZIP and
v3 rollback ZIP remain unchanged. Top-level loose sources now represent v5, which incorporates this tested v4
baseline plus tribe inference; use the current package's files together.

## Confirmed field mappings

The executable hash helper at RVA 0x423E70 translates bytes through the table at
RVA 0x1DB50D0, then accumulates `hash = hash * 919 + translatedByte` from zero.
Ordinary ASCII lowercasing is not an equivalent hash implementation.

| Database hash | Verified field name | Evidence |
| --- | --- | --- |
| 0x234A89CB | AIScaleRange | Exact executable string hash; native range registration at RVA 0x162F09C |
| 0x2FDDC794 | StatTribeMembersKilled | Exact executable string hash |
| 0xC275C7C6 | StatTribeMembersRecruited | Exact executable string hash |

Dark tribe's 220/221 and Undead/ShadowWar's zero values are statistics IDs. They
are not evidence of a fortress geometry index. Earlier unresolved-field discussion
in DEFENDER-STRUCTURE.md is superseded by these names.

## Scale

Shipped `ZTestVeryLargeOrcInfantry` has AIScaleRange [2,2];
`Orc_Infantry_Tough` has approximately [0.9,1.1]; `SQ_Zog` has [0.9,1.0].
The latest preview capture verifies Giant and Bringer's class links changed to the
ordinary Tough class, but the user still observed oversized preview models.

This establishes that changing the member class link did not immediately resize
those displayed models. It does not establish whether scale is cached on a preview
actor, overridden by inventory, or independently retained elsewhere. Native member
XML export at RVA 0x1BC45A8 and appearance export at 0x1C06444 were inspected;
the scalar array at appearance+0x178 is color/tint data, not a safe scale-write target.
No runtime scale setter or preview rebuild has been validated. v4 does not claim
to fix the remaining visual size.

## Tribe evidence

A bounded incoming-link scan of the shipped database finds only one direct
reference to `TribeDef_ShadowWar`: anonymous FactionTribeEntry #604, referenced
by `GP_FakeDarkTalionRoleDef`. This is distinct from ordinary resurrected captains.
Absence of other database links alone does not prove every native use is invalid;
the fallback is limited to the already armed, imported conquest-defender scope.

Normal Raise Dead reward definitions and Zog_TurnUndead explicitly assign
`TribeDef_Undead`. It is also included in the Mystic_Fort tribe list, used by
ordinary fort predicates and upgrade pickers. The two legitimate user controls
independently confirm that Undead tribe must remain supported. Neither this list
nor the zero statistics IDs proves how the throne-room geometry is selected.

## Test

1. Restart with all sources from the v4 ZIP. Confirm the v4 normalization install line.
2. Hold F9 while opening the same hacked Overlord in the conquest preview. Look for
   `normalize field=tribe from=TribeDef_ShadowWar to=TribeDef_Dark result=verified-native`.
3. Before launching, close and reopen the preview cards for Giant and Bringer;
   report whether size changes. This tests recreation without adding a native call.
4. Run the assault and retain the full log through entering the throne room.
   Report both room/floor presence and the two Orcs' actual combat size. Construction
   audit lines distinguish the resulting runtime class from preview-only observations.

If native tribe verification is absent or unconfirmed, do not interpret a missing
room as a test of a successfully applied replacement. Do not use legitimate Undead
as a fallback conversion target for this diagnosis.

## Local validation

MSVC C++20 /W4 /WX structural fixtures and full hooks.cpp syntax checks passed.
Fixtures cover ShadowWar native-call routing/idempotence, live/home/master scope
exclusions, existing AI/class behavior and legitimate Undead controls. Mock setters
cannot validate live engine threading, streaming, or throne-room rendering.

Reproducible offline results are in defender_scale_theme_research.json. Investigation
scripts are work/trace_structure_data.py and work/trace_db_parents.py; neither opens
the game process. No DLL was deployed.

## Subsequent live result and current source

The completed v4 assault retained the Overlord's Dark tribe/Outcast role through
binding and ordinary Defender runtime class. Bringer spawned as Tough Infantry;
the user noticed no oversized enemies and did not fall through the floor. This
supports the tested pre-spawn path; it does not validate an independent scale write.
The current v5 policy and research limits are in DEFENDER-STRUCTURE.md.
