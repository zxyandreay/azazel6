# ADR-0003: Pinned SDL3 and a separate Android host probe

**Status:** Accepted for platform bring-up. **Date:** 2026-10-21.

## Context

Native startup now executes GE initialization, but no drawing stream or menu has
been reached. Android surface, Vulkan lifetime and input adaptation can be built
independently. They require runtime verification on Android before integration
with guest rendering. No device is currently attached.

## Decision

Use external SDL 3.4.18 at the exact revision in `dependencies.json`, with Java
activity classes and native code from the same checkout. Keep this checkout
unchanged and outside tracked source. Implement an original Vulkan 1.1 presenter
and SDL event adapter in an explicitly named **Host Probe** application, separate
from the hash-validated game-entry executable. The probe contains no game input,
generated translations or game assets and does not claim game execution.

The initial target is ARM64, API 29 minimum, compile SDK 36 and SDK 35 targeting. Query device,
queue, surface format and usage support; reject unsupported configurations with
diagnostics. Own GPU resources with deterministic cleanup, recreate presentation
resources on surface changes and suspend presentation in the background. Keep
input and presentation independent of guest clocks. Retain stable touch IDs,
clear state on focus loss and close disconnected gamepads.

Package this research APK with the already installed SDK tools and a checked
Python build driver. Generate the debug key and all intermediates under ignored
`local/`. Verify APK signing and 16 KiB alignment. Fetching SDL is explicit; the
build rejects a mismatched or modified dependency. Include SDL's license in the
APK. This limited packaging decision does not select the later product installer;
Gradle and the Storage Access Framework remain the planned product route.

## Alternatives and consequences

Using the MHP host would import game-specific predicates and ownership. A custom
Android activity/input stack would duplicate SDL's platform glue. Building a
Gradle product shell now would add tooling and import UX before a running game
host exists. The small SDK-driven probe makes the platform experiment inspectable
with current tools, but needs its own packaging checks and is not a release app.

The Vulkan diagnostic presents only a synthetic clear. It is not a GE renderer
and cannot close either milestone's visual gate. Synthetic input tests and APK
compilation are distinct from real-device touch, controller, resize, Home/resume
and driver tests. Integrating guest draw commands remains separate work.

Yakumo remains unchanged under [ADR-0001](ADR-0001-framework-strategy.md).
Guest idle events retain the limits of [ADR-0002](ADR-0002-bounded-idle-events.md).
