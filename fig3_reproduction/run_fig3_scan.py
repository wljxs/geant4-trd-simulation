#!/usr/bin/env python3
"""Run the twelve Geant4 configurations shown in Andronic Fig. 3."""

import argparse
import os
from pathlib import Path
import subprocess

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parent
EXECUTABLE = PROJECT / "build" / "trd"

CONFIGS = {
    "gamma": [
        ("gamma_391", 0.2, 15.0, 300.0),
        ("gamma_978", 0.5, 15.0, 300.0),
        ("gamma_1957", 1.0, 15.0, 300.0),
        ("gamma_3914", 2.0, 15.0, 300.0),
    ],
    "foil": [
        ("foil_5um", 2.0, 5.0, 300.0),
        ("foil_10um", 2.0, 10.0, 300.0),
        ("foil_20um", 2.0, 20.0, 300.0),
        ("foil_40um", 2.0, 40.0, 300.0),
    ],
    "gap": [
        ("gap_30um", 2.0, 15.0, 30.0),
        ("gap_100um", 2.0, 15.0, 100.0),
        ("gap_300um", 2.0, 15.0, 300.0),
        ("gap_900um", 2.0, 15.0, 900.0),
    ],
}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--events", type=int, default=10_000)
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args()
    if args.events <= 0:
        parser.error("--events must be positive")

    for group, configs in CONFIGS.items():
        for name, momentum, foil, gap in configs:
            outdir = HERE / "geant4" / group / name
            outfile = outdir / "trd_electron.root"
            if outfile.exists() and not args.force:
                print(f"SKIP {group}/{name}: {outfile}", flush=True)
                continue
            env = os.environ.copy()
            env.update({
                "TRD_TR_ONLY": "1",
                "TRD_XTR_MODEL": "gammaM",
                "TRD_FOIL_MATERIAL": "G4_POLYETHYLENE",
                "TRD_RADIATOR_LAYERS": "100",
                "TRD_FOIL_THICKNESS_UM": str(foil),
                "TRD_RADIATOR_GAP_UM": str(gap),
                "TRD_MOMENTUM_GEV": str(momentum),
                "TRD_OUTPUT_DIR": str(outdir),
            })
            print(f"RUN  {group}/{name}: model=gammaM, p={momentum} GeV/c, "
                  f"l1={foil} um, l2={gap} um, N={args.events}", flush=True)
            log = outdir / "run.log"
            outdir.mkdir(parents=True, exist_ok=True)
            result = subprocess.run(
                [str(EXECUTABLE), "e-", str(args.events)], cwd=PROJECT,
                env=env, text=True, stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT, check=False)
            log.write_text(result.stdout)
            if result.returncode:
                raise SystemExit(f"Geant4 failed for {group}/{name}; see {log}")
            print(f"DONE {group}/{name}: {outfile}", flush=True)


if __name__ == "__main__":
    main()
