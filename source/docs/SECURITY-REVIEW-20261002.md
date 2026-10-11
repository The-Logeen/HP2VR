# Proxy DLL security review — 2026-10-02

Defender detected the installed runtime/System/d3d11.dll as
Trojan:Win32/Wacatac.C!ml. A false positive has not been established.
Do not disable protection, add blanket exclusions, or restore the quarantined
binary as part of this review.

Source-level hardening (not a detection-evasion change):

- The real D3D11 DLL uses an absolute System32 path, with dependency searches
  restricted to System32.
- The optional read-only ABI diagnostic module no longer loads during normal
  gameplay. Explicit HP2VR_ABI_PROBE=1 enables it for development. Its dependency
  search is restricted to its directory and System32.
- Existing incompatible shared-memory bridges are rejected rather than reset.
- Failed shared-memory view creation releases its mapping handle.

The proxy's file name and exports remain unchanged. No packing, obfuscation,
signature impersonation, security-setting changes, or detection-driven byte
variation is used. A successful build or a clean scan is not proof of safety.
The DLL remains an unsigned local development build. If Defender continues to
flag it, retain the detection and submit the artifact for vendor review; do not
keep varying the binary until detection disappears.
