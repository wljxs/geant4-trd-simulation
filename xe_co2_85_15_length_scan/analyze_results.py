#!/usr/bin/env python3
"""Aggregate per-region and total gas energy depositions."""

import argparse
import csv
import math
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
import ROOT

from run_scan import (FOIL_THICKNESS_UM, GAP_THICKNESS_UM,
                      RADIATOR_LENGTHS_CM, point_name)

HERE = Path(__file__).resolve().parent
REGIONS = 21
GAS_LENGTH_CM = 4.0


def mean_sem(values):
    values = np.asarray(values, dtype=float)
    mean = float(np.mean(values))
    sem = float(np.std(values, ddof=1) / np.sqrt(values.size)) \
        if values.size > 1 else 0.0
    return mean, sem


def read_point(path, length_cm):
    frame = ROOT.RDataFrame("events", str(path))
    columns = frame.AsNumpy([
        "energy_keV", "tr_energy_keV", "total_energy_keV",
        "tr_total_energy_keV", "primary_total_energy_loss_keV",
    ])
    energy = np.vstack([np.asarray(row) for row in columns["energy_keV"]])
    tr_energy = np.vstack(
        [np.asarray(row) for row in columns["tr_energy_keV"]])
    events = energy.shape[0]
    totals = mean_sem(columns["total_energy_keV"])
    tr_totals = mean_sem(columns["tr_total_energy_keV"])
    primary_totals = mean_sem(columns["primary_total_energy_loss_keV"])
    regions = []
    for region in range(REGIONS):
        regions.append((mean_sem(energy[:, region]),
                        mean_sem(tr_energy[:, region])))
    return (events, totals, tr_totals, primary_totals, regions,
            np.asarray(columns["total_energy_keV"], dtype=float),
            np.asarray(columns["tr_total_energy_keV"], dtype=float))


