# 2.8.0 — Observe & Compare (2026-10-01)

- Added 30-second experimental RF timeline, four sourced target profiles, twelve named sessions, phase summaries and 32 persistent bookmarks.
- Added guarded within-session Before/During/After comparisons, fresh receiver quality context, gap/excess interval and prior-save diagnostics.
- Full append-only session recall is separate from rolling recent history; checksummed metadata writes are streamed and old v2.7 records retain unknown context.
- Preserved live-only monitoring when session storage is unavailable. GNSS and receive-only boundaries remain.
- Added independent-unit offline trace comparison and host/layout regression coverage. No orbit prediction or new claims of satellite detection.

# 2.7.0 — Skywatch experimental RF

## New in 2.7: Experimental satellite RF

Open **Satellites → Experimental satellite RF**. This is separate from receiver-identified GNSS satellites:

- **ISS UHF / 437.800 MHz** is a monitoring target, not a claim of ISS reception. ARISS documents this repeater downlink; station operation can change. Check [ARISS status](https://www.ariss.org/current-status-of-iss-stations.html) before an observation. Frequency verified 30 September 2026; operating status is not cached as a live fact.
- **Other UHF / custom target** accepts 387.000–464.000 MHz, within the selected CC1101 band. Use an independently verified downlink for an amateur satellite or CubeSat. No list of purportedly receivable satellites is invented. A compatible frequency does not establish practical reception with the Cap or attached antenna.
- **RF observation history / SD** recalls the last 64 measured windows, newest first, including those without above-gate samples. Neither a row nor its target label means a satellite was detected.

After checking a suitable UHF antenna, press Enter to monitor. Left/right changes a **manual** offset in 1 kHz steps, limited to ±15 kHz; this is not automatic Doppler tracking or proof of Doppler. The fixed receive filter is 58 kHz. Up/down changes the displayed RSSI gate; Enter pauses/resumes; A toggles automatic logging; L stops reception and opens history; Esc stops reception and returns. GNSS-only mode blocks starting the RF receiver. Custom setup uses F to edit the nominal frequency. Existing general RF frequency settings are preserved.

Every eligible ten-second sampled window records mean/peak RSSI, sample count, largest sample gap, gate and number of samples at/above that gate. The gate is an absolute user-set threshold, not a measured noise floor or satellite confidence. At least 100 valid samples and no gap over 250 ms are required. Invalid RSSI, pausing, retuning, gate/logging changes or leaving monitoring discard the partial window. The nominal sample schedule is 50 ms; display updates are 250 ms. Actual timing is recorded, not guaranteed, and reception is not continuous between samples.

The SD archive `/rfexplorer/satellite-rf-v27.csv` keeps a rotating **last 64** windows, replacing the oldest when full. Each row has a checksum; malformed rows are ignored. Writes use a temporary file and backup; failures are shown, and pending RAM rows are not proof of SD persistence. A failure can lose pending rows on power loss. Completed windows are also appended as `SAT_RF_OBSERVATION` to the ordinary session CSV when that logger is available; its RSSI column is the window mean. These observations never enter GNSS detections, burst fingerprints or satellite identity counts.

Saved UTC and fresh receiver coordinates are captured at window completion. Coordinates describe the receiver, not the satellite. Missing fixes remain unknown. No orbit model, satellite sky/geographic position, pass prediction, voice/audio, SSTV image, packet decoding or satellite identification is provided by this experimental mode. The CC1101 operates receive-only. Physical reception with this exact setup remains unverified.

Archive CSV fields are `frequency_khz,utc_epoch,span_ms,samples,max_gap_ms,above_gate_samples,gate_dbm,mean_dbm_x10,peak_dbm_x10,located,receiver_lat_e6,receiver_lon_e6,target,checksum`. Target 0 denotes the ISS monitoring preset; 1 denotes a custom frequency. Checksums detect accidental corruption, not authenticity. UTC 0 means unknown.

# 2.6.0 — Skywatch

## New in 2.6: Satellites

Open **Satellites** from the home menu. Each category shows the number currently detected. Enter opens live details; **L** opens the category's SD history. Detection history also recalls all categories together. Use **comma / slash** to switch saved details between the reported sky position and the receiver geotag.

| Category | What it is | Supported receiver signal |
| --- | --- | --- |
| GPS | United States global navigation constellation | L1 |
| GLONASS | Russian global navigation constellation | R1 |
| Galileo | European global navigation constellation | E1 |
| BeiDou | Chinese global navigation constellation; generations are not guessed from IDs | B1I / B1C |
| QZSS | Japanese regional navigation and augmentation constellation | L1 |
| SBAS | Satellite-based navigation augmentation; availability is regional | L1 |

These capabilities come from [M5Stack's GPS v1.1 specification](https://docs.m5stack.com/en/unit/Unit_GPS_v1.1). ID ranges and signal fields follow the [CASIC receiver protocol linked by M5Stack](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1173/CASIC_Multi-mode_Satellite_Navigation_Receiver_Protocol_Specification.pdf). Reception depends on antenna placement, sky visibility, region and the receiver's enabled modes. The app does not enable additional modes automatically or promise simultaneous reception of all systems.

**Detected** means a checksum-validated, complete receiver GSV cycle reports a supported constellation/ID/signal with positive C/N0, no more than three seconds old. C/N0 is shown in **dB-Hz**, not RSSI or a confidence percentage. This is receiver-reported reception, not independent authentication. Blank/zero C/N0, unknown talkers, unsupported IDs/signals and stale reports cannot become new detections. Missing signal IDs are accepted for known constellations without claiming a particular signal band. Raw reports remain available for diagnostics. A listed satellite is not necessarily tracked or used in a position fix.

**Position** means reported azimuth/elevation in the receiver's sky, with north at the top. Missing angles remain unknown. The separate saved latitude/longitude is the **receiver's position at a valid detection fix**, never the satellite's geographic position. No orbit propagation or satellite latitude/longitude is invented. Live detail retains its selected identity if reception stops.

**Logging:** `/rfexplorer/satellite-log-v26.csv` stores up to 192 constellation-plus-ID records, qualified report counts, first/last detection UTC, last C/N0, sky angles, signal ID, fix-use state and the last valid receiver geotag. Counts are receiver reports, not distinct visits or packets. New records save immediately; repeat updates flush every 30 seconds, so abrupt power loss can lose that interval. Save failures are shown. UTC remains unknown until available; an earlier unknown timestamp is never backdated. The archive is an aggregate logbook, not a full per-report trajectory. Older v21/original files are preserved; legacy sightings remain unverified until freshly received under the new rules, and do not inflate qualified counts. There is no automatic eviction at capacity.

The CC1101 is a sub-GHz RSSI receiver in this application, not a satellite identity decoder. No ISS, weather-satellite, Starlink or other generic RF satellite-detection categories are included. [TI's CC1101 specification](https://www.ti.com/product/CC1101) describes its hardware limits.

Non-RF journal frequency/RSSI fields are blank. GNSS quality history leaves missing C/N0 as gaps and stale HDOP unknown. The SD archive warns when full. Additional accuracy fixes reject implausible RF readings, keep weak continuation windows below the 10 dB qualification margin out of saved activity, prefer fresh satellite reports over stale stronger ones, reject duplicate IDs within a GSV cycle, and keep ambiguous GSA membership unknown. These are software safeguards, not a calibration or guarantee that every received signal can be identified.


# 2.5 Fieldwork

- Aged quiet reference with new-sample updates, activity freeze and explicit relearn.
- Measured sample/display/save timing and reduced detailed-monitor redraw cadence.
- Five-second sampled activity windows, distinct from burst fingerprints and counts.
- Independent persistent investigation/review flags and per-encounter receiver context.
- v25 schema with preserved v24/older migration; qualified v24 evidence retained.
- Fifth dossier page, second timing page and Fine Scan accuracy wording.
- 17 host groups and expanded UI fixtures; see BUILD-REPORT for actual outcomes.

# 2.4.1

- Two-row Fixed Monitor controls and increased footer clearance.
- SD-backed Investigate list with explicit review reasons and dossier/retune access.
- Existing v24 storage and evidence qualification preserved.

# 2.4.0 — Evidence

- Unified completed-burst evidence gate for detection, matching and memory storage.
- Reject isolated/zero-span and undersampled events; separate unresolved activity and sustained energy.
- Freeze event peak, quiet, sampled span, UTC and receiver location; count repeated saves once.
- Compare against prior records before insertion, preventing self-matches.
- Explicit sampling-gap diagnostics and bounded start-to-start recurrence statistics.
- 1,024-sample raw capture/export and replay through the same firmware detector.
- Preserve older dossiers as unverified imports in a separate v24 store; add review-first sorting.
- Reject estimated/manual/simulated GNSS positions and invalid optional measurements.
- Reset sweep floors and reject incomplete fine-scan estimates.
- Add reported-case, genuine-burst, storage migration, timing, replay and UI regression coverage.

# 2.3.0 — Expedition

- Six themed menus with remembered positions and compact native artwork.
- Smoothed battery estimate, slower visible changes and a charging indicator.
- Corrected missing GNSS measurement freshness timestamps and unknown GSA fields.
- Travel live-data page and bounded 64-point breadcrumb view.
- Selectable route intervals, GNSS-only mode and gap-preserving desktop GPX export.
- Aircraft-compatible distance plausibility gate, conservative quality checks and persisted session summary.
- Immediate gap detection between scheduled samples; recording stops on SD-open failure.
- Recovery preserves pending paths and separates its marker from interrupted CSV rows.
- Removed accidental dossier export from the GNSS screen.
- Added navigation, battery, recorder, CSV and GPX regression checks and expanded UI previews.
- Source base: user-supplied RFExplorer-v2.2.zip; existing RF, NFC read-only and memory behavior retained.

# 2.2.0 — Expedition

- Adds reusable named journeys and recording sessions.
- Adds a dedicated Expedition screen and main-menu entry.
- Adds persistent journey and session names using Preferences.
- Adds fresh-fix-only GNSS route recording.
- Adds explicit GAP records when a previously valid position fix is lost.
- Distinguishes fresh GNSS data from stale position and unavailable position.
- Persists active-session state and path metadata so interrupted sessions can be detected after reboot.
- Prevents journey or session renaming while recording is active.
- Updates firmware identity, build helper and packaged binaries to 2.2.0.
- Retains the Understand intelligence layer, dossiers, RF memory, baselines, GNSS satellite model and receive-only CC1101 behaviour from earlier releases.
# 2.0.0 â€” Understand

- Refreshed field UI, compact thematic menu line art and shared visual tokens.
- Four-page dossiers: identity, timestamped encounter history, analyst evidence, family/baseline.
- Conservative anchored family hypotheses; user-captured RSSI baseline and explicit deviation rule.
- Boot journal metrics and cumulative CSV snapshots; dossier interpretation export.
- Separate v20 memory file, v1.8/v1.9 migration, immediate new-entry/edit writes and throttled repeats.
- Updated version, build entry point, image packaging, binary markers and host/render regressions.
- Preserved RF/GNSS acquisition and receive-only boundaries; no protocol or transmitter claims.

# v1.2 Ã¢â‚¬â€ 15 September 2026

- Added a Mayhem-inspired Looking Glass: sequential narrow-filter RSSI sweeps,
  72-row waterfall, cursor marker, peak hold and reset.
- Added Signal Finder activity counting, a threshold-linked marker beep, direct
  monitor handoff and eight named persistent frequency bookmarks.
- Maintained direct receive only. The new display labels and documentation make
  clear that it is not an SDR FFT, packet detector or signal identifier.
- Preserved v1.1 fine scan, Signal Inspector, CSV observations, NFC read-only
  behaviour, antenna presets and official Cap RF switch control.

## v1.1 Ã¢â‚¬â€ 14 September 2026

- Automatic Ã‚Â±100 kHz refinement in 5 kHz steps after a coarse hit; three sweeps,
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
# v1.6 Ã¢â‚¬â€ GNSS Explorer

- Added GNSS fix dimensionality (2D/3D) and PDOP/VDOP parsing from NMEA GSA.
- Added a retained raw NMEA monitor reachable from the GNSS dashboard with `N`.
- Kept the v1.5 receive-only RF, NFC, SD logging, field-intelligence and satellite-observatory functionality intact.
- Firmware compilation remains pending in this environment; no 1.6 binaries are claimed until a compatible PlatformIO runtime is available.
## 1.6.1 Ã¢â‚¬â€ GNSS field-test follow-up

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
# 2.1.0 â€” Position / Time / Spectrum

- Audited RF/GNSS paths for stale fixes, invalid coordinates, checksum handling, NaN samples, UART starvation and CSV write errors.
- Satellites now retain first and latest valid receiver geotags, with migration, recovery and duplicate-sighting protection.
- UTC synchronises from valid GNSS RMC/ZDA time after boot, with leap/date/fraction validation and holdover status.
- Waterfall now maps 0â€“60 dB above the sweep floor to a 32-step indigo/blue/cyan/green/yellow/orange/red/white palette.
- Added focused GPS, satellite-logbook, event-CSV and spectrum regressions plus 18 rendered UI views.

