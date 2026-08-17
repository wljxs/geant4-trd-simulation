#!/usr/bin/env python3
"""Plot primary-particle energy loss in the 4 cm detector gas."""

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
MAIN_MAX_KEV = 200.0
BINS = np.arange(0.0, MAIN_MAX_KEV + 1.0, 1.0)


def root_path(name, tag):
    particle = "pion" if name.startswith("pion") else "electron"
    base = HERE / "root" / tag if tag else HERE / "root"
    return base / name / f"trd_{particle}.root"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--tag", default="")
    parser.add_argument("--radiator-name", default="ROHACELL HF71")
    parser.add_argument("--radiator-label", default="15/200 um")
    args = parser.parse_args()

    suffix = f"_{args.tag}" if args.tag else ""
    figure, (spectrum_axis, tail_axis) = plt.subplots(
        2, 1, figsize=(8.0, 8.3), gridspec_kw={"height_ratios": [1.55, 1.0]})
    summary_rows = []
    spectrum_rows = []

    for name, label, color in RUNS:
        label = label.format(radiator=args.radiator_name)
        path = root_path(name, args.tag)
        if not path.exists():
            raise SystemExit(f"missing {path}; run run_simulation.py first")
        frame = ROOT.RDataFrame("events", str(path))
        values = np.asarray(frame.AsNumpy(
            ["primary_total_energy_loss_keV"])["primary_total_energy_loss_keV"],
            dtype=float)

        counts, edges = np.histogram(values, bins=BINS)
        normalized = counts / values.size
        spectrum_axis.stairs(normalized, edges, label=label, color=color,
                             linewidth=1.15)

        positive = np.sort(values[values > 0.0])
        survival = (positive.size - np.arange(positive.size)) / values.size
        tail_axis.step(positive, survival, where="post", color=color,
                       linewidth=1.05, label=label)

        mode_index = int(np.argmax(counts))
        mode = 0.5 * (edges[mode_index] + edges[mode_index + 1])
        overflow = int(np.count_nonzero(values >= MAIN_MAX_KEV))
        summary_rows.append((
            name, values.size, values.mean(), values.std(ddof=1),
            np.median(values), mode, overflow, overflow / values.size,
            np.quantile(values, 0.90), np.quantile(values, 0.95),
            np.quantile(values, 0.99), values.max()))
        for index, count in enumerate(counts):
            spectrum_rows.append((name, edges[index], edges[index + 1],
                                  int(count), normalized[index]))

    spectrum_axis.set_title(
        rf"3 GeV/$c$ primary-particle energy loss in 4 cm Xe/CO$_2$ (95:5)"
        "\n" + rf"{args.radiator_name}/air {args.radiator_label}")
    spectrum_axis.set_xlabel("Primary energy loss in detector gas (keV)")
    spectrum_axis.set_ylabel("Probability / event / 1 keV")
    spectrum_axis.set_xlim(0.0, MAIN_MAX_KEV)
    spectrum_axis.set_ylim(bottom=0.0)
    spectrum_axis.grid(True, linestyle="--", alpha=0.28)
    spectrum_axis.legend(frameon=True)

    tail_axis.set_xscale("log")
    tail_axis.set_yscale("log")
    tail_axis.set_xlim(10.0, max(row[-1] for row in summary_rows) * 1.05)
    tail_axis.set_ylim(1.0 / summary_rows[0][1], 1.0)
    tail_axis.set_xlabel("Primary energy loss in detector gas (keV)")
    tail_axis.set_ylabel(r"Survival probability $P(\Delta E \geq E)$")
    tail_axis.grid(True, which="both", linestyle="--", alpha=0.28)
    tail_axis.legend(frameon=True, fontsize=8, ncol=2)

    figure.tight_layout()
    plots = HERE / "plots"
    plots.mkdir(parents=True, exist_ok=True)
    figure.savefig(plots / f"primary_energy_loss_3GeV{suffix}.png", dpi=180)
    figure.savefig(plots / f"primary_energy_loss_3GeV{suffix}.pdf")

    data = HERE / "data"
    data.mkdir(parents=True, exist_ok=True)
    with (data / f"primary_energy_loss_summary{suffix}.csv").open(
            "w", newline="") as output:
        writer = csv.writer(output)
        writer.writerow([
            "run", "events", "full_mean_keV", "full_std_keV", "median_keV",
            "mode_keV", "events_ge_200keV", "fraction_ge_200keV", "q90_keV",
            "q95_keV", "q99_keV", "maximum_keV"])
        writer.writerows(summary_rows)
    with (data / f"primary_energy_loss_spectra{suffix}.csv").open(
            "w", newline="") as output:
        writer = csv.writer(output)
        writer.writerow([
            "run", "bin_low_keV", "bin_high_keV", "count",
            "probability_per_event_per_1keV"])
        writer.writerows(spectrum_rows)

    for row in summary_rows:
        print(f"{row[0]:24s} N={row[1]:6d} median={row[4]:8.3f} keV "
              f"mode={row[5]:5.1f} keV q95={row[9]:9.3f} keV "
              f"P(E>=200 keV)={row[7]:.4%} max={row[11]:.3f} keV")


if __name__ == "__main__":
    main()