def is_complete_root_file(path):
    try:
        root_file = ROOT.TFile.Open(str(path), "READ")
    except OSError:
        return False
    valid = bool(root_file) and not root_file.IsZombie() and bool(
        root_file.Get("events"))
    if root_file:
        root_file.Close()
    return valid


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--allow-incomplete", action="store_true")
    args = parser.parse_args()
    data_directory = HERE / "data"
    plot_directory = HERE / "plots"
    data_directory.mkdir(parents=True, exist_ok=True)
    plot_directory.mkdir(parents=True, exist_ok=True)

    summary_rows = []
    region_rows = []
    curves = {}
    total_energy_samples = {}
    tr_total_energy_samples = {}
    missing = []
    period_um = FOIL_THICKNESS_UM + GAP_THICKNESS_UM
    for length_cm in RADIATOR_LENGTHS_CM:
        path = HERE / "root" / point_name(length_cm) / "trd_electron.root"
        if not path.exists() or not is_complete_root_file(path):
            missing.append(path)
            continue
        (events, total, tr_total, primary_total, regions,
         total_samples, tr_total_samples) = read_point(path, length_cm)
        periods = length_cm * 10_000 // period_um
        summary_rows.append({
            "radiator_length_cm": length_cm,
            "actual_radiator_length_cm": periods * period_um / 10_000,
            "periods": periods,
            "events": events,
            "mean_total_energy_deposit_keV": total[0],
            "sem_total_energy_deposit_keV": total[1],
            "mean_total_tr_energy_deposit_keV": tr_total[0],
            "sem_total_tr_energy_deposit_keV": tr_total[1],
            "mean_primary_energy_loss_keV": primary_total[0],
            "sem_primary_energy_loss_keV": primary_total[1],
        })
        curves[length_cm] = [value[0][0] for value in regions]
        total_energy_samples[length_cm] = total_samples
        tr_total_energy_samples[length_cm] = tr_total_samples
        for region, (energy, tr_energy) in enumerate(regions, start=1):
            region_rows.append({
                "radiator_length_cm": length_cm,
                "gas_region": region,
                "gas_z_start_cm": (region - 1) * GAS_LENGTH_CM / REGIONS,
                "gas_z_end_cm": region * GAS_LENGTH_CM / REGIONS,
                "mean_energy_deposit_keV": energy[0],
                "sem_energy_deposit_keV": energy[1],
                "mean_tr_energy_deposit_keV": tr_energy[0],
                "sem_tr_energy_deposit_keV": tr_energy[1],
            })

    if missing and not args.allow_incomplete:
        raise SystemExit(f"Missing {len(missing)} ROOT files; run run_scan.py first")
    if not summary_rows:
        raise SystemExit("No ROOT results found")

    with (data_directory / "summary.csv").open("w", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=list(summary_rows[0]))
        writer.writeheader()
        writer.writerows(summary_rows)
    with (data_directory / "regions.csv").open("w", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=list(region_rows[0]))
        writer.writeheader()
        writer.writerows(region_rows)

    # Use common 2 keV bins so all six event-level total-deposition spectra
    # can be compared directly. The upper edge expands to include every event.
    bin_width_keV = 2.0
    maximum = max(float(np.max(values))
                  for values in total_energy_samples.values())
    upper_edge = max(100.0, np.ceil(maximum / 20.0) * 20.0)
    bin_edges = np.arange(0.0, upper_edge + bin_width_keV, bin_width_keV)
    spectrum_rows = []
    spectrum_counts = {}
    spectrum_root = ROOT.TFile.Open(
        str(data_directory / "total_energy_spectra.root"), "RECREATE")
    for length_cm, values in total_energy_samples.items():
        counts, _ = np.histogram(values, bins=bin_edges)
        spectrum_counts[length_cm] = counts
        histogram = ROOT.TH1D(
            f"total_energy_{length_cm}cm",
            f"{length_cm} cm radiator;Total gas energy deposit (keV);Events",
            len(bin_edges) - 1, float(bin_edges[0]), float(bin_edges[-1]))
        for index, count in enumerate(counts, start=1):
            histogram.SetBinContent(index, int(count))
        histogram.SetEntries(len(values))
        histogram.Write()
        for index, count in enumerate(counts):
            spectrum_rows.append({
                "radiator_length_cm": length_cm,
                "bin_low_keV": bin_edges[index],
                "bin_high_keV": bin_edges[index + 1],
                "bin_center_keV": 0.5 * (bin_edges[index] + bin_edges[index + 1]),
                "event_count": int(count),
                "probability_per_bin": count / len(values),
            })
    spectrum_root.Close()
    with (data_directory / "total_energy_spectra.csv").open(
            "w", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=list(spectrum_rows[0]))
        writer.writeheader()
        writer.writerows(spectrum_rows)

    figure, axis = plt.subplots(figsize=(9, 5.5), constrained_layout=True)
    for length_cm, values in curves.items():
        axis.plot(range(1, REGIONS + 1), values, marker="o", markersize=3,
                  label=f"{length_cm} cm radiator")
    axis.set(xlabel="Gas region (upstream to downstream)",
             ylabel="Mean energy deposit (keV/event)",
             title="Xe/CO2 85:15, 4 cm gas: per-region energy deposition")
    axis.set_xticks(range(1, REGIONS + 1))
    axis.grid(alpha=0.25)
    axis.legend(ncol=2)
    figure.savefig(plot_directory / "energy_deposit_by_region.png", dpi=180)
    plt.close(figure)

    lengths = [row["radiator_length_cm"] for row in summary_rows]
    means = [row["mean_total_energy_deposit_keV"] for row in summary_rows]
    errors = [row["sem_total_energy_deposit_keV"] for row in summary_rows]
    figure, axis = plt.subplots(figsize=(7, 5), constrained_layout=True)
    axis.errorbar(lengths, means, yerr=errors, marker="o", capsize=3)
    axis.set(xlabel="Radiator thickness (cm)",
             ylabel="Mean total energy deposit (keV/event)",
             title="Total deposition in the 4 cm Xe/CO2 85:15 gas gap")
    axis.grid(alpha=0.25)
    figure.savefig(plot_directory / "total_energy_vs_radiator_length.png",
                   dpi=180)
    plt.close(figure)

    spectrum_columns = math.ceil(len(spectrum_counts) / 2)
    figure, axes = plt.subplots(2, spectrum_columns,
                               figsize=(4 * spectrum_columns, 8), sharex=True,
                               sharey=True, constrained_layout=True)
    for axis, (length_cm, counts) in zip(axes.flat, spectrum_counts.items()):
        axis.stairs(counts, bin_edges, color="tab:blue", linewidth=1.2)
        axis.set_title(f"{length_cm} cm radiator")
        axis.grid(alpha=0.2)
    for axis in axes.flat[len(spectrum_counts):]:
        axis.set_visible(False)
    figure.supxlabel("Total energy deposited in 4 cm gas (keV/event)")
    figure.supylabel("Events / 2 keV")
    figure.suptitle("Xe/CO2 85:15 total energy-deposition spectra")
    figure.savefig(plot_directory / "total_energy_spectra.png", dpi=180)
    plt.close(figure)

    figure, axis = plt.subplots(figsize=(9, 5.5), constrained_layout=True)
    for length_cm, counts in spectrum_counts.items():
        axis.stairs(counts / np.sum(counts), bin_edges,
                    label=f"{length_cm} cm", linewidth=1.2)
    axis.set(xlabel="Total energy deposited in 4 cm gas (keV/event)",
             ylabel="Probability / 2 keV",
             title="Normalized total energy-deposition spectra")
    axis.grid(alpha=0.2)
    axis.legend(ncol=2)
    figure.savefig(plot_directory / "total_energy_spectra_overlay.png",
                   dpi=180)
    plt.close(figure)

    thick_spectra = {
        length: counts for length, counts in spectrum_counts.items()
        if length >= 7
    }
    if thick_spectra:
        thick_columns = 2
        thick_rows = math.ceil(len(thick_spectra) / thick_columns)
        figure, axes = plt.subplots(
            thick_rows, thick_columns, figsize=(10, 4 * thick_rows),
            sharex=True, sharey=True, constrained_layout=True)
        axes_flat = np.atleast_1d(axes).flat
        for axis, (length_cm, counts) in zip(
                axes_flat, thick_spectra.items()):
            axis.stairs(counts, bin_edges, color="tab:blue", linewidth=1.2)
            axis.set_title(f"{length_cm} cm radiator")
            axis.grid(alpha=0.2)
        for axis in list(axes_flat)[len(thick_spectra):]:
            axis.set_visible(False)
        figure.supxlabel("Total energy deposited in 4 cm gas (keV/event)")
        figure.supylabel("Events / 2 keV")
        figure.suptitle("7-10 cm radiator: total energy-deposition spectra")
        figure.savefig(
            plot_directory / "total_energy_spectra_7to10cm.png", dpi=180)
        plt.close(figure)

        figure, axis = plt.subplots(figsize=(9, 5.5), constrained_layout=True)
        for length_cm, counts in thick_spectra.items():
            axis.stairs(counts / np.sum(counts), bin_edges,
                        label=f"{length_cm} cm", linewidth=1.2)
        axis.set(xlabel="Total energy deposited in 4 cm gas (keV/event)",
                 ylabel="Probability / 2 keV",
                 title="7-10 cm normalized total energy-deposition spectra")
        axis.grid(alpha=0.2)
        axis.legend(ncol=2)
        figure.savefig(
            plot_directory / "total_energy_spectra_7to10cm_overlay.png",
            dpi=180)
        plt.close(figure)

    # TR-only contribution to the event-level 21-region sum.
    tr_maximum = max(float(np.max(values))
                     for values in tr_total_energy_samples.values())
    tr_upper_edge = max(100.0, np.ceil(tr_maximum / 20.0) * 20.0)
    tr_bin_edges = np.arange(
        0.0, tr_upper_edge + bin_width_keV, bin_width_keV)
    tr_spectrum_rows = []
    tr_spectrum_counts = {}
    tr_spectrum_root = ROOT.TFile.Open(
        str(data_directory / "tr_total_energy_spectra.root"), "RECREATE")
    for length_cm, values in tr_total_energy_samples.items():
        counts, _ = np.histogram(values, bins=tr_bin_edges)
        tr_spectrum_counts[length_cm] = counts
        histogram = ROOT.TH1D(
            f"tr_total_energy_{length_cm}cm",
            f"{length_cm} cm radiator;TR total gas energy deposit (keV);Events",
            len(tr_bin_edges) - 1, float(tr_bin_edges[0]),
            float(tr_bin_edges[-1]))
        for index, count in enumerate(counts, start=1):
            histogram.SetBinContent(index, int(count))
        histogram.SetEntries(len(values))
        histogram.Write()
        for index, count in enumerate(counts):
            tr_spectrum_rows.append({
                "radiator_length_cm": length_cm,
                "bin_low_keV": tr_bin_edges[index],
                "bin_high_keV": tr_bin_edges[index + 1],
                "bin_center_keV": 0.5 * (
                    tr_bin_edges[index] + tr_bin_edges[index + 1]),
                "event_count": int(count),
                "probability_per_bin": count / len(values),
            })
    tr_spectrum_root.Close()
    with (data_directory / "tr_total_energy_spectra.csv").open(
            "w", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=list(tr_spectrum_rows[0]))
        writer.writeheader()
        writer.writerows(tr_spectrum_rows)

    tr_spectrum_columns = math.ceil(len(tr_spectrum_counts) / 2)
    figure, axes = plt.subplots(2, tr_spectrum_columns,
                               figsize=(4 * tr_spectrum_columns, 8), sharex=True,
                               sharey=True, constrained_layout=True)
    for axis, (length_cm, counts) in zip(
            axes.flat, tr_spectrum_counts.items()):
        axis.stairs(counts, tr_bin_edges, color="tab:red", linewidth=1.2)
        axis.set_yscale("log")
        axis.set_title(f"{length_cm} cm radiator")
        axis.grid(alpha=0.2)
    for axis in axes.flat[len(tr_spectrum_counts):]:
        axis.set_visible(False)
    figure.supxlabel("TR-origin energy deposited in 4 cm gas (keV/event)")
    figure.supylabel("Events / 2 keV")
    figure.suptitle("Xe/CO2 85:15 TR total energy-deposition spectra")
    figure.savefig(plot_directory / "tr_total_energy_spectra.png", dpi=180)
    plt.close(figure)

    figure, axis = plt.subplots(figsize=(9, 5.5), constrained_layout=True)
    for length_cm, counts in tr_spectrum_counts.items():
        axis.stairs(counts / np.sum(counts), tr_bin_edges,
                    label=f"{length_cm} cm", linewidth=1.2)
    axis.set_yscale("log")
    axis.set(xlabel="TR-origin energy deposited in 4 cm gas (keV/event)",
             ylabel="Probability / 2 keV",
             title="Normalized TR total energy-deposition spectra")
    axis.grid(alpha=0.2)
    axis.legend(ncol=2)
    figure.savefig(plot_directory / "tr_total_energy_spectra_overlay.png",
                   dpi=180)
    plt.close(figure)
    print(f"Wrote {len(summary_rows)} summary rows and {len(region_rows)} region rows")


if __name__ == "__main__":
    main()
