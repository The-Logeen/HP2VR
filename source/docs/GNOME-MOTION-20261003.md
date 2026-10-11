# Optional gnome motion throwing

VR settings: right grip + Y, OTHER SETTINGS twice, Gnome motion throw On. Default is Off; right-trigger throwing remains available in either mode, without wand ownership.

Once HP2's native pickup has established CarryingActor=GNOME, presentation follows the right controller. Settle the hand briefly (150ms), then thrust/flick forward. The recognizer uses head-relative controller-tip motion in the calibrated body-forward frame, so stick locomotion and whole-body movement do not themselves count as throws. It requires at least 6cm forward tip travel, sufficient forward speed and dominant forward direction within 300ms. A wrist flick contributes through a 25cm virtual motion lever. Slow repositioning, side/back/up motions, initial pickup motion, invalid tracking, excessive jumps and menu interaction are rejected.

One motion event per carried gnome requests HP2's native AltFire/throw-animation path. Native ThrowCarryingActor still handles release, trajectory assistance, speed and gameplay state; the gesture pose is retained for its eventual release. No spell or wand is required. Controller pose loss, recenter and a changed carried actor invalidate the sample. This does not increase throw strength by swing velocity.

HP2GnomeThrowTests exercises recognition, one-shot behavior, settle-to-arm and rejection cases. Actual gnome pickup, attachment and native delayed release require headset gameplay verification; thresholds may need tuning after a real trial. Use small deliberate movements in a clear play area, not a forceful swing.
