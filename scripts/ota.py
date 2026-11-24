#!/usr/bin/env python3
"""Snapshot or update this dispenser through its existing ElegantOTA interface.

Examples (run from the checkout):
  python3 scripts/ota.py snapshot --host 10.8.0.155 --expected-id DEVICE_ID
  python3 scripts/ota.py upload --host 10.8.0.155 --expected-id DEVICE_ID \
    --kind firmware --image build/PillDispenser.ino.bin --manifest build/manifest.json

Evidence contains private settings. It is stored under .local/ota with private
Unix permissions. The script never dispenses, dismisses, saves settings, resets
settings, or sends test messages. An OTA reboot itself moves the servos to their
normal locked positions and clears any active, unpersisted tray alerts.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import sys
import time
from datetime import datetime, timezone
from html.parser import HTMLParser
from urllib.error import HTTPError, URLError
from urllib.parse import urlsplit
from urllib.request import Request, build_opener, HTTPRedirectHandler, ProxyHandler
import uuid


TRAY_FIELDS = ("tr_name", "tr_color", "tr_inst", "tr_enab", "tr_trig",
               "tr_disp", "tr_dism", "tr_time")
MAX_FIRMWARE = 1966080
FILESYSTEM_SIZE = 131072
# Only these controls were deliberately removed with the audio/Telegram features.
# This is not a general-purpose settings ignore list.
REMOVABLE_SETTINGS = frozenset(("alertinterval", "spkvolume", "mutebutton",
                                "bottoken", "chatID", "telegramalert"))


class RefuseRedirects(HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        # Do not follow an unexpected redirect to a device-control route.
        return None


class SettingsParser(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.controls = []
        self.text = []
        self.select = None
        self.option = None
        self.textarea = None

    def handle_starttag(self, tag, attrs):
        a = dict(attrs)
        if tag == "input" and a.get("name"):
            kind = a.get("type", "text").lower()
            if kind not in ("submit", "button", "reset", "image", "file"):
                self.controls.append({"tag": tag, "name": a["name"],
                                      "type": kind, "value": a.get("value", ""),
                                      "checked": "checked" in a,
                                      "disabled": "disabled" in a})
        elif tag == "select" and a.get("name"):
            self.select = {"tag": tag, "name": a["name"], "options": [],
                           "multiple": "multiple" in a, "disabled": "disabled" in a}
        elif tag == "option" and self.select is not None:
            self.option = {"value": a.get("value"), "label": "",
                           "selected": "selected" in a, "disabled": "disabled" in a}
        elif tag == "textarea" and a.get("name"):
            self.textarea = {"tag": tag, "name": a["name"], "value": "",
                             "disabled": "disabled" in a}

    def handle_data(self, data):
        self.text.append(data)
        if self.option is not None:
            self.option["label"] += data
        if self.textarea is not None:
            self.textarea["value"] += data

    def handle_endtag(self, tag):
        if tag == "option" and self.option is not None:
            self.option["label"] = self.option["label"].strip()
            if self.option["value"] is None:
                self.option["value"] = self.option["label"]
            self.select["options"].append(self.option)
            self.option = None
        elif tag == "select" and self.select is not None:
            options = self.select["options"]
            selected = [o["value"] for o in options if o["selected"]]
            if not selected and options and not self.select["multiple"]:
                selected = [next((o["value"] for o in options if not o["disabled"]), "")]
            self.select["value"] = selected
            self.controls.append(self.select)
            self.select = None
        elif tag == "textarea" and self.textarea is not None:
            self.controls.append(self.textarea)
            self.textarea = None

    def version(self):
        text = " ".join(" ".join(self.text).split())
        match = re.search(r"Current Firmware Ver\.:\s*([^\s<]+)", text)
        return match.group(1) if match else None


def utc_now():
    return datetime.now(timezone.utc).isoformat()


def private_directory(path):
    path.mkdir(mode=0o700, parents=True, exist_ok=True)
    if path.is_symlink():
        raise ValueError("Evidence directory must not be a symbolic link")
    path.chmod(0o700)
    return path


def write_private(path, content):
    flags = os.O_WRONLY | os.O_CREAT | os.O_TRUNC | getattr(os, "O_NOFOLLOW", 0)
    fd = os.open(path, flags, 0o600)
    with os.fdopen(fd, "w", encoding="utf-8") as handle:
        handle.write(content)
    path.chmod(0o600)


def write_json(path, value):
    write_private(path, json.dumps(value, indent=2, ensure_ascii=False) + "\n")


class Device:
    def __init__(self, host, timeout):
        url = urlsplit(host if "://" in host else "http://" + host)
        if (url.scheme not in ("http", "https") or not url.hostname or url.username
                or url.password or url.path not in ("", "/") or url.query or url.fragment):
            raise ValueError("--host must be a device hostname/IP, optionally with scheme and port")
        self.base = f"{url.scheme}://{url.netloc}"
        self.timeout = timeout
        self.opener = build_opener(ProxyHandler({}), RefuseRedirects())

    def request(self, path, data=None, headers=None, timeout=None):
        method = "POST" if data is not None else "GET"
        if method == "GET":
            allowed = path in ("/", "/update/identity") or any(
                re.fullmatch(r"/" + field + r"\?id=(?:[1-9]|10)", path)
                for field in TRAY_FIELDS)
        else:
            allowed = path == "/update"
        if not allowed:
            raise ValueError("Refused a route outside the snapshot/OTA allowlist")
        req = Request(self.base + path, data=data, headers=headers or {}, method=method)
        try:
            response = self.opener.open(req, timeout=timeout or self.timeout)
        except HTTPError as error:
            response = error
        with response:
            raw = response.read(2 * 1024 * 1024 + 1)
            if len(raw) > 2 * 1024 * 1024:
                raise ValueError("Device response exceeded the size limit")
            return {"status": response.status, "body": raw.decode("utf-8", errors="replace")}

    def identity(self, expected_id, timeout=None):
        result = self.request("/update/identity", timeout=timeout)
        if result["status"] != 200:
            raise ValueError("Device identity endpoint did not return HTTP 200")
        identity = json.loads(result["body"])
        if identity.get("hardware") != "ESP32":
            raise ValueError("Device did not identify itself as ESP32")
        if expected_id and str(identity.get("id", "")).upper() != expected_id.upper():
            raise ValueError("Device identity does not match --expected-id")
        return identity


def capture(device, expected_id, directory):
    private_directory(directory)
    identity = device.identity(expected_id)
    root = device.request("/")
    if root["status"] != 200:
        raise ValueError("Device home page did not return HTTP 200")
    parser = SettingsParser()
    parser.feed(root["body"])
    if not parser.controls or not parser.version():
        raise ValueError("Could not recognize dispenser settings and firmware version")
    write_private(directory / "root.html", root["body"])
    trays = {}
    for number in range(1, 11):
        trays[str(number)] = {field: device.request(f"/{field}?id={number}")
                              for field in TRAY_FIELDS}
    result = {"captured_at": utc_now(), "host": device.base, "identity": identity,
              "firmware_version": parser.version(), "settings": parser.controls, "trays": trays}
    write_json(directory / "snapshot.json", result)
    return result


def active_trays(snapshot):
    active = []
    for number, fields in snapshot["trays"].items():
        installed = fields["tr_inst"]
        trigger = fields["tr_trig"]
        if installed["status"] == 200:
            if trigger["status"] != 200 or trigger["body"].strip() not in ("0", "1"):
                raise ValueError("Could not establish active-alert state for every configured tray")
            if trigger["body"].strip() == "1":
                active.append(number)
        elif installed["status"] != 400:
            raise ValueError("Could not establish which trays are configured")
    return active


def validate_removed_settings(value):
    if (not isinstance(value, list) or any(not isinstance(name, str) for name in value)
            or len(value) != len(set(value)) or not set(value) <= REMOVABLE_SETTINGS):
        raise ValueError("Manifest removed_settings must list unique, known audio/Telegram control names")
    return list(value)


def validate_image(args):
    image = Path(args.image)
    if image.suffix.lower() != ".bin":
        raise ValueError("Image must be a .bin file")
    raw = image.read_bytes()
    if args.kind == "firmware":
        if not 24 <= len(raw) <= MAX_FIRMWARE or raw[0] != 0xE9:
            raise ValueError("Firmware must be an ESP32 application image within the 1,966,080-byte OTA slot")
    elif len(raw) != FILESYSTEM_SIZE:
        raise ValueError("Filesystem image must be exactly 131,072 bytes for this partition layout")
    digest = hashlib.sha256(raw).hexdigest()
    expected_version = args.expected_version
    removed_settings = []
    checksums = [args.sha256] if args.sha256 else []
    if args.manifest:
        manifest = json.loads(Path(args.manifest).read_text(encoding="utf-8"))
        removed_settings = validate_removed_settings(manifest.get("removed_settings", []))
        artifact = manifest.get("artifacts", {}).get(image.name)
        if not isinstance(artifact, dict) or not artifact.get("sha256"):
            raise ValueError("Manifest must contain artifacts[image basename].sha256")
        checksums.append(artifact["sha256"])
        if "bytes" in artifact and artifact["bytes"] != len(raw):
            raise ValueError("Image size does not match build manifest")
        if args.kind == "firmware":
            manifest_version = manifest.get("firmware_version")
            if expected_version and manifest_version and expected_version != manifest_version:
                raise ValueError("Expected firmware version disagrees with build manifest")
            expected_version = expected_version or manifest_version
    for expected in checksums:
        if not isinstance(expected, str) or not re.fullmatch(r"[0-9a-fA-F]{64}", expected) or expected.lower() != digest:
            raise ValueError("Image SHA-256 does not match the supplied expected hash")
    if args.kind == "firmware" and not expected_version:
        raise ValueError("Firmware uploads require --expected-version or manifest firmware_version")
    return raw, {"filename": image.name, "kind": args.kind, "bytes": len(raw),
                 "sha256": digest, "md5": hashlib.md5(raw).hexdigest(),
                 "expected_version": expected_version, "hash_verified": bool(checksums),
                 "removed_settings": removed_settings}


def multipart(raw, metadata):
    boundary = "pill-dispenser-" + uuid.uuid4().hex
    # The MD5 must precede the binary so ElegantOTA can read it on the first chunk.
    body = (f"--{boundary}\r\nContent-Disposition: form-data; name=\"MD5\"\r\n\r\n"
            f"{metadata['md5']}\r\n--{boundary}\r\n"
            f"Content-Disposition: form-data; name=\"file\"; filename=\"{metadata['kind']}\"\r\n"
            "Content-Type: application/octet-stream\r\n\r\n").encode()
    body += raw + f"\r\n--{boundary}--\r\n".encode()
    return body, {"Content-Type": f"multipart/form-data; boundary={boundary}",
                  "Connection": "close"}


def wait_healthy(device, expected_id, expected_version, seconds):
    deadline = time.monotonic() + seconds
    attempts = 0
    # ElegantOTA waits one second after its response before rebooting.
    time.sleep(min(2, seconds))
    while time.monotonic() < deadline:
        attempts += 1
        try:
            device.identity(expected_id, timeout=min(5, max(0.1, deadline - time.monotonic())))
            root = device.request("/", timeout=min(5, max(0.1, deadline - time.monotonic())))
            parser = SettingsParser()
            parser.feed(root["body"])
            if (root["status"] == 200 and parser.controls and parser.version()
                    and (not expected_version or parser.version() == expected_version)):
                return {"healthy": True, "attempts": attempts, "firmware_version": parser.version()}
        except (OSError, ValueError, URLError):
            pass
        time.sleep(min(2, max(0, deadline - time.monotonic())))
    return {"healthy": False, "attempts": attempts}


def compare_snapshots(before, after, allow_active, removed_settings=None):
    changes = []
    intentional_alert_resets = []
    declared_removals = set(validate_removed_settings(
        [] if removed_settings is None else removed_settings))
    after_names = {control["name"] for control in after["settings"]}
    actual_removals = sorted({control["name"] for control in before["settings"]
                              if control["name"] in declared_removals
                              and control["name"] not in after_names})
    retained_settings = [control for control in before["settings"]
                         if control["name"] not in actual_removals]
    if before["identity"] != after["identity"]:
        changes.append("device identity")
    # A declared control is exempt only if it disappeared altogether. If it is
    # still present, its value, options, flags and position must remain exact.
    if retained_settings != after["settings"]:
        changes.append("rendered settings")
    if before["trays"].keys() != after["trays"].keys():
        changes.append("tray inventory")
    for number, fields in before["trays"].items():
        if number not in after["trays"]:
            continue
        if fields.keys() != after["trays"][number].keys():
            changes.append(f"tray {number}: fields")
        for field, old in fields.items():
            new = after["trays"][number].get(field)
            if old != new:
                reset = (allow_active and field == "tr_trig" and new is not None
                         and old["status"] == new["status"] == 200
                         and old["body"].strip() == "1" and new["body"].strip() == "0")
                if reset:
                    intentional_alert_resets.append(number)
                else:
                    changes.append(f"tray {number}: {field}")
    return {"unchanged_except_firmware_version": not changes, "changes": changes,
            "authorized_active_alert_resets": intentional_alert_resets,
            "authorized_removed_settings": actual_removals}


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    for command in ("snapshot", "upload"):
        p = sub.add_parser(command)
        p.add_argument("--host", required=True)
        p.add_argument("--expected-id", required=(command == "upload"))
        p.add_argument("--output-dir", help="Private evidence directory; default .local/ota/<UTC timestamp>")
        p.add_argument("--timeout", type=float, default=8, help="Per-GET timeout in seconds")
        if command == "upload":
            p.add_argument("--kind", choices=("firmware", "filesystem"), required=True)
            p.add_argument("--image", required=True)
            p.add_argument("--manifest")
            p.add_argument("--sha256")
            p.add_argument("--expected-version")
            p.add_argument("--upload-timeout", type=float, default=180)
            p.add_argument("--reboot-timeout", type=float, default=120)
            p.add_argument("--allow-active-alert", action="store_true",
                           help="Explicitly accept reboot clearing a pending tray alert; use only with user awareness")
    args = parser.parse_args()
    if args.timeout <= 0 or (args.command == "upload" and (args.upload_timeout <= 0 or args.reboot_timeout <= 0)):
        parser.error("Timeouts must be positive")
    os.umask(0o077)
    output = private_directory(Path(args.output_dir or (
        ".local/ota/" + datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ"))))
    device = Device(args.host, args.timeout)
    try:
        if args.command == "snapshot":
            snapshot = capture(device, args.expected_id, output)
            active = active_trays(snapshot)
            print(f"Snapshot saved privately to {output}; active tray alerts: {len(active)}.")
            return 0
        raw, metadata = validate_image(args)
        write_json(output / "image.json", metadata)
        before = capture(device, args.expected_id, output / "before")
        # Recheck alerts last, immediately before the only permitted write.
        for number, fields in before["trays"].items():
            if fields["tr_inst"]["status"] == 200:
                fields["tr_trig"] = device.request(f"/tr_trig?id={number}")
        before["alerts_rechecked_at"] = utc_now()
        write_json(output / "before" / "snapshot.json", before)
        active = active_trays(before)
        write_json(output / "preflight.json", {"active_trays": active,
                   "allow_active_alert": args.allow_active_alert, "checked_at": utc_now()})
        if active and not args.allow_active_alert:
            raise ValueError("Upload blocked: active tray alerts would be cleared by reboot. Handle them first, or explicitly use --allow-active-alert with user awareness")
        body, headers = multipart(raw, metadata)
        device.identity(args.expected_id)
        print("Uploading once; the request will never be automatically retried.", flush=True)
        post = {"started_at": utc_now()}
        try:
            post.update(device.request("/update", data=body, headers=headers, timeout=args.upload_timeout))
        except (OSError, ValueError, URLError) as error:
            post["transport_error"] = type(error).__name__
        post["finished_at"] = utc_now()
        write_json(output / "upload-response.json", post)
        expected_version = metadata["expected_version"] or before["firmware_version"]
        health = wait_healthy(device, args.expected_id, expected_version, args.reboot_timeout)
        write_json(output / "health.json", health)
        if not health["healthy"]:
            raise ValueError("Device did not return healthy with the expected identity/version before the deadline; no upload retry was attempted")
        after = capture(device, args.expected_id, output / "after")
        if after["firmware_version"] != expected_version:
            raise ValueError("Firmware version changed during post-update verification")
        comparison = compare_snapshots(before, after, args.allow_active_alert,
                                       metadata["removed_settings"])
        write_json(output / "comparison.json", comparison)
        if post.get("status") != 200 or post.get("body", "").strip() != "OK":
            raise ValueError("Device is reachable, but the upload did not return an unambiguous HTTP 200 OK; inspect private evidence before deciding next steps")
        if not comparison["unchanged_except_firmware_version"]:
            raise ValueError("Device recovered, but settings/state changed; inspect private comparison.json")
        removals = comparison["authorized_removed_settings"]
        exception = (" except declared removed controls: " + ", ".join(removals)) if removals else ""
        print(f"OTA verified; settings and persistent tray state preserved{exception}. Evidence: {output}")
        return 0
    except (OSError, ValueError, URLError) as error:
        # Error messages are deliberately about the operation, never setting values.
        message = str(error) if isinstance(error, ValueError) else type(error).__name__
        write_json(output / "error.json", {"at": utc_now(), "error": message})
        print(f"Stopped: {message}. Evidence: {output}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
