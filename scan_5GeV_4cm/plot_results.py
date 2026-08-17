#!/usr/bin/env python3
"""Aggregate ROOT files and produce the 23 requested heat maps."""

import argparse
import csv
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
import ROOT

from run_scan import L1_VALUES, L2_VALUES, TARGET_LENGTH_UM, point_name

HERE = Path(__file__).resolve().parent
REGIONS = 21


def mean_sem(values):
    values = np.asarray(values, dtype=float)
    mean = float(np.mean(values))
    sem = float(np.std(values, ddof=1) / np.sqrt(values.size)) \
        if values.size > 1 else 0.0
    return mean, sem


def read_point(path):
    data = ROOT.RDataFrame("events", str(path))
    count_result = data.Count()
    layer_mean_results = []
    layer_std_results = []
    for region in range(REGIONS):
        column = f"tr_layer_{region:02d}"
        data = data.Define(column, f"tr_energy_keV[{region}]")
        layer_mean_results.append(data.Mean(column))
        layer_std_results.append(data.StdDev(column))
    total_mean_result = data.Mean("tr_total_energy_keV")
    total_std_result = data.StdDev("tr_total_energy_keV")
    photon_mean_result = data.Mean("tr_photon_count")
    photon_std_result = data.StdDev("tr_photon_count")
    events = int(count_result.GetValue())
    layer_mean = [float(result.GetValue()) for result in layer_mean_results]
    layer_sem = [float(result.GetValue()) / np.sqrt(events)
                 for result in layer_std_results]
    total_mean = float(total_mean_result.GetValue())
    total_sem = float(total_std_result.GetValue()) / np.sqrt(events)
    photon_mean = float(photon_mean_result.GetValue())
    photon_sem = float(photon_std_result.GetValue()) / np.sqrt(events)
    root_file = ROOT.TFile.Open(str(path), "READ")
    photon_energy_histogram = root_file.Get("tr_photon_energy")
    mean_photon_energy = float(photon_energy_histogram.GetMean())
    sem_photon_energy = float(photon_energy_histogram.GetMeanError())
    root_file.Close()
    return {
        "events": events,
        "layer_mean": layer_mean,
        "layer_sem": layer_sem,
        "total_mean": total_mean,
        "total_sem": total_sem,
        "photon_mean": photon_mean,
        "photon_sem": photon_sem,
        "photon_energy_mean": mean_photon_energy,
        "photon_energy_sem": sem_photon_energy,
    }


