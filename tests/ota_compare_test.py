#!/usr/bin/env python3
"""Offline preservation checks; imports actual uploader without contacting a device."""
from copy import deepcopy
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("ota", Path(__file__).resolve().parents[1] / "scripts/ota.py")
ota = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ota)


def snapshot():
    parser = ota.SettingsParser()
    parser.feed('''<input name="ssid" value="Example network">
      <input name="bottoken" value="synthetic test value">
      <input type="checkbox" name="screensaver" checked>
      <select name="spkvolume"><option value="9" selected>10</option></select>
      <select name="traycount"><option value="10" selected>10</option></select>''')
    return {"identity": {"hardware": "ESP32", "id": "TEST"},
            "firmware_version": "old", "settings": parser.controls,
            "trays": {str(i): {field: {"status": 200, "body": "0"}
                               for field in ota.TRAY_FIELDS} for i in range(1, 11)}}


class PreservationTests(unittest.TestCase):
    def setUp(self):
        self.before = snapshot()
        self.after = deepcopy(self.before)
        self.after["firmware_version"] = "v.1.9-grid"

    def remove(self, *names):
        self.after["settings"] = [x for x in self.after["settings"] if x["name"] not in names]

    def compare(self, names=None, allow_active=False):
        return ota.compare_snapshots(self.before, self.after, allow_active, names)

    def test_unchanged_and_new_firmware(self):
        self.assertTrue(self.compare()["unchanged_except_firmware_version"])
        self.assertEqual(self.compare()["authorized_removed_settings"], [])

    def test_old_manifest_stays_strict(self):
        self.remove("bottoken")
        self.assertEqual(self.compare()["changes"], ["rendered settings"])

    def test_only_declared_missing_controls_are_accepted_and_reported(self):
        self.remove("bottoken", "spkvolume")
        result = self.compare(["bottoken", "spkvolume", "telegramalert"])
        self.assertTrue(result["unchanged_except_firmware_version"])
        self.assertEqual(result["authorized_removed_settings"], ["bottoken", "spkvolume"])

    def test_present_declared_control_value_change_is_rejected(self):
        self.after["settings"][1]["value"] = "changed synthetic value"
        self.assertEqual(self.compare(["bottoken"])["changes"], ["rendered settings"])

    def test_known_but_undeclared_removal_is_rejected(self):
        self.remove("bottoken", "spkvolume")
        self.assertEqual(self.compare(["bottoken"])["changes"], ["rendered settings"])

    def test_unrelated_removal_is_rejected(self):
        self.remove("bottoken", "ssid")
        self.assertEqual(self.compare(["bottoken"])["changes"], ["rendered settings"])

    def test_remaining_values_flags_options_and_order_stay_exact(self):
        for mutation in (lambda x: x[0].update(value="changed"),
                         lambda x: x[2].update(checked=False),
                         lambda x: x[4]["options"][0].update(label="changed"),
                         lambda x: x.reverse(),
                         lambda x: x.append({"tag": "input", "name": "extra", "value": "0"})):
            with self.subTest(mutation=mutation):
                self.after = deepcopy(self.before)
                mutation(self.after["settings"])
                self.assertIn("rendered settings", self.compare(["bottoken"])["changes"])

    def test_duplicate_name_is_not_partially_exempted(self):
        self.before["settings"].append(deepcopy(self.before["settings"][1]))
        self.assertIn("rendered settings", self.compare(["bottoken"])["changes"])

    def test_all_tray_fields_remain_exact(self):
        for field in ota.TRAY_FIELDS:
            with self.subTest(field=field):
                self.after = deepcopy(self.before)
                self.after["trays"]["4"][field]["body"] = "1"
                self.assertEqual(self.compare(["bottoken"])["changes"], [f"tray 4: {field}"])

    def test_missing_or_added_tray_data_is_rejected(self):
        for mutation in (lambda t: t.pop("10"), lambda t: t.update({"11": {}}),
                         lambda t: t["1"].pop("tr_disp"),
                         lambda t: t["1"].update(extra={"status": 200, "body": "0"})):
            with self.subTest(mutation=mutation):
                self.after = deepcopy(self.before)
                mutation(self.after["trays"])
                self.assertFalse(self.compare()["unchanged_except_firmware_version"])

    def test_active_alert_reset_still_requires_separate_permission(self):
        self.before["trays"]["4"]["tr_trig"]["body"] = "1"
        self.assertFalse(self.compare(["bottoken"])["unchanged_except_firmware_version"])
        result = self.compare(["bottoken"], allow_active=True)
        self.assertTrue(result["unchanged_except_firmware_version"])
        self.assertEqual(result["authorized_active_alert_resets"], ["4"])
        self.after["trays"]["4"]["tr_disp"]["body"] = "1"
        self.assertEqual(self.compare(["bottoken"], True)["changes"], ["tray 4: tr_disp"])

    def test_identity_change_is_rejected(self):
        self.after["identity"]["id"] = "OTHER"
        self.assertEqual(self.compare(["bottoken"])["changes"], ["device identity"])

    def test_removal_declaration_is_strict(self):
        for value in (None, "bottoken", {}, [None], ["ssid"], ["chatid"], ["bottoken", "bottoken"]):
            with self.subTest(value=value), self.assertRaises(ValueError):
                ota.validate_removed_settings(value)
        self.assertEqual(set(ota.validate_removed_settings(sorted(ota.REMOVABLE_SETTINGS))), ota.REMOVABLE_SETTINGS)


class ManifestTests(unittest.TestCase):
    def test_exception_requires_manifest_and_matching_artifact_hash(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            raw = bytes([0xE9]) + bytes(23)
            image = root / "test.bin"
            image.write_bytes(raw)
            args = SimpleNamespace(image=str(image), kind="firmware", expected_version="v.1.9-grid",
                                   sha256=None, manifest=None)
            self.assertEqual(ota.validate_image(args)[1]["removed_settings"], [])
            manifest = {"firmware_version": "v.1.9-grid", "removed_settings": ["bottoken"],
                        "artifacts": {image.name: {"bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest()}}}
            args.manifest = str(root / "manifest.json")
            Path(args.manifest).write_text(json.dumps(manifest))
            result = ota.validate_image(args)[1]
            self.assertTrue(result["hash_verified"])
            self.assertEqual(result["removed_settings"], ["bottoken"])
            manifest["artifacts"][image.name]["sha256"] = "0" * 64
            Path(args.manifest).write_text(json.dumps(manifest))
            with self.assertRaisesRegex(ValueError, "SHA-256"):
                ota.validate_image(args)

    def test_filesystem_manifest_and_legacy_manifest(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            raw = bytes(ota.FILESYSTEM_SIZE)
            image = root / "test.spiffs.bin"
            image.write_bytes(raw)
            args = SimpleNamespace(image=str(image), kind="filesystem", expected_version=None,
                                   sha256=None, manifest=str(root / "manifest.json"))
            manifest = {"artifacts": {image.name: {"sha256": hashlib.sha256(raw).hexdigest()}}}
            for declarations in ([], sorted(ota.REMOVABLE_SETTINGS)):
                if declarations:
                    manifest["removed_settings"] = declarations
                Path(args.manifest).write_text(json.dumps(manifest))
                self.assertEqual(ota.validate_image(args)[1]["removed_settings"], declarations)


if __name__ == "__main__":
    # Any accidental device access makes this offline suite fail immediately.
    with patch.object(ota.Device, "request", side_effect=AssertionError("No device access in tests")):
        unittest.main(verbosity=2)
