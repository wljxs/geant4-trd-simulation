#!/usr/bin/env python3
"""Plot normalized total gas-energy spectra for the four HF71 runs."""

import argparse
import csv
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
import ROOT

HERE = Path(__file__).resolve().parent
RUNS = [
    ("pion_no_radiator", r"$\pi^-$ no radiator", "#5b9bd5"),
    ("pion_rohacell", r"$\pi^-$ {radiator}", "#f5a623"),
    ("electron_no_radiator", r"$e^-$ no radiator", "#45a247"),
    ("electron_rohacell", r"$e^-$ {radiator}", "#4247ff"),
]
BINS = np.arange(-7.5, 152.6, 1.0)


def root_path(name, tag):
    particle = "pion" if name.startswith("pion") else "electron"
    base = HERE / "root" / tag if tag else HERE / "root"
    return base / name / f"trd_{particle}.root"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--tag", default="")
    parser.add_argument("--radiator-label", default="15/200 um")
    parser.add_argument("--radiator-name", default="ROHACELL HF71")
    args = parser.parse_args()
    suffix = f"_{args.tag}" if args.tag else ""
    figure, axis = plt.subplots(figsize=(7.2, 5.4))
    rows = []
    spectrum_rows = []
    for name, label, color in RUNS:
        label = label.format(radiator=args.radiator_name)
        path = root_path(name, args.tag)
        if not path.exists():
            raise SystemExit(f"missing {path}; run run_simulation.py first")
        frame = ROOT.RDataFrame("events", str(path))
        values = np.asarray(
            frame.AsNumpy(["total_energy_keV"])["total_energy_keV"],
            dtype=float,
        )
        counts, edges = np.histogram(values, bins=BINS)
        normalized = counts / values.size
        axis.stairs(normalized, edges, label=label, color=color,
                    linewidth=1.05)
        mode_index = int(np.argmax(counts))
        mode = 0.5 * (edges[mode_index] + edges[mode_index + 1])
        rows.append((name, values.size, values.mean(), values.std(ddof=1),
                     np.median(values), mode, normalized[mode_index],
                     values.max()))
        for index, count in enumerate(counts):
            spectrum_rows.append((name, edges[index], edges[index + 1],
                                  int(count), normalized[index]))

    axis.set_title(
        rf"3 GeV/$c$ energy deposition "
        rf"({args.radiator_name}/air {args.radiator_label})")
    axis.set_xlabel(r"Energy deposited in 4 cm Xe/CO$_2$ (95:5) (keV)")
    axis.set_ylabel("Counts / event / 1 keV")
    axis.set_xlim(-7.5, 150)
    axis.set_ylim(bottom=0)
    axis.grid(True, linestyle="--", alpha=0.28)
    axis.legend(frameon=True)
    figure.tight_layout()
    plots = HERE / "plots"
    plots.mkdir(parents=True, exist_ok=True)
    figure.savefig(plots / f"energy_deposition_3GeV{suffix}.png", dpi=180)
    figure.savefig(plots / f"energy_deposition_3GeV{suffix}.pdf")

    data = HERE / "data"
    data.mkdir(parents=True, exist_ok=True)
    with (data / f"summary{suffix}.csv").open("w", newline="") as output:
        writer = csv.writer(output)
        writer.writerow(["run", "events", "mean_keV", "std_keV",
                         "median_keV", "mode_keV", "peak_probability_per_keV",
                         "maximum_keV"])
        writer.writerows(rows)
    with (data / f"spectra{suffix}.csv").open("w", newline="") as output:
        writer = csv.writer(output)
        writer.writerow(["run", "bin_low_keV", "bin_high_keV", "count",
                         "probability_per_event_per_1keV"])
        writer.writerows(spectrum_rows)
    for row in rows:
        print(f"{row[0]:24s} N={row[1]:6d} mean={row[2]:7.3f} keV "
              f"median={row[4]:7.3f} keV mode={row[5]:5.1f} keV "
              f"peak={row[6]:.4f}")


if __name__ == "__main__":
    main()