def draw_heatmap(matrix, title, colorbar_label, output, value_format=".2f"):
    figure, axis = plt.subplots(figsize=(14, 13), constrained_layout=True)
    image = axis.imshow(matrix, origin="lower", aspect="auto", cmap="viridis")
    axis.set_xticks(range(len(L1_VALUES)), labels=L1_VALUES)
    axis.set_yticks(range(len(L2_VALUES)), labels=L2_VALUES)
    axis.set_xlabel(r"Foil thickness $l_1$ ($\mu$m)")
    axis.set_ylabel(r"Gap thickness $l_2$ ($\mu$m)")
    axis.set_title(title)
    colorbar = figure.colorbar(image, ax=axis)
    colorbar.set_label(colorbar_label)
    finite_values = matrix[np.isfinite(matrix)]
    if finite_values.size:
        lower = float(np.min(finite_values))
        upper = float(np.max(finite_values))
        midpoint = lower + 0.52 * (upper - lower)
        for row in range(matrix.shape[0]):
            for column in range(matrix.shape[1]):
                value = matrix[row, column]
                if not np.isfinite(value):
                    continue
                text_color = "black" if value >= midpoint else "white"
                axis.text(column, row, format(value, value_format),
                          ha="center", va="center", color=text_color,
                          fontsize=6, fontweight="semibold")
    figure.savefig(output, dpi=180)
    plt.close(figure)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--allow-incomplete", action="store_true",
                        help="Plot available points and leave missing points blank")
    args = parser.parse_args()
    data_directory = HERE / "data"
    plot_directory = HERE / "plots"
    data_directory.mkdir(parents=True, exist_ok=True)
    plot_directory.mkdir(parents=True, exist_ok=True)

    layer_matrices = np.full((REGIONS, len(L2_VALUES), len(L1_VALUES)), np.nan)
    total_matrix = np.full((len(L2_VALUES), len(L1_VALUES)), np.nan)
    photon_matrix = np.full_like(total_matrix, np.nan)
    photon_energy_matrix = np.full_like(total_matrix, np.nan)
    rows = []
    missing = []
    for l1_index, l1 in enumerate(L1_VALUES):
        for l2_index, l2 in enumerate(L2_VALUES):
            root_path = HERE / "root" / point_name(l1, l2) / "trd_electron.root"
            if not root_path.exists():
                missing.append(str(root_path))
                continue
            result = read_point(root_path)
            periods = TARGET_LENGTH_UM // (l1 + l2)
            actual_length_um = periods * (l1 + l2)
            row = {
                "l1_um": l1,
                "l2_um": l2,
                "periods": periods,
                "actual_length_cm": actual_length_um / 10_000,
                "length_error_percent":
                    100 * (TARGET_LENGTH_UM - actual_length_um) / TARGET_LENGTH_UM,
                "events": result["events"],
                "mean_total_tr_energy_keV": result["total_mean"],
                "sem_total_tr_energy_keV": result["total_sem"],
                "mean_generated_tr_photons": result["photon_mean"],
                "sem_generated_tr_photons": result["photon_sem"],
                "mean_generated_tr_photon_energy_keV":
                    result["photon_energy_mean"],
                "sem_generated_tr_photon_energy_keV":
                    result["photon_energy_sem"],
            }
            for region in range(REGIONS):
                row[f"mean_tr_energy_layer_{region + 1:02d}_keV"] = \
                    result["layer_mean"][region]
                row[f"sem_tr_energy_layer_{region + 1:02d}_keV"] = \
                    result["layer_sem"][region]
                layer_matrices[region, l2_index, l1_index] = \
                    result["layer_mean"][region]
            total_matrix[l2_index, l1_index] = result["total_mean"]
            photon_matrix[l2_index, l1_index] = result["photon_mean"]
            photon_energy_matrix[l2_index, l1_index] = \
                result["photon_energy_mean"]
            rows.append(row)

    if missing and not args.allow_incomplete:
        raise SystemExit(f"Missing {len(missing)} ROOT files; run the scan first or "
                         "use --allow-incomplete")
    if not rows:
        raise SystemExit("No ROOT results found")
    incomplete_marker = HERE / "INCOMPLETE_RESULTS.txt"
    if missing:
        incomplete_marker.write_text(
            f"Validation output only: {len(missing)} of 220 scan points are missing.\n",
            encoding="utf-8")
    elif incomplete_marker.exists():
        incomplete_marker.unlink()
    fieldnames = list(rows[0])
    with (data_directory / "scan_results.csv").open("w", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)
    np.savez_compressed(data_directory / "scan_matrices.npz",
        l1_um=np.asarray(L1_VALUES), l2_um=np.asarray(L2_VALUES),
        layer_mean_tr_energy_keV=layer_matrices,
        total_mean_tr_energy_keV=total_matrix,
        mean_generated_tr_photons=photon_matrix,
        mean_generated_tr_photon_energy_keV=photon_energy_matrix)

    for region in range(REGIONS):
        draw_heatmap(layer_matrices[region],
            f"Layer {region + 1:02d}: mean detected TR energy",
            "Mean TR energy deposited (keV/event)",
            plot_directory / f"layer_{region + 1:02d}_mean_tr_energy.png")
    draw_heatmap(total_matrix, "All 21 layers: mean detected TR energy",
        "Mean total TR energy deposited (keV/event)",
        plot_directory / "total_mean_tr_energy.png")
    draw_heatmap(photon_matrix, "Mean generated TR photon count",
        "TR photons/event", plot_directory / "mean_generated_tr_photons.png",
        value_format=".3f")
    draw_heatmap(photon_energy_matrix, "Mean generated TR photon energy",
        "Mean TR photon energy (keV/photon)",
        plot_directory / "mean_generated_tr_photon_energy.png")
    print(f"Wrote {len(rows)} rows and 24 plots; missing points: {len(missing)}")


if __name__ == "__main__":
    main()
