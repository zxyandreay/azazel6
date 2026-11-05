# ADR-0005: Explicit checked read-only local UMD media

**Status:** Accepted for bounded startup research. **Date:** 2026-11-05.

## Context and decision

The exact-input startup worker retries an empty-drive wait while the main thread
polls its exit status. This computer has a matching-title USA ISO, allowing the
next observed media path to run without inventing readiness or worker completion.

Mount only an explicitly selected ISO through the research runner's `--iso`.
Pin its size and SHA-256 in the Tekken profile independently of the decrypted
EBOOT fingerprint. Validate the EBOOT first. The runner checks the image before
creating run outputs; the native host independently hashes its opened media
stream, checks ISO geometry and bounded directories, then validates PARAM.SFO
title, region and revision before creating guest state. Hashing uses bounded
memory, with compression adapted from the pinned MIT-licensed Yakumo source;
the external checkout remains unchanged.

The profile owns the provider, handles and I/O policy. No arbitrary host directory
or writable mount is exposed. Only absolute `disc0:/` paths are accepted; reject
traversal, other devices, unsupported ISO layouts, malformed records, oversized
metadata and out-of-image extents. File handles belong to one runtime, have a
bounded live count and are not reused. The image must remain unchanged while
mounted; this research interface does not implement hot replacement or ejection.

A validated medium plus successful activation permits the observed readable-state
wait to return. Without media, preserve the existing finite empty-drive timeouts.
Callers requiring callback delivery remain rejecting. The first file ioctl
returns the actual file's first sector, following pinned PortableKit's
`ioctl_op` contract. Unsupported imports and commands remain research stops.
Subsequent I/O requires observed arguments and checked guest-memory ownership.

## Alternatives and limits

An extracted host-directory bridge loses original sector addresses and creates
unnecessary path exposure. The framework's whole-file hash helper uses multiple
image-sized buffers. The bounded provider and streaming hash avoid both problems
without adding a production runtime dependency.

This is research input selection, not the final Android import UX. Private game
inputs, generated translations and captures remain local. The source repository
contains only fingerprints, independently authored fixtures and reviewed evidence.
No guest CPU cycles or I/O latency are inferred from host time. Runnable-time
modeling, media callbacks and general asynchronous I/O remain separate work.

## Verification

Synthetic tests cover streaming-hash block/padding boundaries, valid ISO/SFO
metadata, wrong hashes/identity, truncated metadata, endian disagreement, cyclic
directories, invalid extents, paths, EOF and oversized reads. I/O tests cover
activation, mounted readiness, open/sector-query/close, stale handles and atomic
rejection of invalid output buffers. Exact-input execution evidence is recorded
separately; these contracts alone prove no boot.
