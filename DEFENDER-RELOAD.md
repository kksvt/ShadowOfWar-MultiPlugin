# Repair provenance and reloads

Trait removal erases detection evidence. A local journal preserves positive
repair diagnoses so a matching imported member can remain protected after a
native rebuild. This is conditional reapplication, not custom game-save
serialization or an edit to the defender's remote army.

## Identity and scope

The key combines invasion Cloud ID, faction record name, nonzero 64-bit member
seed and name hash. Matching requires a unique owned roster member. Duplicate
identities, zero/unreadable fields, home/non-roster members or changing context
are rejected. Heap addresses, pool indices and mutable class/role/tribe values
are not identities. A new Cloud ID needs a new diagnosis.

Native context is GM+0x6D30: active +0x47C, mode +0x118, region +0x130 and inline
Cloud ID +0x168. Supported modes are ranked/friendly conquest and Vendetta;
setup paths include RVAs 0x1B39EE8 and 0x188F3AC.

## Local storage

`%LOCALAPPDATA%/SoWPlugin/RepairState/provenance-v1.tsv` stores bounded positive
diagnoses before their evidence is removed. Limits are 4096 entries and 1 MiB.
Checksums and schema validation reject malformed or duplicate files; oldest
entries are evicted when full. Writes use copied data on a background worker,
coalesced temporary-file replacement and retry after later observations. The
worker performs no game reads or native gameplay calls. Storage failure leaves
ordinary detection/repair operational.

Delete the journal with the game closed to forget diagnoses. This does not
restore already-removed traits. The file is local support state and contains
opaque invasion identity, not the defender's display name or credentials.

## Reapplication and evidence

Matched diagnoses are applied before construction, including later
initialize=false bindings. Live identity registration maintains calculated
outgoing-damage suppression. Mortal-vulnerability picker caches are verified
and retried per family rather than treated as serialized native state.

Identity, malformed-file, atomic-write and DLL-worker lifetime fixtures pass.
The worker holds a DLL reference and releases it after its callback; explicit
shutdown drains at a quiescent point. This does not make arbitrary hot unload
safe. Real invasion reload inheritance under the same Cloud ID still needs a
controlled in-game test. A journal match is deliberately conservative and is
not a claim of globally unique Uruk identity.
