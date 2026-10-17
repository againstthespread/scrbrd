# SCRBRD Lab 1009 validated baseline

Validated firmware source: `ee5b0c6bba4915002f5f1a325c2b8bac4f449523`.

Pinned by annotated tag `baseline/lab-1009-validated-2026-10-17`. Original main
is preserved as `rollback/main-before-production-hardening-2026-10-17`.
Lab 1009 is the user-approved provisional development baseline, with known
Sleeper projection mismatch (F28 deferred for commercial licensing) and physical
coverage limits in the mobile `INTEGRATED_HARDWARE_VALIDATION.md`. No projection
calculations were investigated or changed during merge preparation.

The signed app, source archive, Lab-only build configuration, checksums,
installation records and Lab 1008 rollback remain preserved at:

`/Users/peterdeluca/Projects/_worktrees/scrbrd-mobile-codex/build/hardware-validation/integrated-1009`

Production source retains its original bundle ID/configuration and pubspec build
number; the signed Lab1009 configuration is a separate preserved build artifact.
No phone installation or ESP flash is part of this merge preparation.
