# RFExplorer 2.1 — Position / Time / Spectrum

Receive-only RF field instrument for M5Stack Cardputer ADV with Cap CC1101/NFC and optional GPS v1.1.
Derived from the verified 2.0 Understand release, with reviewed dossier/UI foundations from v1.9.
RF tuning, scanning, GNSS parsing, NFC read-only application behaviour and partitions are retained.

## Flash yourself

Build from this directory with `pio run -e cardputer_adv` or `./BUILD_2_1.ps1`.
The post-build hook creates and verifies:

- `firmware/RFExplorer-v2.1-cardputer-adv-merged.bin`: standalone image; flash at **0x0**.
- `firmware/RFExplorer-v2.1-app-only.bin`: **0x10000**, only with matching bootloader and partition layout.
- `firmware/manifest.json`: image sizes, SHA-256 hashes, offsets and version.
- `firmware/source-sha256.json`: source provenance.

No device is flashed by the build. Board is ESP32-S3, 8 MB flash, DIO, 80 MHz.
Use the merged image for a full installation. Existing SD data is migrated, not erased.

## Interface and controls

Dark field palette, restrained cyan/amber/green, status indicators and crisp native-resolution
line artwork: radio tower, orbital sky, archive, journal route and instrument panel.
Main menu displays four compact rows. `;`/`.` move, Enter opens, Esc returns.
Status G = fresh valid GNSS fix, S = SD logger, L = automatic logging; battery at right.
RF views include RSSI trace, waterfall, refinement and Inspector evidence pages.
The waterfall maps 0–60 dB above each sweep floor across 32 colours. Satellite logbook entries
retain first and latest valid receiver coordinates. Valid GNSS RMC/ZDA time synchronises UTC after boot.
Discovery Memory: `I` opens a dossier, `O` cycles frequency/most-seen/strongest ordering,
Enter listens. The correct RF band is selected when listening to a stored entry.

Four dossier pages, selected with `,`/`/` (left/right keyboard controls):

1. Identity: frequency, saved sightings, strongest RSSI, first/last UTC, user category and note.
   `N` edits name, `T` edits note, `C` cycles user-assigned categories. These are not decoded identities.
2. History: last 12 saved RSSI samples, with UTC and observed envelope duration where available.
   `;`/`.` selects a sample. The graph uses encounter order, not elapsed time.
3. RF Analyst: measured tuned frequency, RSSI and completed envelope duration, separated from
   the limited inference that the saved fingerprint has recurred. Identity, motion and period remain unknown.
4. Family/baseline: candidate family size and user-captured RSSI baseline. `B` captures the
   mean of the last 6–12 saved RSSI samples; `X` exports the dossier interpretation to session CSV.

Family grouping uses same RF band, <=250 kHz tuned-frequency separation and known envelope
durations within a factor of two. Groups are anchored at the lowest-frequency dossier and
do not chain transitively across the spectrum. Family IDs refer to local dossier indices,
not global or decoded device IDs. Missing duration produces a singleton. Existing >=75/100
fingerprint matching is preserved; that score is a heuristic, not statistical confidence.
Several transmitters can share one fingerprint, and one transmitter can produce several dossiers.

Baseline deviations flag a >=12 dB change from an explicitly captured baseline with >=6 samples.
This is a comparison of saved RSSI observations, not calibrated anomaly detection. Compare the
same antenna, receiver setup and location. There is no automatic place model or absence detector.

Field Journal summarises this boot: logged SIGNAL activity rows, saved detailed observations,
new dossiers, repeated matches, candidate-family observations, baseline deviations, observations
linked to a fresh GNSS fix, strongest saved RSSI and observations not stored because memory is full.
It writes cumulative SESSION snapshots every 30 seconds while dirty, on Back and on Enter in the journal.
Snapshots share the session CSV: use the latest snapshot rather than summing cumulative rows.
Activity rows and detailed observations may describe the same activity; these are not packet counts.
Automatic summaries can lag the source CSV by 30 seconds on sudden power removal.

## Storage and exports

48 dossiers maximum; no silent eviction. Labels/notes are sanitised to 24 characters.
`/rfexplorer/signal-memory-v20.csv` stores dossiers, UTC, last-12 RSSI/duration/time history and baseline.
If absent, 2.0 imports v1.9 CSV or the original `/rfexplorer/signal-memory.csv` without changing it.
Unknown legacy UTC remains unknown. Temp-file replacement retains one backup generation.
New entries, names, notes, tags and baseline changes attempt immediate persistence.
Repeated sightings use a 30-second flush; power removal may lose those recent in-memory changes.
Failed writes keep dirty state for retry. SD failure is reported; boot-relative legacy times are never called UTC.
SESSION and DOSSIER rows supplement existing RF/GNSS-linked CSV records; the full dossier history
is in the v20 memory CSV. UTC comes from a valid GNSS clock or the user-set clock, when available.

## Hardware limits

CC1101 is receive-only in this application: supported bands are 300–348, 387–464 and 779–928 MHz.
Sequential swept RSSI is not wideband I/Q. Tuned/refined frequency is approximate. Receiver filter
width is not occupied bandwidth. RSSI envelopes do not decode protocols or establish transmitter
identity, range, direction, motion, local/mobile origin or periodicity. GNSS is a separate receiver.
No LoRa, ADS-B, VHF airband, protocol decoding, missing-signal alerts or location-baseline claims.
NFC uses only explicit Type-A detection/select and disables its field after the read.

## Validation

Run `python tests/run_host_tests.py`, `python tests/render_ui.py`, and after building,
`python tests/validate_release.py`. Host C++ tests use MSVC Build Tools on Windows or a C++14 compiler elsewhere.
UI renderer requires Pillow, MSVC and the pinned M5GFX fonts resolved by PlatformIO.
See VALIDATION.md for scope. Physical Cardputer, display, SD, RF and GNSS testing remains required.
