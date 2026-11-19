# v10.20 planner capture: two separate waiting states

Input: user attachment 7b60d89a-1988-4de6-b9fb-505b3fcafe4e.
Native planner and perception traces both report successful installation.
No gameplay source changes were made while interpreting this capture.

| Window | Observation |
| --- | --- |
| Batch 1, focus 2 | Circle admitted once, six updates, cleanup; one Attack admission and immediate start. Current record includes the frontal resist variant. A start is not proof of damage. |
| Batch 1, focus 3 | 282 CircleTarget updates over five seconds; all 282 nested update permission checks false. Attackability stays true; 12 attack admission attempts, none accepted. Captain-specific perception receives 12 refreshes. Circle start preceded this focus window. |
| Batch 2, focus 4 | Dominant 12-second failure: 115 CircleTarget admission attempts, none accepted, no CircleTarget updates. 231 Attack admission attempts, none accepted; nested admission permission passes/fails 70/45. Attack update ran 21 result-zero and one result-two times near the beginning, with no observed start edges; its admission may precede the focus window. |
| Batch 2, focus 5 | Late remaining follower: CircleTarget admitted once, 47 updates, all permission checks false, no observed attack start. Captain-specific refresh resumes for this source. Only about one second of this focus is captured. |

## Dominant failure in the final command

Focus 4 source is 0x228B9E970 and target is 0x22BC2C640. Initially attackable=1;
every subsequent one-second sample through focusMs=12000 has attackable=0 while
the selected target remains correct. At focusMs=7000 the focused source is 0.61m
away. At focusMs=12000 eight nearby sampled followers all have attackable=0.
These are bounded samples, not proof that every follower had identical state.

The focused captain pair has zero matched dispatch, prepare, delivery,
classification and refresh calls across the window. Other stimuli still produce
240 source-wide dispatches, six source-wide queued jobs and six deliveries.
Its first evidence flag clears, the second remains set, and the evidence stamp
eventually stays at world 189.545 while age reaches 11.282 seconds. Both actor
slots exist; occupied-slot samples stay below 256. The trace reports no dropped
counter updates. This excludes an entirely idle source pipeline and hard slot
exhaustion for the sampled pair; it does not identify why target-specific work
stops arriving.

Attack admission RVA 0x445930 requires effective permission and separately calls
0x114A308, which reads AI+0xB9 attackability. Therefore passing permission can
coexist with a rejected attack. The capture strongly points to that independent
eligibility layer. Exact per-call rejection branches were not instrumented, so
the 70 permission passes must not all be labeled confirmed +0xB9 rejections.

CircleTarget is conclusively active in focus 3 and late focus 5. It is not the
active observed updater for focus 4's long wait: all its admissions fail there.
Movement during that portion must not be named CircleTarget solely from its
appearance. Current attack records intermittently show AI_Bip_Cheer_Loop; that
record alone does not establish the target's health or explain the movement.

The next diagnostic should explain loss of captain-specific perception and
capture target liveness/state alongside the actual attackability rejection.
Target health/death/incapacity is not logged in this window, so a legitimate
temporary target-state restriction remains possible. A blanket CircleTarget or
permission bypass would not address the demonstrated independent failure.
