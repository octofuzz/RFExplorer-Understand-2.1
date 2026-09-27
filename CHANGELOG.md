# 2.0.0 — Understand

- Refreshed field UI, compact thematic menu line art and shared visual tokens.
- Four-page dossiers: identity, timestamped encounter history, analyst evidence, family/baseline.
- Conservative anchored family hypotheses; user-captured RSSI baseline and explicit deviation rule.
- Boot journal metrics and cumulative CSV snapshots; dossier interpretation export.
- Separate v20 memory file, v1.8/v1.9 migration, immediate new-entry/edit writes and throttled repeats.
- Updated version, build entry point, image packaging, binary markers and host/render regressions.
- Preserved RF/GNSS acquisition and receive-only boundaries; no protocol or transmitter claims.

# v1.2 â€” 15 September 2026

- Added a Mayhem-inspired Looking Glass: sequential narrow-filter RSSI sweeps,
  72-row waterfall, cursor marker, peak hold and reset.
- Added Signal Finder activity counting, a threshold-linked marker beep, direct
  monitor handoff and eight named persistent frequency bookmarks.
- Maintained direct receive only. The new display labels and documentation make
  clear that it is not an SDR FFT, packet detector or signal identifier.
- Preserved v1.1 fine scan, Signal Inspector, CSV observations, NFC read-only
  behaviour, antenna presets and official Cap RF switch control.

## v1.1 â€” 14 September 2026

- Automatic Â±100 kHz refinement in 5 kHz steps after a coarse hit; three sweeps,
  narrow filter, repeatability checks, edge clipping and uncertain-result fallback.
- Signal Inspector with coarse/refined frequency, live RSSI, noise estimate,
  RSSI delta, sampled burst duration/count and honest unknown modulation status.
- Fixed-frequency observation graph, retry/scan controls and labelled SD saves.
- Extended CSV schema with refinement validity, burst metrics and sampling-gap
  diagnostics; preserves ordinary readings and old-log browsing.
- Preserved v1.0 scanner styling, presets, manual settings, hardware pinout,
  documented RF switching, battery, antenna prompts and explicit NFC-A reads.
- No CC1101 transmit/replay feature. Added compile-time analysis contracts.

v1.1 is compiled and software-checked, awaiting physical validation.
# v1.3 - 16 September 2026

- Added direct NMEA GNSS input for the proven Cardputer ADV Grove configuration:
  RX GPIO 1, TX GPIO 2, 115200 baud.
- Added a GNSS dashboard and sky plot. It reports satellites visible from GPS,
  GLONASS, Galileo, BeiDou and QZSS separately when supplied by NMEA GSV data.
- RF observations and automatic SIGNAL/BURST rows now include valid GPS position,
  altitude, speed, course, fix/visible satellite totals and HDOP.
- A valid GNSS time automatically sets the session UTC clock.
- Added a receive-only RF Catalogue of focused 315/433/868/915 MHz survey profiles.
# v1.4 - Individual satellite observatory

- Added scrollable Individual Satellites page from GNSS sky screen.
- Shows constellation, reported PRN/SVID, elevation/azimuth, SNR and USED/unknown status.
- Retained Cardputer ADV Grove UART RX GPIO 1, TX GPIO 2, 115200 baud.
- GSA active satellite IDs are associated with GSV entries where the receiver provides an unambiguous talker ID; unknown status is shown rather than guessed.
# v1.5 - Field Intelligence

- Based directly on the supplied RFExplorer v1.4 source and its compiled artifacts.
- Added receive-only encounter, watch, survey and RSSI-hunt data contracts.
- Added explicit CC1101 supported-band validation and RSSI bar scaling.
- Retained v1.4's individual satellite observatory, NMEA/GNSS handling and all prior scanner functionality.
# v1.6 â€” GNSS Explorer

- Added GNSS fix dimensionality (2D/3D) and PDOP/VDOP parsing from NMEA GSA.
- Added a retained raw NMEA monitor reachable from the GNSS dashboard with `N`.
- Kept the v1.5 receive-only RF, NFC, SD logging, field-intelligence and satellite-observatory functionality intact.
- Firmware compilation remains pending in this environment; no 1.6 binaries are claimed until a compatible PlatformIO runtime is available.
## 1.6.1 â€” GNSS field-test follow-up

- Expanded the Raw NMEA monitor from 8 to 64 retained lines with scrolling and selectable ALL/GSV/GSA/GGA/RMC/ZDA/TXT filters (`F`).
- Retains overlong input markers in ALL mode so malformed receiver output remains diagnosable.
- Refined AT6668 combined-GN SVID classification for GLONASS, Galileo, BeiDou and QZSS while preserving explicit NMEA talker IDs.
- `GPTXT ... ANTENNA OPEN` remains diagnostic text and is never treated as a fatal error when a valid fix is present.

## 1.8.0
- Redesigned compact UI with width-aware text fitting to prevent clipping and overflow on the 240x135 display.
- GNSS Explorer dashboard with constellation-coloured sky plot and explicit SBAS count.
- Individual satellite detail view with elevation, azimuth, signal level and fix-use status.
- Persistent SD satellite logbook with first/last seen, best SNR, maximum elevation and sighting counts.
- New-satellite discovery notification and optional sound cue.
- Two-minute GNSS quality history for visible satellites, average SNR and HDOP.
- GNSS diagnostics with NMEA sentence counters, age, checksum failures and truncation counts.
- Automatic GNSS snapshot rows in the normal session CSV, including constellation counts and individual satellites.
- Manual RF waypoint logging with GNSS geotagging using the W key in live/inspector views.
- Build/about screen and consistent 1.7 release packaging.
- Preserves the 1.6.1 multi-constellation SatelliteStore parser and existing RF/NFC/storage features.
# 2.1.0 — Position / Time / Spectrum

- Audited RF/GNSS paths for stale fixes, invalid coordinates, checksum handling, NaN samples, UART starvation and CSV write errors.
- Satellites now retain first and latest valid receiver geotags, with migration, recovery and duplicate-sighting protection.
- UTC synchronises from valid GNSS RMC/ZDA time after boot, with leap/date/fraction validation and holdover status.
- Waterfall now maps 0–60 dB above the sweep floor to a 32-step indigo/blue/cyan/green/yellow/orange/red/white palette.
- Added focused GPS, satellite-logbook, event-CSV and spectrum regressions plus 18 rendered UI views.
