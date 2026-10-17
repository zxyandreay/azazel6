# Native ultrawide migration

**DOCUMENTED baseline:** official Ultrawide v1.2.1 / OPT-EXP4A at
[`d50cd5f`](https://github.com/zxyandreay/tekken6-ultrawide/tree/d50cd5f19053a2f0aeda7f2970619b984aebad8f).
Its source, architecture and accepted renderer findings were inspected; device
results were not repeated here. All addresses in this page are PSP guest addresses.

| Landmark | Ownership/state required | Native candidate and validation |
| --- | --- | --- |
| `0x0892D9F8` shared HUD slot builder | Resource/family, slot scope, side and caller | Scoped override preserving original entry/exit state; reject unrelated slots |
| `0x08AB9380` rectangle builder | Source geometry and descriptor owner | Transform copied geometry only after classification |
| `0x08825EAC` shared sprite submission | Caller, resource, vertex type, source identity | Submission adapter; never globally scale every sprite |
| `0x0892D72C` timer renderer | Timer/central UI ownership | CENTER transform, test normal and practice/infinity |
| `0x08AC9C38` winner-glow callsite | Four vertices, exact packed 64x64 UVs, Y row, full P1/P2 X families | Controlled source-X transform around converter `0x08AE94A4`; restore original source |
| `0x0892D56C` / `0x0892D5B0` sprite callsites | HP resource/side/orientation packet | Preserve measured HP affine transform, not ordinary anchoring |

For display aspect A, `s = (16/9)/A`. In a 480-wide logical coordinate system:
LEFT is `s*x`, CENTER is `240+s*(x-240)`, RIGHT is `480+s*(x-480)`;
ordinary width is `s*w`. Y is unchanged. The pure implementation accepts finite
aspects in [1,4], matching the inspected reference validity range. Unit tests cover
16:9 identity, 20:9 reference values, fixed anchors, symmetry and invalid input.
These tests establish math only, not visual behavior at every ratio.

SIDE_TO_CENTER is a distinct HP composition. The inspected
[HP downstream findings](https://github.com/zxyandreay/tekken6-ultrawide/blob/d50cd5f19053a2f0aeda7f2970619b984aebad8f/research/renderer-optimization/findings/hp-downstream.md)
give `d=1-s`, `hp_shift=6.528055667877197*d`,
`hp_scale=1+0.24672749638557434*d`, with
`x'=x-orientation*hp_shift` and `width'=width*hp_scale`.
Do not implement HP by independently applying the common endpoint anchors.
This specialized mapping remains DOCUMENTED and is deliberately not exposed as a
generic transform until its packet ownership can be tested in Azazel6.

Winner glow had both a packed UV identity error (`0x00400040`) and first-slot-only
ownership coverage. Preserve corrected UV/Y/full-round X predicates, animated
rotation/skew, alpha, Z and source restoration. Acceptance needs P1/P2 first and
later wins, moving spinner, persistent next-round alignment and negative neighboring
draws. The EXP4A wrapper optimization did not change the accepted glow behavior.

Before installing any native override: validate game hash and original function
ABI, establish stock output, classify owners, compare captured source and output
geometry, then test replacement fonts/textures, Practice, Gold Rush, fast-forward,
pause/front-end and transient UI. Projection/camera ownership remains separate;
do not copy the PRX's direct instruction injection into the native runtime.
