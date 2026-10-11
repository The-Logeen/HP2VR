# Selective tessellation

Supersedes the previous default-off decision for the installed test configuration.

Renderer author documentation specifies System/tessellation.cfg, one material name and factor (0..1) per line:
https://www.moddb.com/mods/unreal-directx11-renderer-for-ut-ug-rune-and-dx

Added zero-factor entries for HP2VRAvatarAtlas and HPVRWandAtlas, including Transient-qualified aliases. These textures belong only to the custom VR body and tracked wand. Native game textures are unchanged. Global Tesselation remains enabled with TessDefaultFactor=0.15 for eligible native meshes, not every BSP surface, sprite or texture.

Build-Gate2 copies the file; Install-Gate2 installs it with ownership hashes and backup protection. No base-game or HP1 files changed. The existing VR settings toggle still disables tessellation globally after restart.

Verification: isolated BeanRewardRoom, simulated avatar crouch/trigger, 18 seconds; 411 coherent pairs and clean exit. Tessellation enabled in engine log. Captured avatar/wand match the non-inflated appearance of the prior tessellation-off test, rather than the prior unexcluded spiky body. See TESSELLATION-EXCLUDED-20261003.bmp and .log. This is visual smoke-test evidence, not GPU per-draw instrumentation or headset verification. Native out-of-sequence scene script errors from the diagnostic map route are not claimed fixed by this graphics change.

Normal runtime now has Tessellation=1. Start from the main menu, load the same save, compare body and wand while standing/crouching/aiming. Inspect nearby native characters and curved props, and check for new holes/shimmering or frame-time loss. If worse, right grip + Y, Other Settings twice, Tessellation Off, then restart.
