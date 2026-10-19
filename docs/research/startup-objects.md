# Post-constructor objects and exception boundaries

**VERIFIED, 2026-10-19:** after registering its exit callback, startup allocates
an object of size `0x1AC` and enters initializer `0x0887B844`. Allocator wrappers
`0x088789D8` and `0x08878B48` share the range beginning at `0x08B7960C`; assigning
that range its own root preserves unique generated ownership.

The initializer calls `0x0887D63C` for embedded objects at offsets `0xBC`, `0xE4`,
`0x10C` and `0x134`. That constructor installs vtable `0x08B924C0`. The four
offset-4 virtual calls at `0x0887B468`, `0x0887B520`, `0x0887B5D8` and `0x0887B68C`
therefore derive their allowed destination from file-backed slot `0x08B924C4`.
The outer initializer also installs vtable `0x08B92310` at offset `0x15C`;
calls `0x0887961C` and `0x08879578` use its offset-4 and offset-8 slots.
Each generated transfer checks the destination at runtime. These maps describe
the investigated startup objects, not every future object or modified vtable.

**VERIFIED:** the normal/error paths around division in this initializer contain
BREAK instructions, including `0x0887B260` and `0x0887B2B4`. CFG discovery records
BREAK as a terminal instruction with no delay slot or fallthrough. Both sides of
the preceding conditional remain reachable in conservative analysis. No arithmetic
error branch is erased or treated as success. Inline SYSCALL still rejects.

The pinned Yakumo emitter already emits `Runtime::unsupported` followed by an
immediate return for BREAK. Generation now verifies the exact stop at each
discovered site and rejects missing, changed or duplicate labels. Azazel6 installs
no unsupported-instruction recovery hook. This permits coverage of normal paths
without emulating exceptions or allowing execution beyond a trap.

Synthetic tests cover a BREAK as the final file-backed word, multiple break codes,
conditional bypass, rejection in a delay slot, continued syscall rejection,
generated-stop integrity and the native stop's preservation of GPR/HI/LO state.
Raw disassembly and generated code remain in ignored local experiments.

**VERIFIED execution:** the initializer completes a 2048-record loop, decrementing
its offset from `0x11FDC` by 36 until the final record. Each record crosses six
dispatch boundaries. The previous 4096-dispatch limit stopped partway through
this progressing loop. The diagnostic limit is now 32768; the independent
15-second timeout and disabled interpreter remain. This changes no guest clock,
timing or FPS policy. The trace contains exactly 2048 visits to `0x08879844` and
reaches uncovered helper `0x08878B50` after 15552 dispatches.

The initializer passes a null mutex name and retains error `0x80020001` as its
handle. Subsequent lock/unlock calls return invalid-UID errors, which this code
ignores. This agrees with the pinned reference's null-name expectations; the
profile does not convert the failed create into a successful anonymous mutex.
The run has one exit callback, three valid mutexes, eight inactive timers and
three data-cache operations. No BREAK was executed.

The checkpoint covers 218 roots/19293 words and 37 terminal BREAK sites.
All 41 Python cases, nine CTest suites and Windows/Android ARM64 compilation
passed for this checkpoint. Title-screen boot and exception delivery
remain unimplemented; the next experiment is the observed helper above.

## Object allocator query and selection tables

**VERIFIED continuation:** `0x0887929C` queries the initialized allocator through
the seven-entry table at `0x08B92680`, with JR site `0x088792F4`. Allocation helper
`0x0887BF48` uses another seven-entry table at `0x08B926A8`, with JR site
`0x0887C0A4`. Both indices have explicit unsigned bounds before the table load.
The generator validates every file-backed destination and guards both jumps.

The latter helper also has an optional callback at `0x0887C1E8`, loaded from the
object passed in argument register 10. Its target is not established, so that
site has an empty allowlist and stops before the call or delay slot. Observed
startup passes no callback and completes this path without reaching the guard.
The native trace then enters the next startup object through `0x0886074C`.
