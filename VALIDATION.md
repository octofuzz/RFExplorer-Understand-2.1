# RFExplorer 2.1 validation

## Software checks

- PlatformIO `cardputer_adv` build and post-build packaging; see delivered build report for exact result and resource use.
- Image manifest hashes, ESP32-S3 header, DIO flash mode, application at 0x10000 and complete partition map.
- Package hook verifies bootloader, partition table, boot_app0 and application against merged image.
- Embedded 2.1, Understand, analyst, dossier, family/baseline, journal, location and v20 memory markers.
- Host tests: core, envelope analysis, finder, GNSS satellite model, field intelligence, fingerprint
  matching, actual SD memory implementation, investigation and Understand rules.
- Storage regressions: v1.8/v1.9 import, rollover, last-12 history, UTC/duration/baseline persistence,
  write failure, interrupted replacement, capacity and no transitive family chaining.
- Firmware-derived renders inspect 15 views with illustrative fixtures; fonts from pinned M5GFX.
  Text bounds are checked by the host canvas. These renders do not emulate hardware or RF.
- 2.1 regressions cover strict NMEA checksums, coordinate/date validation, stale-fix and stale-sky
  expiry, GNSS UTC startup synchronisation, bounded UART polling, NaN RSSI handling, complete GSV
  cycle commits, satellite location migration/recovery, event CSV alignment and 32-step waterfall colours.

## Device checks still required

No physical Cardputer was flashed or tested. Verify boot, keyboard navigation, readable display,
Cap receive operation, correct antennas, GNSS UART/fix and SD migration/power interruption on hardware.
Baseline quality depends on comparable receiver settings, antenna, placement and samples.
History is saved encounters, not continuous coverage. No inferred transmitter identity is asserted.

## Source preservation

Receiver, GNSS, scan/finder algorithms, storage CSV schema and shared-bus dependency workaround retain
the working base. Application integration adds journal bookkeeping and dossier/UI behaviour.
Dependency files under `.pio/libdeps` are not patched. Source hashes identify this release.
