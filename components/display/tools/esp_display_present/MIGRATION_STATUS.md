# Component boundaries and migration status

Production components are `esp_display_present` and sibling `esp_lv_present`.
Applications use public headers; `src/` is internal. The old V2 component is not a
production dependency.

- RGB/MIPI use multi-FB scanout; NONE and single-FB updates are unsupported. AUTO
  selects DOUBLE_PARTIAL or TRIPLE_PARTIAL from the actual driver pool.
- Panel IO keeps NONE/TE. GRAM NONE permits tearing; TE depends on real panel
  timing and transfer budget.
- Creation rotation and runtime rotation are distinct. Rotation requires suitable
  storage; synchronous in-place swap may reject nonzero runtime rotation.
- The plan is a peak budget/capacity model, not a reservation of heap memory.

See [compatibility](COMPATIBILITY.md), [modes](docs/MODE_ARCHITECTURE.md),
[state machines](STATE_MACHINE.md) and [development](dev/README.md).
[Recorded board results](dev/evidence/display-latest-20261111/README.md) retain
source/configuration provenance and reuse notes. [History](dev/ARCHIVE.md) is
external. Cleanup does not imply Registry publication or external compatibility.
