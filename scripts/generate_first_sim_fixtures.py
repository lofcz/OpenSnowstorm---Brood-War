import argparse
import json
import os
import struct
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent


def parse_args():
    parser = argparse.ArgumentParser(
        description="Generate the first local simulation parity fixtures from a licensed StarCraft install."
    )
    parser.add_argument("--data-dir", required=True, help="Brood War data directory (for example E:\\Starcraft)")
    parser.add_argument("--base-map", help="Base .scm/.scx map to use as a local terrain template")
    parser.add_argument("--bin", help="Path to gfxtest.exe")
    parser.add_argument(
        "--out-dir",
        default=str(ROOT / "tests" / "generated" / "first_sim_family"),
        help="Output directory for generated .chk/.rep/.hashes files",
    )
    return parser.parse_args()


def find_binary(explicit_path):
    if explicit_path:
        return Path(explicit_path)
    candidates = [
        ROOT / "build" / "Release" / "gfxtest.exe",
        ROOT / "build" / "Debug" / "gfxtest.exe",
        ROOT / "dist_temp" / "gfxtest.exe",
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    raise FileNotFoundError("Could not find gfxtest.exe; pass --bin explicitly.")


def find_base_map(data_dir, explicit_map):
    if explicit_map:
        return Path(explicit_map)
    preferred = Path(data_dir) / "maps" / "ladder" / "(4)Lost Temple.scm"
    if preferred.exists():
        return preferred
    for candidate in sorted(Path(data_dir).rglob("*.scm")):
        return candidate
    for candidate in sorted(Path(data_dir).rglob("*.scx")):
        return candidate
    raise FileNotFoundError("Could not find a base map under the provided data directory.")


def run(cmd, cwd=ROOT):
    print("+", " ".join(f'"{c}"' if " " in c else c for c in cmd))
    subprocess.run(cmd, cwd=cwd, check=True)


def parse_enum(enum_name):
    text = (ROOT / "bwenums.h").read_text(encoding="utf-8")
    start = text.index(f"enum struct {enum_name} : int {{")
    end = text.index("};", start)
    block = text[start:end].splitlines()[1:]
    values = {}
    current = 0
    for line in block:
        line = line.split("//", 1)[0].strip().rstrip(",")
        if not line:
            continue
        if "=" in line:
            name, value = [part.strip() for part in line.split("=", 1)]
            current = int(value, 0)
        else:
            name = line
        values[name] = current
        current += 1
    return values


def parse_chunks(data):
    chunks = []
    offset = 0
    while offset + 8 <= len(data):
        tag = data[offset : offset + 4].decode("ascii")
        size = struct.unpack_from("<I", data, offset + 4)[0]
        start = offset + 8
        end = start + size
        if end > len(data):
            raise ValueError(f"Chunk {tag!r} overruns file")
        chunks.append([tag, bytearray(data[start:end])])
        offset = end
    if offset != len(data):
        raise ValueError("Trailing bytes after CHK chunk stream")
    return chunks


def encode_chunks(chunks):
    out = bytearray()
    for tag, payload in chunks:
        out += tag.encode("ascii")
        out += struct.pack("<I", len(payload))
        out += payload
    return bytes(out)


def get_chunk(chunks, tag):
    for chunk_tag, payload in chunks:
        if chunk_tag == tag:
            return payload
    raise KeyError(f"Missing required chunk {tag!r}")


def replace_chunk(chunks, tag, payload):
    for entry in chunks:
        if entry[0] == tag:
            entry[1] = bytearray(payload)
            return
    chunks.append([tag, bytearray(payload)])


def patch_upgrade_levels(payload, count, player, upgrade_id, level):
    expected_size = count * (12 + 12 + 1 + 1 + 12)
    if len(payload) != expected_size:
        raise ValueError(f"Unexpected upgrade-level chunk size {len(payload)} (expected {expected_size})")
    player_max = payload[0 : 12 * count]
    player_cur = payload[12 * count : 24 * count]
    global_max = payload[24 * count : 25 * count]
    player_default = payload[26 * count : 38 * count]

    idx = player * count + upgrade_id
    player_default[idx] = 0
    player_max[idx] = max(player_max[idx], level)
    player_cur[idx] = level
    global_max[upgrade_id] = max(global_max[upgrade_id], level)


def build_unit_entry(unit_id, x, y, owner, hp_percent=100, shield_percent=100, energy_percent=100):
    return struct.pack(
        "<IHHHHHHBBBBIHHII",
        0,
        x,
        y,
        unit_id,
        0,
        0,
        0x0002,
        owner,
        hp_percent,
        shield_percent,
        energy_percent,
        0,
        0,
        0,
        0,
        0,
    )


def find_start_location(base_chk, start_location_id):
    chunks = parse_chunks(base_chk)
    unit_chunk = get_chunk(chunks, "UNIT")
    for offset in range(0, len(unit_chunk), 36):
        _, x, y, unit_id = struct.unpack_from("<IHHH", unit_chunk, offset)
        if unit_id == start_location_id:
            return x, y
    return None


def make_fixture(base_chk, units, upgrade_patch=None, preserve_existing_units=False):
    chunks = parse_chunks(base_chk)
    dims = get_chunk(chunks, "DIM ")
    width, height = struct.unpack_from("<HH", dims, 0)
    start_anchor = find_start_location(base_chk, parse_enum("UnitTypes")["Special_Start_Location"])
    if start_anchor:
        center_x, center_y = start_anchor
    else:
        center_x = width * 16
        center_y = height * 16

    unit_payload = bytearray(get_chunk(chunks, "UNIT")) if preserve_existing_units else bytearray()
    for spec in units(center_x, center_y):
        unit_payload += build_unit_entry(**spec)
    replace_chunk(chunks, "UNIT", unit_payload)

    if upgrade_patch:
        try:
            upgrade_chunk = get_chunk(chunks, "PUPx")
            upgrade_count = 61
        except KeyError:
            upgrade_chunk = get_chunk(chunks, "UPGR")
            upgrade_count = 46
        for player, upgrade_id, level in upgrade_patch:
            if upgrade_id >= upgrade_count:
                raise ValueError(
                    f"Upgrade id {upgrade_id} does not fit in this map's upgrade chunk (count={upgrade_count})"
                )
            patch_upgrade_levels(upgrade_chunk, upgrade_count, player, upgrade_id, level)

    return encode_chunks(chunks)


def write_bytes(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)


def main():
    args = parse_args()
    gfxtest = find_binary(args.bin)
    base_map = find_base_map(args.data_dir, args.base_map)
    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    unit_ids = parse_enum("UnitTypes")
    upgrade_ids = parse_enum("UpgradeTypes")

    base_chk = out_dir / "base_template.chk"
    run(
        [
            str(gfxtest),
            "--data-dir",
            args.data_dir,
            "--map",
            str(base_map),
            "--extract-scenario-chk",
            str(base_chk),
        ]
    )
    base_chk_bytes = base_chk.read_bytes()
    start_anchor = find_start_location(base_chk_bytes, unit_ids["Special_Start_Location"])

    def cooldown_units(cx, cy):
        if start_anchor:
            cx, cy = start_anchor
        return [
            {"unit_id": unit_ids["Terran_Marine"], "x": cx - 160, "y": cy - 24, "owner": 0},
            {"unit_id": unit_ids["Terran_Marine"], "x": cx - 128, "y": cy + 24, "owner": 0},
            {"unit_id": unit_ids["Terran_Marine"], "x": cx - 96, "y": cy - 24, "owner": 0},
            {"unit_id": unit_ids["Zerg_Hydralisk"], "x": cx + 96, "y": cy - 24, "owner": 1},
            {"unit_id": unit_ids["Zerg_Hydralisk"], "x": cx + 128, "y": cy + 24, "owner": 1},
        ]

    def armor_units(cx, cy):
        if start_anchor:
            cx, cy = start_anchor
        return [
            {"unit_id": unit_ids["Terran_Marine"], "x": cx - 160, "y": cy - 24, "owner": 0},
            {"unit_id": unit_ids["Terran_Marine"], "x": cx - 128, "y": cy + 24, "owner": 0},
            {"unit_id": unit_ids["Terran_Marine"], "x": cx - 96, "y": cy - 24, "owner": 0},
            {"unit_id": unit_ids["Zerg_Hydralisk"], "x": cx + 96, "y": cy - 24, "owner": 1},
            {"unit_id": unit_ids["Zerg_Hydralisk"], "x": cx + 128, "y": cy + 24, "owner": 1},
        ]

    def splash_units(cx, cy):
        if start_anchor:
            cx, cy = start_anchor
        return [
            {"unit_id": unit_ids["Terran_Siege_Tank_Siege_Mode"], "x": cx - 192, "y": cy, "owner": 0},
            {"unit_id": unit_ids["Zerg_Hydralisk"], "x": cx + 24, "y": cy, "owner": 1},
            {"unit_id": unit_ids["Zerg_Hydralisk"], "x": cx + 48, "y": cy + 20, "owner": 1},
            {"unit_id": unit_ids["Zerg_Hydralisk"], "x": cx + 76, "y": cy - 28, "owner": 1},
            {"unit_id": unit_ids["Zerg_Hydralisk"], "x": cx + 112, "y": cy + 44, "owner": 1},
            {"unit_id": unit_ids["Zerg_Hydralisk"], "x": cx + 160, "y": cy - 52, "owner": 1},
        ]

    def spell_timing_units(cx, cy):
        if start_anchor:
            cx, cy = start_anchor
        return [
            {"unit_id": unit_ids["Terran_Medic"], "x": cx - 112, "y": cy, "owner": 0, "energy_percent": 100},
            {"unit_id": unit_ids["Terran_Marine"], "x": cx - 144, "y": cy - 28, "owner": 0, "hp_percent": 45},
            {"unit_id": unit_ids["Terran_Marine"], "x": cx - 132, "y": cy + 12, "owner": 0, "hp_percent": 55},
            {"unit_id": unit_ids["Terran_Marine"], "x": cx - 96, "y": cy + 40, "owner": 0, "hp_percent": 65},
            {"unit_id": unit_ids["Terran_Marine"], "x": cx + 80, "y": cy - 20, "owner": 1},
            {"unit_id": unit_ids["Terran_Marine"], "x": cx + 112, "y": cy + 20, "owner": 1},
        ]

    def cloak_detection_units(cx, cy):
        if start_anchor:
            cx, cy = start_anchor
        return [
            {"unit_id": unit_ids["Protoss_Dark_Templar"], "x": cx - 176, "y": cy - 40, "owner": 0},
            {"unit_id": unit_ids["Terran_Marine"], "x": cx - 80, "y": cy - 56, "owner": 1},
            {"unit_id": unit_ids["Terran_Marine"], "x": cx - 56, "y": cy - 24, "owner": 1},
            {"unit_id": unit_ids["Protoss_Dark_Templar"], "x": cx + 16, "y": cy + 40, "owner": 0},
            {"unit_id": unit_ids["Terran_Marine"], "x": cx + 112, "y": cy + 24, "owner": 1},
            {"unit_id": unit_ids["Terran_Marine"], "x": cx + 136, "y": cy + 56, "owner": 1},
            {"unit_id": unit_ids["Terran_Science_Vessel"], "x": cx + 88, "y": cy + 80, "owner": 1, "energy_percent": 100},
        ]

    def transport_unload_units(cx, cy):
        if start_anchor:
            cx, cy = start_anchor
        return [
            {"unit_id": unit_ids["Terran_Dropship"], "x": cx - 176, "y": cy, "owner": 0},
            {"unit_id": unit_ids["Terran_Marine"], "x": cx - 144, "y": cy - 18, "owner": 0},
            {"unit_id": unit_ids["Terran_Marine"], "x": cx - 138, "y": cy + 22, "owner": 0},
            {"unit_id": unit_ids["Zerg_Hydralisk"], "x": cx + 56, "y": cy - 16, "owner": 1},
            {"unit_id": unit_ids["Zerg_Hydralisk"], "x": cx + 88, "y": cy + 16, "owner": 1},
        ]

    def worker_resource_units(cx, cy):
        if start_anchor:
            cx, cy = start_anchor
        return [
            {"unit_id": unit_ids["Terran_Command_Center"], "x": cx - 32, "y": cy - 16, "owner": 0},
            {"unit_id": unit_ids["Terran_SCV"], "x": cx + 36, "y": cy + 12, "owner": 0},
        ]

    fixtures = [
        {
            "name": "combat_cooldown",
            "description": "Baseline Marine-vs-Hydralisk engagement used to lock down attack cadence and cooldown timing.",
            "chk_bytes": make_fixture(base_chk_bytes, cooldown_units),
        },
        {
            "name": "combat_armor_upgrades",
            "description": "Marine-vs-Hydralisk engagement with infantry weapons and carapace levels applied through PUPx.",
            "chk_bytes": make_fixture(
                base_chk_bytes,
                armor_units,
                upgrade_patch=[
                    (0, upgrade_ids["Terran_Infantry_Weapons"], 1),
                    (1, upgrade_ids["Zerg_Carapace"], 1),
                ],
            ),
        },
        {
            "name": "combat_splash",
            "description": "Siege Tank splash scenario with a staggered Hydralisk cluster to lock down inner, medium, and outer radial damage behavior.",
            "chk_bytes": make_fixture(base_chk_bytes, splash_units),
        },
        {
            "name": "spell_timing_healing",
            "description": "Medic heal scenario with staggered wounded Marines to lock down spell cadence, target selection, and energy-window timing.",
            "chk_bytes": make_fixture(base_chk_bytes, spell_timing_units),
        },
        {
            "name": "cloak_detection",
            "description": "Parallel Dark Templar skirmishes that lock down undetected-vs-detected targeting behavior using a nearby Science Vessel detector on only one side.",
            "chk_bytes": make_fixture(base_chk_bytes, cloak_detection_units),
        },
        {
            "name": "transport_unload",
            "description": "Dropship load and move-unload sequence against a Hydralisk pair to lock down cargo pickup, unload timing, and post-unload combat setup.",
            "chk_bytes": make_fixture(base_chk_bytes, transport_unload_units),
            "fixture_script": "transport_unload",
        },
        {
            "name": "worker_resource",
            "description": "SCV mineral gather-and-return loop using preserved map mineral fields to lock down worker targeting, mining cadence, and resource return timing.",
            "chk_bytes": make_fixture(
                base_chk_bytes,
                worker_resource_units,
                preserve_existing_units=True,
            ),
            "fixture_script": "worker_resource",
        },
    ]

    manifest = {
        "data_dir": args.data_dir,
        "base_map": str(base_map),
        "fixtures": [],
    }

    for fixture in fixtures:
        chk_path = out_dir / f"{fixture['name']}.chk"
        rep_path = out_dir / f"{fixture['name']}.rep"
        hashes_path = out_dir / f"{fixture['name']}.hashes"
        write_bytes(chk_path, fixture["chk_bytes"])
        run(
            [
                str(gfxtest),
                "--data-dir",
                args.data_dir,
                "--map",
                str(chk_path),
                "--use-map-settings",
                "--gen-test-replay",
                str(rep_path),
                "--record-hashes",
                str(hashes_path),
                "--frames",
                "360",
                "--hash-interval",
                "24",
                *(
                    ["--fixture-script", fixture["fixture_script"]]
                    if "fixture_script" in fixture
                    else []
                ),
            ]
        )
        run(
            [
                str(gfxtest),
                "--data-dir",
                args.data_dir,
                "--replay",
                str(rep_path),
                "--verify-hashes",
                str(hashes_path),
                "--headless",
            ]
        )
        manifest["fixtures"].append(
            {
                "name": fixture["name"],
                "description": fixture["description"],
                "chk": str(chk_path),
                "replay": str(rep_path),
                "hashes": str(hashes_path),
                "verify_command": [
                    str(gfxtest),
                    "--data-dir",
                    args.data_dir,
                    "--replay",
                    str(rep_path),
                    "--verify-hashes",
                    str(hashes_path),
                    "--headless",
                ],
            }
        )

    manifest_path = out_dir / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    print(f"Wrote fixture bundle to {out_dir}")


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as exc:
        print(f"Command failed with exit code {exc.returncode}", file=sys.stderr)
        sys.exit(exc.returncode)
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        sys.exit(1)
