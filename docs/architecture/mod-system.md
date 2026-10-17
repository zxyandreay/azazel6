# Moveset and mod subsystem design

**PLANNED, not implemented.** Milestone 001 intentionally has no cheat replay
engine, payload loader or gameplay writes. Current evidence justifies a design,
not speculative interfaces that would freeze unknown layouts into public APIs.

A future package manifest binds game hash, character, format version and resource
hashes to move/animation descriptors, static payload blocks, table substitutions
and a bounded event program. Keep package data in host-owned immutable storage
until a validated guest allocation/lifetime allows staging. A guest address is
never a native pointer and a package file cannot authorize arbitrary host writes.

Activation should move through validated package -> selected character -> observed
battle readiness -> staged payload -> verified pointer/table substitution -> active.
Each stage checks game version, pointer ranges, alignment, expected old values,
allocation generation and ownership. Detect overlapping writes across packages and
camera/ultrawide owners before installation. Sort operations deterministically.
Journal old bytes and restore only if the current values still match this package's
writes and the allocation generation is unchanged. Otherwise stop and report an
ownership conflict; writing old pointers into a recycled allocation is not rollback.

Static data construction should run once per validated lifetime. Translate proven
conditional semantics to events at selection, battle start, move transition, round
end and unload. Animation/cinematic callbacks require explicit camera and timing
ownership and cleanup on interruption. Keep diagnostic ring buffers bounded and
count bytes copied, operations run and conflicts per guest tick. Avoid continuous
replay of hundreds of thousands of master records.

Two moveset pointer layouts and large memory-copy operations are documented in
[Jin8 findings](../research/moveset-findings.md). Native compilation does not remove
these hazards. Some source master writes exceed classic 32 MiB RAM, whereas stock
ELF loading here fits 32 MiB. Do not automatically allocate mod-compatible expanded
memory without validating the selected package and game state.

The first implementation gate is a tiny, independently redistributable synthetic
package exercising conflict detection and rollback, followed by one user-owned
move whose memory changes and lifecycle have been captured. Full Jin8 compatibility
must wait for the unresolved freeze investigation.
