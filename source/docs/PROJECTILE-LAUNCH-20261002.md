# Ranged Lumos / Flipendo launch correction

User reported Lumos only working practically touching the statue; Flipendo showed rune/incantation but did not move the slab or release beans.

HP2 baseWand.CastSpell has a native short-distance auto-hit path. That is a plausible explanation for close-range success; it does not prove the projectile traveled correctly.

The actual missing integration was baseWand.ProjectileFire2: regular spells spawn from mesh-derived Pawn.WeaponLoc plus a rotated offset, directed along harry.Cam.Rotation. The existing tracked GetWandEndPoint adapter only corrected effects, not this separate launch path.

Read-only V31 SpellHooksV30.h supplied the scoped launch-expression pattern. The new adapter uses HP2's verified retail function (761 script bytes, 24 parameter bytes), checks reflected local vStart/R assignments and branch boundaries, and substitutes only non-sword launch position/rotation expressions. Native spawn fallback, projectile class, target, charge, seeking, boss tuning and hit handlers remain native. Script bytes and function dispatch are restored after the call. No synthetic hits or enlarged auto-hit range were added.

Validation: full build and existing regression tests passed. Isolated runtime with HP2VR_PROJECTILE_TEST=1 and simulated wand input spawned both actual native classes. Lumos and Flipendo each measured tipError=0.000, forwardDot=1.0000, scriptRestored=1; both SELFTEST PASS. Test projectiles were destroyed after measurement. Run exited 0. This tests launch, not target collision or puzzle completion. Test output is PROJECTILE-SELFTEST-20261002.log. Normal runtime settings/saves and HP1 reference were not changed by the tests. Installer backs up the previous HP2 engine DLL.

Retest: normal Start-HP2VR.ps1, existing save, Stay in body enabled. From several steps away, acquire the rune, hold until charged, release trigger while keeping the wand aimed. Verify Lumos statue/light and Flipendo slab/beans. If unsuccessful, engine logs now include projectile class, launch tip error and directional agreement for each tracked launch.

Source: https://raw.githubusercontent.com/metallicafan212/HP2UScriptDecompile/master/HGame/Classes/baseWand.uc ; retail reflection/bytecode validation was performed rather than relying on community source alone.
