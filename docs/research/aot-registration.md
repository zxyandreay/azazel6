# Bounded host registration stack

**VERIFIED, 2026-10-23:** the expanded startup slice exposed a Windows host
failure before guest execution: exit status `0xC00000FD`, with an empty trace.
The MSVC `/Od` object code reserves `0xF9BF8` (1,022,968) bytes in
`register_generated_functions`, while the executable reserves 1 MiB of stack
for the entire thread. The pinned generator emits separate calls with string
temporaries for each registered instruction address.

The profile's `tools/aot_registration.py` now validates that registration body
and replaces it with a static table and one loop. Addresses, function pointers,
names and order are preserved exactly. Unexpected statements, duplicate or
unaligned addresses, mismatched names, missing/duplicate functions and repeated
transformation reject. Guest function bodies and indirect guards are unchanged.
Yakumo remains external, pinned and unmodified.

The resulting MSVC registration frame reserves 152 bytes, independent of the
table length. No larger host or guest stack, interpreter fallback, changed
dispatch allowance or execution timeout is used to mask the failure.

**VERIFIED validation:** three new Python cases check 32,768 ordered synthetic
registrations, a single-entry boundary and malformed generator output. The full
Python suite passes 69 cases. An independent generated synthetic executable
compiled with MSVC `/Od /W4 /WX` registers and checks all 32,768 entries on the
default host stack. All 18 integrated CTest suites pass. The Windows Release
and Android ARM64 builds pass; the exact-hash Windows probe now produces its
guest trace and reaches the explicit guard at `0x08AC757C`. Raw
assembly, generated fixtures and test outputs remain under ignored `local/`.

This addresses host setup, not guest boot or Android gameplay. Runtime evidence
for the expanded guest slice is recorded separately in the
[startup continuation](startup-indirect-2026-10-22.md).
