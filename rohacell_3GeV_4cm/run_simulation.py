#!/usr/bin/env python3
"""Run 3 GeV/c e-/pi- with and without a 4 cm HF71-equivalent radiator."""

import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import os
from pathlib import Path
import subprocess

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parent
EXECUTABLE = PROJECT / "build" / "trd"
PARTICLES = {"electron": "e-", "pion": "pi-"}


def macro_text(particle, radiator, events, output_directory, foil_um, gap_um,
               material):
    fraction = foil_um / (foil_um + gap_um)
    compact_density_mg_cm3 = {
        "ROHACELL_HF71": 1100.0,
        "G4_POLYPROPYLENE": 900.0,
    }.get(material)
    density_comment = (f", density={compact_density_mg_cm3 * fraction:.3f} mg/cm3"
                       if compact_density_mg_cm3 is not None else "")
    periods = int(40_000 // (foil_um + gap_um))
    return f"""# 3 GeV/c particle in 4 cm Xe/CO2 95:5
# Effective {material}/air radiator: {foil_um:g}/{gap_um:g} um,
# solid fraction={fraction:.6f}{density_comment},
# periods={periods}, modeled length={periods * (foil_um + gap_um):.3f} um.
/trd/beam/particle {particle}
/trd/beam/momentum 3 GeV
/trd/radiator/enabled {'true' if radiator else 'false'}
/trd/radiator/material {material}
/trd/radiator/foilThickness {foil_um:g} um
/trd/radiator/gapThickness {gap_um:g} um
/trd/radiator/totalLength 4 cm
/trd/detector/gas XeCO2_95_5
/trd/output/directory {output_directory}
/run/initialize
/run/beamOn {events}
"""


def prepare(name, particle, radiator, events, foil_um, gap_um, material, tag):
    macro_base = HERE / "macros" / tag if tag else HERE / "macros"
    root_base = HERE / "root" / tag if tag else HERE / "root"
    log_base = HERE / "logs" / tag if tag else HERE / "logs"
    macro = macro_base / f"{name}.mac"
    output_directory = root_base / name
    macro.parent.mkdir(parents=True, exist_ok=True)
    output_directory.mkdir(parents=True, exist_ok=True)
    macro.write_text(macro_text(particle, radiator, events, output_directory,
                                foil_um, gap_um, material),
                     encoding="utf-8")
    particle_file = "electron" if particle == "e-" else "pion"
    return (name, macro, output_directory / f"trd_{particle_file}.root",
            log_base / f"{name}.log")


def run(item, force):
    name, macro, output, log = item
    log.parent.mkdir(parents=True, exist_ok=True)
    if output.exists() and not force:
        return name, "SKIP", output
    environment = os.environ.copy()
    environment.pop("TRD_TR_ONLY", None)
    environment["TRD_XTR_MODEL"] = "gammaM"
    result = subprocess.run(
        [str(EXECUTABLE), str(macro)], cwd=PROJECT, env=environment,
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
        check=False,
    )
    log.write_text(result.stdout, encoding="utf-8")
    if result.returncode:
        raise RuntimeError(f"{name} failed; see {log}")
    return name, "DONE", output


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--events", type=int, default=50_000)
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--force", action="store_true")
    parser.add_argument("--generate-only", action="store_true")
    parser.add_argument("--foil-um", type=float, default=15.0)
    parser.add_argument("--gap-um", type=float, default=200.0)
    parser.add_argument("--material", default="ROHACELL_HF71",
                        help="Geant4 compact cell-wall material")
    parser.add_argument("--tag", default="",
                        help="Subdirectory tag used to preserve other runs")
    args = parser.parse_args()
    if (args.events <= 0 or args.jobs <= 0 or args.foil_um <= 0 or
            args.gap_um <= 0):
        parser.error("events, jobs, foil thickness, and gap must be positive")
    if args.tag and ("/" in args.tag or "\\" in args.tag or args.tag in {".", ".."}):
        parser.error("--tag must be a single directory name")
    if not EXECUTABLE.exists():
        parser.error(f"executable not found: {EXECUTABLE}")

    items = []
    for label, particle in PARTICLES.items():
        for radiator in (False, True):
            suffix = "rohacell" if radiator else "no_radiator"
            items.append(prepare(f"{label}_{suffix}", particle, radiator,
                                 args.events, args.foil_um, args.gap_um,
                                 args.material, args.tag))
    if args.generate_only:
        print(f"Generated {len(items)} macros in {HERE / 'macros'}")
        return
    with ThreadPoolExecutor(max_workers=min(args.jobs, len(items))) as executor:
        futures = [executor.submit(run, item, args.force) for item in items]
        for future in as_completed(futures):
            name, status, output = future.result()
            print(f"{status:4s} {name}: {output}", flush=True)


if __name__ == "__main__":
    main()
