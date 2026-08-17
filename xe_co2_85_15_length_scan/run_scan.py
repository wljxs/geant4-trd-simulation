#!/usr/bin/env python3
"""Run the fixed 25/100 um PE/air radiator-length scan."""

import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import csv
import os
from pathlib import Path
import subprocess

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parent
EXECUTABLE = PROJECT / "build" / "trd"

RADIATOR_LENGTHS_CM = list(range(1, 11))
FOIL_THICKNESS_UM = 25
GAP_THICKNESS_UM = 100
MOMENTUM_GEV = 5


def point_name(length_cm):
    return f"radiator_{length_cm}cm"


def macro_text(length_cm, events, output_directory):
    period_um = FOIL_THICKNESS_UM + GAP_THICKNESS_UM
    periods = length_cm * 10_000 // period_um
    actual_length_cm = periods * period_um / 10_000
    return f"""# Auto-generated Xe/CO2 85:15 detector-gas scan
# 5 GeV/c e-, PE/air={FOIL_THICKNESS_UM}/{GAP_THICKNESS_UM} um
# requested radiator={length_cm} cm, periods={periods}, actual={actual_length_cm:g} cm
/trd/beam/particle e-
/trd/beam/momentum {MOMENTUM_GEV} GeV
/trd/radiator/material G4_POLYETHYLENE
/trd/radiator/foilThickness {FOIL_THICKNESS_UM} um
/trd/radiator/gapThickness {GAP_THICKNESS_UM} um
/trd/radiator/totalLength {length_cm} cm
/trd/detector/gas XeCO2_85_15
/trd/output/directory {output_directory}
/run/initialize
/run/beamOn {events}
"""


def prepare_point(length_cm, events):
    name = point_name(length_cm)
    macro = HERE / "macros" / f"{name}.mac"
    output_directory = HERE / "root" / name
    macro.parent.mkdir(parents=True, exist_ok=True)
    output_directory.mkdir(parents=True, exist_ok=True)
    macro.write_text(macro_text(length_cm, events, output_directory),
                     encoding="utf-8")
    return name, macro, output_directory / "trd_electron.root"


def run_point(item, force):
    name, macro, output_file = item
    log = HERE / "logs" / f"{name}.log"
    log.parent.mkdir(parents=True, exist_ok=True)
    if output_file.exists() and not force:
        return name, "SKIP", output_file
    environment = os.environ.copy()
    environment.pop("TRD_TR_ONLY", None)
    result = subprocess.run(
        [str(EXECUTABLE), str(macro)], cwd=PROJECT, env=environment,
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
        check=False,
    )
    log.write_text(result.stdout, encoding="utf-8")
    if result.returncode:
        raise RuntimeError(f"{name} failed; see {log}")
    return name, "DONE", output_file


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--events", type=int, default=50_000)
    parser.add_argument("--jobs", type=int, default=1)
    parser.add_argument("--force", action="store_true")
    parser.add_argument("--generate-only", action="store_true")
    parser.add_argument("--limit", type=int,
                        help="Run only the first N thicknesses")
    args = parser.parse_args()
    if args.events <= 0 or args.jobs <= 0:
        parser.error("--events and --jobs must be positive")
    if not EXECUTABLE.exists():
        parser.error(f"executable not found: {EXECUTABLE}")

    points = [prepare_point(length, args.events)
              for length in RADIATOR_LENGTHS_CM]
    print(f"Generated {len(points)} macros in {HERE / 'macros'}", flush=True)
    if args.generate_only:
        return

    selected = points[:args.limit] if args.limit is not None else points
    failures = []
    with (HERE / "scan_status.csv").open("w", newline="") as status_file:
        writer = csv.writer(status_file)
        writer.writerow(["point", "status", "root_file"])
        with ThreadPoolExecutor(max_workers=args.jobs) as executor:
            futures = [executor.submit(run_point, item, args.force)
                       for item in selected]
            for future in as_completed(futures):
                try:
                    name, status, output = future.result()
                    writer.writerow([name, status, output])
                    status_file.flush()
                    print(f"{status:4s} {name}: {output}", flush=True)
                except Exception as error:
                    failures.append(str(error))
                    writer.writerow(["unknown", "FAIL", str(error)])
                    status_file.flush()
                    print(f"FAIL {error}", flush=True)
    if failures:
        raise SystemExit("\n".join(failures))


if __name__ == "__main__":
    main()
