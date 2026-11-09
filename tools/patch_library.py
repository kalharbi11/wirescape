# SPDX-License-Identifier: GPL-3.0-only
"""Validate patch JSON or copy it to an explicitly selected local patch folder."""
import argparse
import json
import math
import os
from pathlib import Path
import tempfile

ROOT = Path(__file__).resolve().parents[1]
LIMIT = 2 * 1024 * 1024


def need(ok, message):
    if not ok:
        raise ValueError(message)


def number(value):
    return type(value) in (int, float) and math.isfinite(value)


def validate(data, catalogue, shared=False):
    need(isinstance(data, dict), "patch must be an object")
    need(data.get("format") == "wirescape.native.patch" and data.get("version") == 2,
         "export a native patch v2 from Studio")
    modules, cables = data.get("modules"), data.get("connections")
    need(isinstance(modules, list) and 0 < len(modules) <= 32, "require 1–32 modules")
    need(isinstance(cables, list) and len(cables) <= 128, "require <=128 connections")
    registry = {item["id"]: item for item in catalogue["modules"]}
    nodes = {}
    for module in modules:
        need(isinstance(module, dict), "invalid module")
        ident = module.get("id")
        need(type(ident) is int and 0 <= ident <= 0xffffffff and ident not in nodes,
             "invalid/duplicate node ID")
        descriptor = registry.get(module.get("module"))
        need(descriptor is not None, f"module unavailable: {module.get('module')}")
        need(module.get("version", descriptor["version"]) == descriptor["version"],
             f"module version differs from catalogue: {module.get('module')}")
        params = module.get("parameters", {})
        need(isinstance(params, dict), "invalid parameters")
        specs = {p["id"]: p for p in descriptor["parameters"]}
        for key, value in params.items():
            spec = specs.get(key)
            need(spec is not None and number(value), f"unknown/nonfinite parameter: {key}")
            need(spec["minimum"] <= value <= spec["maximum"], f"parameter out of range: {key}")
        nodes[ident] = descriptor

    def port(endpoint, direction, kind):
        need(isinstance(endpoint, dict), "invalid endpoint")
        descriptor = nodes.get(endpoint.get("module"))
        need(descriptor is not None, "dangling module reference")
        matches = [p for p in descriptor["ports"] if p["id"] == endpoint.get("port")
                   and p["direction"] == direction and p["kind"] == kind]
        need(bool(matches), f"missing/wrong-kind {direction} port: {endpoint}")
        return descriptor

    for cable in cables:
        need(isinstance(cable, dict), "invalid connection")
        kind = cable.get("kind")
        need(kind in ("signal", "event", "modulation"), "invalid connection kind")
        port(cable.get("from"), "output", "event" if kind == "event" else "signal")
        if kind == "modulation":
            target = cable.get("to")
            need(isinstance(target, dict), "invalid modulation target")
            descriptor = nodes.get(target.get("module"))
            need(descriptor is not None, "dangling modulation target")
            need(any(p["id"] == target.get("port") and p.get("modulatable")
                     for p in descriptor["parameters"]), "target is not a modulatable parameter")
            amount = cable.get("amount", 1)
            need(number(amount) and -1 <= amount <= 1, "modulation amount outside [-1,1]")
        else:
            port(cable.get("to"), "input", kind)
    routes = data.get("outputs")
    need(isinstance(routes, list) and routes, "explicit output routes required")
    for route in routes:
        port(route, "output", "signal")
        bus = route.get("bus_channel")
        need(type(bus) is int and 0 <= bus < 16, "invalid output bus")
    if shared:
        need("name" not in data or (isinstance(data["name"], str) and 0 < len(data["name"]) <= 240), "shared patch name must be 1–240 characters")
        need(not data.get("devices"), "remove private device bindings before sharing")
        need(not any(m["module"].startswith("midi.") for m in modules), "web library excludes hardware MIDI modules; use host.keyboard")
        settings = data.get("settings", {})
        need(isinstance(settings, dict), "invalid settings")
        need(settings.get("sample_rate", 48000) == 48000, "shared fast lane uses 48 kHz")
        need(all(r["bus_channel"] in (0, 1) for r in routes), "shared fast lane is stereo")
    return sorted({m["module"] for m in modules})


def read_patch(path, catalogue, shared=False):
    need(path.is_file() and not path.is_symlink(), f"not a regular patch file: {path}")
    need(path.suffix.lower() == ".json" and path.stat().st_size <= LIMIT, "JSON limit is 2 MiB")
    raw = path.read_bytes()
    data = json.loads(raw.decode("utf-8"), parse_constant=lambda x: (_ for _ in ()).throw(
        ValueError("nonfinite JSON: " + x)))
    return raw, validate(data, catalogue, shared)


def install(path, dest, raw, replace=False):
    dest.mkdir(parents=True, exist_ok=True)
    target = dest / path.name
    need(not target.is_symlink(), "refusing symbolic-link destination")
    if target.exists():
        need(replace, f"{target} exists; use --replace for an intentional edit")
        need(target.is_file(), "destination is not a file")
        # Preserve a recoverable copy; never overwrite an earlier backup.
        with tempfile.NamedTemporaryFile(prefix=path.name + ".", suffix=".bak",
                                         dir=dest, delete=False) as backup:
            backup.write(target.read_bytes())
        print("Backup:", backup.name)
    fd, temporary = tempfile.mkstemp(prefix="." + path.name + ".", dir=dest)
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(raw)
        if replace:
            os.replace(temporary, target)
        else:
            # Atomic no-clobber even if a file appears after the earlier check.
            os.link(temporary, target)
    finally:
        Path(temporary).unlink(missing_ok=True)
    print("Installed:", target)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("check", "install"))
    parser.add_argument("source", type=Path)
    parser.add_argument("--dest", type=Path, help="actual folder shown by desktop Save As")
    parser.add_argument("--replace", action="store_true")
    parser.add_argument("--shared", action="store_true", help="48k/stereo/no-device sharing profile")
    args = parser.parse_args()
    try:
        catalogue = json.loads((ROOT / "apps/studio/public/wirescape_modules.json").read_text())
        paths = sorted(args.source.glob("*.json")) if args.source.is_dir() else [args.source]
        need(bool(paths) or (args.action == "check" and args.source.is_dir()), "no JSON patches found")
        prepared = [(p, *read_patch(p, catalogue, args.shared)) for p in paths]
        if args.action == "install":
            need(args.dest is not None, "install requires --dest; use the actual desktop patch folder")
            for path, raw, _ in prepared:
                install(path, args.dest, raw, args.replace)
        for path, _, modules in prepared:
            web = "desktop MIDI required" if any(m.startswith("midi.") for m in modules) else "web module set"
            print(f"PASS {path}: {len(modules)} module types; {web}; structural check only")
    except (OSError, ValueError, KeyError, TypeError) as error:
        parser.exit(1, f"Patch error: {error}\n")


if __name__ == "__main__":
    main()

