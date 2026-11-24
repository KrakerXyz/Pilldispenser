# Local builds and OTA

This checkout uses upstream `main` at `2dc95f4dd7f6bc56445161e6359c97daf1f168a3` as its baseline. The original Arduino source identifies itself as v1.9; the upstream v1.9 Git tag predates the actual v1.9 source update, so do not use that tag as the build baseline.

## Setup and build

Run inside this checkout on dev-env:

```sh
./scripts/setup.sh
./scripts/build.sh
```

Arduino CLI comes from the dev-env image. Setup installs Espressif's ESP32 Arduino core **2.0.17** under the persistent `~/.arduino15`, and creates a project Python environment with pyserial 3.5. Rerunning setup verifies the active core and skips installed dependencies. The build uses the repository's bundled libraries, including its LVGL and TFT display configuration.

The board is ESP32 Dev Module, 4 MB flash, DIO, 80 MHz flash, no PSRAM, and the `min_spiffs` partition scheme. Each firmware slot holds 1,966,080 bytes. SPIFFS holds 131,072 bytes, using 4096-byte blocks and 256-byte pages.

Build outputs:

- `build/PillDispenser.ino.bin`: application firmware for OTA.
- `build/PillDispenser.spiffs.bin`: web files for filesystem OTA.
- `build/manifest.json`: source identity, actual core/CLI versions, input hash, output sizes and checksums.
- Additional bootloader, partition and ELF files are build artifacts, not application OTA uploads.

The current touchscreen revision identifies itself as `v.1.9-grid`. The build compiles a staged copy and defaults to that label without rewriting the working source. Override the label for subsequent builds:

```sh
PILL_BUILD_LABEL=v.1.9-grid.2 ./scripts/build.sh
```

## Touchscreen changes and checks

The home screen shows all ten numbered trays in their configured colors. A green border records a dispense command for that day, including manually dispensed trays. It does not confirm that pills physically arrived. Tap a tray to confirm a single dispense; when it belongs to a ready color group, the dialog also offers dispensing or skipping that group. Group membership is limited to the ready trays shown when the action is accepted. A tray already dispensed today gets a repeat warning. Completion returns to the tray grid without a separate Take Pills screen.

Config contains Wi-Fi, tray settings, alignment, System, and Display. System holds the installed-tray count and clock/reset settings; Display holds the clock screensaver and screen-off action. The passcode is configured through the web page. Audio playback and Telegram integration have been removed from the firmware, web settings and vendored libraries. Their old saved keys are no longer read. Other saved settings remain in place.

Touchscreen, physical-button and web motor commands share one controller reservation. Saving settings, resetting, or changing Wi-Fi reserves that controller before a restart; a busy web save/reset returns HTTP 409. Motor commands are rejected while a restart is reserved. The main loop performs the delayed restart, not the asynchronous web callback.

The maintained touchscreen source is `Arduino/libraries/ui/src/`, including `ui_flow.c` and the screen files; sketch event bindings live in `Arduino/PillDispenser/`. The original `Squareline Studio/` project is a historical reference and no longer reproduces the current UI. Re-exporting it over the maintained files would overwrite the redesign.

Run the host controller checks before building:

```sh
./scripts/test-controller.sh
./scripts/build.sh
```

The host checks exercise the actual controller code with recorded motor, clock and persistence effects. They cannot verify the touchscreen hardware or pill delivery. Before relying on the updated device, use empty trays to check single-tray confirmation, a ready same-color group, Cancel, Skip, repeat warnings, return to the grid, alignment, and rejection of save/reset requests during movement. Check the Config pages and verify saved tray settings after a reboot. Do not repeatedly request a dispense to investigate a delayed response.

## Snapshot and OTA

Take a read-only settings/state snapshot:

```sh
python3 scripts/ota.py snapshot --host 10.8.0.155 --expected-id 65F4E9D4
```

For the grid revision, upload the matching filesystem image first and wait for the device to return:

```sh
python3 scripts/ota.py upload --host 10.8.0.155 --expected-id 65F4E9D4 \
  --kind filesystem --image build/PillDispenser.spiffs.bin --manifest build/manifest.json
```

Then upload firmware, wait for recovery, and compare settings and tray state:

```sh
python3 scripts/ota.py upload --host 10.8.0.155 --expected-id 65F4E9D4 \
  --kind firmware --image build/PillDispenser.ino.bin --manifest build/manifest.json
```

Both operations reboot the dispenser. The uploader checks device identity, image size/hash, current reminders, and post-reboot health. It makes one upload attempt and never retries a POST automatically. It sends no dispense, dismiss, reset, save or alarm/test commands.

Active reminders are held in memory and would be cleared by a reboot. The script blocks uploads while any reminder is active. Handle the reminder on the device first. `--allow-active-alert` exists only for an explicitly accepted reminder-clearing reboot. Normal reboot also moves the servos to their locked positions.

The build manifest declares the six removed audio/Telegram controls. The uploader accepts only those declared removals and compares all retained settings and persisted tray state. Inspect the saved comparison before retrying an update that reports an unexpected change.

Snapshots and upload evidence are private files under `.local/ota/`, excluded from Git. They include rendered settings and tray state; they are not a full NVS/flash backup and do not include the saved Wi-Fi password. Firmware and filesystem OTA preserve NVS settings. Inspect evidence before retrying any failed or uncertain update.

The upstream web footer says SPIFF v1.8 even in its v1.9 release files. Do not infer an old filesystem from this label alone.

## Recovery files

Official v1.9 recovery images are saved in `.local/recovery/` from:

https://github.com/Shaztech/Pilldispenser/releases/tag/v1.9

The built partition table was compared byte-for-byte with the official release. A failed filesystem mount can prevent the web server from starting, in which case recovery requires the board's USB connection. Do not upload bootloader or partition images through the firmware OTA form.

## Previous baseline deployment — 2026-11-12

Before the touchscreen redesign, both locally built baseline images were successfully uploaded to the existing dispenser:

- Firmware OTA returned HTTP 200 `OK`; the device rebooted and reported `v.1.9-local`.
- Filesystem OTA returned HTTP 200 `OK`; the device rebooted and the served `webcmd.html` matched the local source byte-for-byte.
- Device identity, rendered settings, and saved tray state matched before and after both updates. The fresh preflight had no active reminders, so no alert override was needed.
- Evidence is under `.local/ota/20261112-firmware/` and `.local/ota/20261112-filesystem/`.

No dispensing or alarm-control commands were used in these checks. Physical touchscreen and dispensing operation were not tested remotely.

## Grid deployment — 2026-11-12

The matching filesystem was uploaded first, followed by firmware. Both uploads returned HTTP 200 `OK`, and both recovery checks passed. The final firmware label is `v.1.9-grid`.

- Device identity, retained settings, and persisted tray states were preserved in both comparisons, with no unexpected changes.
- The first reboot cleared the active Dinner reminders for trays 7, 8 and 9, as explicitly authorized. No active reminders were cleared by the firmware stage.
- The filesystem stage removed the six declared audio/Telegram controls as expected.
- Evidence: `.local/ota/grid-filesystem/` and `.local/ota/grid-firmware/`, including `health.json`, `comparison.json`, and `upload-response.json`.

These checks verified the uploads, recovery and saved state; physical touchscreen and dispensing operation still require an on-device check.
