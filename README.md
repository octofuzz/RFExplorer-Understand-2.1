# RFExplorer Understand 2.1

**A serious, receive-only RF field notebook for the M5Stack Cardputer ADV.** RFExplorer combines CC1101 spectrum discovery, GNSS position and time, SD-backed signal memory, NFC read-only support, and an analyst-style interface in a compact instrument you can carry into the field.

> RFExplorer observes radio activity. It does not transmit, decode private protocols, or identify transmitters.

## What it does

- **Discover RF activity** across the supported CC1101 bands: 300–348 MHz, 387–464 MHz, and 779–928 MHz.
- **Remember encounters** with frequency, RSSI, time, envelope duration where available, notes, categories, and up to 48 dossiers.
- **Tag satellite observations** with the first and latest valid GNSS coordinates where they were seen.
- **Set UTC automatically at boot** from valid GNSS time (RMC/ZDA), while preserving a user-set clock when GNSS time is unavailable.
- **Explore signal dossiers** with identity, history, analyst evidence, family grouping, and user-captured RSSI baselines.
- **Read the colour waterfall**: a 32-colour intensity scale makes weak, medium, and strong activity easier to distinguish across each sweep.
- **Keep a field journal** with session summaries, GNSS-linked observations, repeated matches, new dossiers, family candidates, and baseline deviations.
- **Export evidence** to SD CSV files for later review.
- **Use NFC read-only support** through the Cap accessory; the application does not write to tags.

Measured values and limited inferences are shown separately throughout the interface. A dossier is a local observation record, not a decoded device identity.

## Required hardware

- **M5Stack Cardputer ADV** (ESP32-S3, 8 MB flash)
- **M5Stack Cardputer ADV Cap** with the CC1101 and NFC hardware
- **microSD card** for memory and CSV exports
- **Optional AT6668 GPS v1.1 module** connected to the Cardputer ADV Grove UART

The Cap is required for the CC1101 RF functions and NFC functions. GPS is optional, but required for automatic position tagging and GNSS UTC synchronisation.

## Install the release image

1. Download [`RFExplorer-v2.1-cardputer-adv-merged.bin`](firmware/RFExplorer-v2.1-cardputer-adv-merged.bin) from this repository.
2. Flash the merged image to the Cardputer ADV at **offset `0x0`** using your preferred ESP32 flashing tool.
3. Insert the Cap and microSD card, then boot the Cardputer.
4. If using GPS, connect the AT6668 module before boot and give it a clear view of the sky. The status indicator will show when a valid fix and UTC time are available.

The merged image includes the bootloader, partition table, and application. Existing RFExplorer SD data is migrated rather than erased.

The smaller [`RFExplorer-v2.1-app-only.bin`](firmware/RFExplorer-v2.1-app-only.bin) is for an existing matching installation and must be flashed at **`0x10000`**. Use the merged image for a complete first installation.

## Controls and storage

The main menu uses `;` and `.` to move, Enter to open, and Esc to return. In a dossier, `,` and `/` change pages. `N` edits a name, `T` edits a note, `C` cycles a category, `B` captures a baseline, and `X` exports the interpretation to the session CSV.

Data is stored under `/rfexplorer/` on the SD card. New dossiers, labels, notes, tags, and baselines attempt immediate persistence; repeated sightings are flushed within 30 seconds. A sudden power loss can therefore lose the most recent repeated-sighting updates.

## Hardware limits

CC1101 scanning is a sequential RSSI sweep, not wideband I/Q capture. Frequency, bandwidth, modulation, range, direction, motion, transmitter identity, and protocol interpretation cannot be established from these measurements. Baseline comparisons require the same antenna, receiver setup, and location. GNSS is a separate receiver and does not identify the RF source.

## Build from source

From the project directory:

```powershell
pio run -e cardputer_adv
python tests/run_host_tests.py
python tests/render_ui.py
python tests/validate_release.py
```

The build creates verified images, manifests, hashes, and source provenance files in `firmware/`. See [VALIDATION.md](VALIDATION.md) for the verification scope. Physical Cardputer, display, SD, RF, NFC, and GNSS testing remains a device-side responsibility.

## Project status

RFExplorer Understand 2.1 is a field-oriented receive-only instrument release. Contributions and careful reports from real Cardputer ADV setups are welcome.
