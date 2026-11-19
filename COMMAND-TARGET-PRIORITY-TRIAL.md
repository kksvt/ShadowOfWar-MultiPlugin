# v10.21 command target selection trial

This is an experimental gameplay change addressing the observed live-target
suspension. It is not a confirmed cure for all circling.

`prioritizeCommandTarget=true` installs one signature-checked detour at native
selection policy RVA 0xF2FF00. The original policy still runs exactly once with
the original configuration and cutoff. For a living enemy referenced by an
active custom command pair, the trial moves its temporary candidate to the
front and requests native candidate class 2. Other candidates retain relative
ordering. This gives the target first consideration under the existing quota;
it does not guarantee allocation when native configuration/other conditions
disallow it. Another actor can lose a slot as a result.

The temporary list is restored when the original policy returns. No persistent
priority token, global limit, actor suspension flag or trait is written. Native
policy applies activation/recovery. Once no owned command pair qualifies, future
passes receive the original list. Activation can persist until the next native
policy update; there is no manual forced-deactivation cleanup.

Scope checks include published source/target identity, source allegiance/liveness,
native bound command target and command flag, enemy liveness, supported actor
slot ownership and readable exclusion flags. Dead/unavailable targets, native
ineligible candidates, missing/duplicate candidates, malformed list bounds and
zero-selection cutoffs retain native behavior. The snapshot/list writes concern
the native caller-owned transient list on that same call, not a saved heap list.

## Testing

Rebuild/restart with the new source. Expect:

`target priority trial v1 installed; transient native selection promotion`

Continue using capture script v1.2 and the same Shift+X controls. During orders
the bounded audit includes `target priority evaluated=... promoted=... skipped=...`.
Promotion counts are policy evaluations, not distinct captains or confirmed
activations. Total command audit output can reach eight summary lines/second.

The useful comparison is whether the captain remains unsuspended and retains
persistent stimuli while nearby followers can acquire attackability. Observe
both working and stalled attempts. Native permission, attack-specific path and
animation conditions still apply. Setting `prioritizeCommandTarget=false` removes
the trial on the next rebuild/restart; existing command features remain.

Fixture coverage includes active-pair/exclusion guards, native partition bounds,
stable promotion and restoration, unchanged actor/manager memory, duplicate and
reused-slot rejection, exception unwinding and disabled original forwarding.
Live installation and gameplay outcome remain pending.
