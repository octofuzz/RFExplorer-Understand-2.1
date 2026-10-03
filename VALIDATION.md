# 2.8 validation scope

Run all 19 groups with `python tests/run_host_tests.py`, then `test_replay.py`, `test_integration.py`, `test_export.py`, `test_capture_comparison.py` and `render_ui.py` under tests. Build with `pio run -e cardputer_adv`, then run `python tests/validate_release.py` to verify the exact binaries and source manifest. See the delivered BUILD-REPORT.md for actual results.

The UI renderer checks 74 fixtures against the 240x135 display; it is not a physical display test. SD failure/reload, named session recall, compatibility rejection, bookmarks and timeline rollover are host-tested. Detector replay retains the reported 1019 ms noise-floor regression. Hardware reception, reference capture comparison, SD latency and real-world battery behaviour remain unverified. Do not call RSSI observations satellite detections.

# RFExplorer 2.7 Skywatch validation

Run from this source directory:

```
python tests/run_host_tests.py
python tests/test_replay.py
python tests/test_integration.py
python tests/test_export.py
python tests/render_ui.py
pio run -e cardputer_adv
python tests/validate_release.py
```

The host suite covers the six supported satellite categories, signal/ID boundaries, blank/zero and stale C/N0, rollover, ambiguous GN talkers, duplicate GSV IDs, incomplete cycles, unsupported GSA system IDs, missing sky angles and fresh-versus-stale stream selection. Logbook tests cover legacy migration without confidence promotion, strict geotag loading, report deduplication, confirmed counts, reload, write failure and recovery. RF regression tests retain the near-floor/zero-burst artefact rejection and add weak continuation-window exclusion. All previous GNSS, journey, memory, navigation, battery and RF tests remain.

UI fixtures render actual firmware branches with illustrative data, including every satellite category, empty/stale states, live detail and saved history/geotags. They check text bounds on 240x135. Hardware font/driver appearance still requires device testing.

Release validation checks receive-only application calls, feature/version strings in the real binary, source SHA256 provenance, ESP32-S3 image headers, partition offsets, size and merged embedded components. See the delivered BUILD-REPORT.md for actual results and exact changed files.

No physical device, RF accuracy/calibration, GNSS reception, antenna or SD performance test is implied. Test outside with a clear sky; compare raw NMEA with the detection list, unplug GNSS to verify expiry, reboot to verify saved history, and verify the displayed UTC/geotags independently. Software cannot guarantee reception in all regions or receiver modes. New records save immediately; repeated changes can lose up to 30 seconds on abrupt power loss.

The new satellite_rf host group checks supported tuning limits, constant noise as measurements rather than detections, sample-gap rejection, counter rollover, invalid samples, coordinate bounds, checksum corruption, write/commit failure recovery and last-64 archive retention. The 63 UI fixtures include experimental target setup, custom setup, live/paused/empty states, history and receiver location. No physical satellite reception is established.
