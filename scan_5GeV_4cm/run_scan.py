#!/usr/bin/env python3
"""Generate and run the 5 GeV/c, 4 cm radiator scan."""

import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import csv
import os
from pathlib import Path
import subprocess

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parent
EXECUTABLE = PROJECT / "build" / "trd"

L1_VALUES = [2, 5, 10, 15, 20, 25, 30, 35, 40, 50]
L2_VALUES = [
    10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 150, 200, 250, 300,
    350, 400, 500, 600, 700, 800, 900, 1000,
]
TARGET_LENGTH_UM = 40_000


def point_name(l1, l2):
    return f"l1_{l1:03d}um_l2_{l2:04d}um"


def macro_text(l1, l2, events, output_directory):
    periods = TARGET_LENGTH_UM // (l1 + l2)
    actual_length_um = periods * (l1 + l2)
    return f"""# Auto-generated 5 GeV/c, 4 cm TR radiator scan
# l1={l1} um, l2={l2} um, periods={periods}
# actual radiator length={actual_length_um} um
/trd/beam/particle e-
/trd/beam/momentum 5 GeV
/trd/radiator/material G4_POLYETHYLENE
/trd/radiator/foilThickness {l1} um
/trd/radiator/gapThickness {l2} um
/trd/radiator/totalLength 4 cm
/trd/output/directory {output_directory}
/run/initialize
/run/beamOn {events}
"""


def prepare_point(l1, l2, events):
    name = point_name(l1, l2)
    macro = HERE / "macros" / f"{name}.mac"
    output_directory = HERE / "root" / name
    macro.parent.mkdir(parents=True, exist_ok=True)
    output_directory.mkdir(parents=True, exist_ok=True)
    macro.write_text(macro_text(l1, l2, events, output_directory), encoding="utf-8")
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
                        help="Run only the first N points (for validation)")
    args = parser.parse_args()
    if args.events <= 0 or args.jobs <= 0:
        parser.error("--events and --jobs must be positive")
    if not EXECUTABLE.exists():
        parser.error(f"executable not found: {EXECUTABLE}")

    points = [prepare_point(l1, l2, args.events)
              for l1 in L1_VALUES for l2 in L2_VALUES]
    print(f"Generated {len(points)} macros in {HERE / 'macros'}", flush=True)
    if args.generate_only:
        return
    selected = points[:args.limit] if args.limit is not None else points
    failures = []
    status_path = HERE / "scan_status.csv"
    with status_path.open("w", newline="") as status_file:
        status_writer = csv.writer(status_file)
        status_writer.writerow(["point", "status", "root_file"])
        status_file.flush()
        with ThreadPoolExecutor(max_workers=args.jobs) as executor:
            futures = [executor.submit(run_point, item, args.force)
                       for item in selected]
            for future in as_completed(futures):
                try:
                    name, status, output = future.result()
                    status_writer.writerow([name, status, output])
                    status_file.flush()
                    print(f"{status:4s} {name}: {output}", flush=True)
                except Exception as error:
                    failures.append(str(error))
                    status_writer.writerow(["unknown", "FAIL", str(error)])
                    status_file.flush()
                    print(f"FAIL {error}", flush=True)
    if failures:
        raise SystemExit("\n".join(failures))


if __name__ == "__main__":
    main()
