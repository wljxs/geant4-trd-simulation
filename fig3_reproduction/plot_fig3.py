#!/usr/bin/env python3
"""Calculate Eq. (10), normalize Geant4 output, and make Fig. 3 comparisons."""

from __future__ import annotations

import csv
import math
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
import uproot
import xraylib

HERE = Path(__file__).resolve().parent
ALPHA = 1.0 / 137.035999084
HBARC_KEV_UM = 0.0001973269804
ELECTRON_MASS_GEV = 0.00051099895
PLASMA_FOIL_KEV = 21.0992e-3
PLASMA_AIR_KEV = 0.706676e-3
POLYETHYLENE_DENSITY = 0.94  # g cm^-3, G4_POLYETHYLENE
AIR_DENSITY = 0.00120479  # g cm^-3, close to G4_AIR at STP
N_FOILS = 100

GROUPS = [
    ("gamma", r"Lorentz-factor scan: $l_1=15\,\mu$m, $l_2=300\,\mu$m, $N_f=100$", [
        ("gamma_3914", 2.0, 15.0, 300.0, r"$\gamma=3914$"),
        ("gamma_1957", 1.0, 15.0, 300.0, r"$\gamma=1957$"),
        ("gamma_978", 0.5, 15.0, 300.0, r"$\gamma=978$"),
        ("gamma_391", 0.2, 15.0, 300.0, r"$\gamma=391$"),
    ]),
    ("foil", r"Foil-thickness scan: $l_2=300\,\mu$m, $N_f=100$, $\gamma=3914$", [
        ("foil_40um", 2.0, 40.0, 300.0, r"$l_1=40\,\mu$m"),
        ("foil_20um", 2.0, 20.0, 300.0, r"$l_1=20\,\mu$m"),
        ("foil_10um", 2.0, 10.0, 300.0, r"$l_1=10\,\mu$m"),
        ("foil_5um", 2.0, 5.0, 300.0, r"$l_1=5\,\mu$m"),
    ]),
    ("gap", r"Gap-thickness scan: $l_1=15\,\mu$m, $N_f=100$, $\gamma=3914$", [
        ("gap_900um", 2.0, 15.0, 900.0, r"$l_2=900\,\mu$m"),
        ("gap_300um", 2.0, 15.0, 300.0, r"$l_2=300\,\mu$m"),
        ("gap_100um", 2.0, 15.0, 100.0, r"$l_2=100\,\mu$m"),
        ("gap_30um", 2.0, 15.0, 30.0, r"$l_2=30\,\mu$m"),
    ]),
]
COLORS = ["#e41a1c", "#d627d9", "#22bb44", "#2455d6"]


def gamma_from_momentum(momentum_gev: float) -> float:
    return math.sqrt(1.0 + (momentum_gev / ELECTRON_MASS_GEV) ** 2)


def attenuation_per_period(energy_kev: float, foil_um: float, gap_um: float) -> float:
    """Dimensionless sigma in Eq. (10), including foil and air absorption."""
    foil_mu_per_um = (xraylib.CS_Total_CP("Polyethylene", energy_kev)
                      * POLYETHYLENE_DENSITY / 1.0e4)
    air_mu_per_um = (xraylib.CS_Total_CP("Air, Dry (near sea level)", energy_kev)
                     * AIR_DENSITY / 1.0e4)
    return foil_mu_per_um * foil_um + air_mu_per_um * gap_um


def equation10_yield(energy_kev: float, gamma: float, foil_um: float,
                     gap_um: float, n_terms: int = 20_000) -> float:
    """Photon yield dN/dE from Eqs. (10)-(11), in keV^-1/electron.

    The paper writes dW/domega; division by photon energy converts the
    radiated-energy spectrum to the photon-yield ordinate used in Fig. 3.
    """
    beta = math.sqrt(1.0 - 1.0 / gamma**2)
    kappa = gap_um / foil_um
    phase_scale = energy_kev * foil_um / (2.0 * beta * HBARC_KEV_UM)
    rho1 = phase_scale * (gamma**-2 + (PLASMA_FOIL_KEV / energy_kev) ** 2)
    rho2 = phase_scale * (gamma**-2 + (PLASMA_AIR_KEV / energy_kev) ** 2)
    phase = rho1 + kappa * rho2
    n0 = max(1, math.floor(phase / (2.0 * math.pi)) + 1)
    n = np.arange(n0, n0 + n_terms, dtype=np.float64)
    theta = (2.0 * math.pi * n - phase) / (1.0 + kappa)
    summand = (theta * (1.0 / (rho1 + theta) - 1.0 / (rho2 + theta)) ** 2
               * (1.0 - np.cos(rho1 + theta)))
    sigma = attenuation_per_period(energy_kev, foil_um, gap_um)
    absorption = -math.expm1(-N_FOILS * sigma)
    d_w_d_e = 4.0 * ALPHA * absorption * summand.sum() / (sigma * (kappa + 1.0))
    return d_w_d_e / energy_kev


def read_geant4(group: str, name: str):
    path = HERE / "geant4" / group / name / "trd_electron.root"
    root_file = uproot.open(path)
    histogram = root_file["tr_photon_energy"]
    counts = histogram.values(flow=False).astype(float)
    edges = histogram.axis().edges(flow=False)
    events = root_file["events"].num_entries
    widths = np.diff(edges)
    return (0.5 * (edges[:-1] + edges[1:]), counts / (events * widths),
            np.sqrt(counts) / (events * widths), events)


def calculate_formula_grid():
    energies = np.linspace(1.0, 32.0, 311)
    result = {}
    for group, _, configs in GROUPS:
        for name, momentum, foil, gap, _ in configs:
            gamma = gamma_from_momentum(momentum)
            result[(group, name)] = np.array([
                equation10_yield(e, gamma, foil, gap) for e in energies
            ])
    return energies, result


def style_axis(ax, ylim):
    ax.set_xlim(0.5, 32.0)
    ax.set_ylim(0.0, ylim)
    ax.set_ylabel(r"TR yield (keV$^{-1}$ electron$^{-1}$)")
    ax.grid(alpha=0.16)
    ax.legend(frameon=False, fontsize=8, ncol=2)


def make_three_panel(kind: str, energies, formula, output: Path):
    fig, axes = plt.subplots(3, 1, figsize=(8.2, 10.5), sharex=True)
    for ax, (group, title, configs) in zip(axes, GROUPS):
        panel_max = 0.0
        for color, (name, momentum, foil, gap, label) in zip(COLORS, configs):
            if kind in ("formula", "comparison"):
                suffix = " — Eq. (10)" if kind == "comparison" else ""
                formula_y = formula[(group, name)]
                panel_max = max(panel_max, float(formula_y.max()))
                ax.plot(energies, formula_y, color=color,
                        lw=1.65, label=label + suffix)
            if kind in ("geant4", "comparison"):
                x, y, err, _ = read_geant4(group, name)
                mask = x <= 32.0
                panel_max = max(panel_max, float(y[mask].max()))
                suffix = " — Geant4" if kind == "comparison" else ""
                ax.errorbar(x[mask], y[mask], yerr=err[mask], color=color,
                            fmt="o", ms=2.2, lw=0.7, capsize=1.2,
                            alpha=0.78, label=label + suffix)
        ax.set_title(title, fontsize=10)
        baseline_ylim = 0.25 if group == "gap" else 0.22
        style_axis(ax, max(baseline_ylim, 1.08 * panel_max))
    axes[-1].set_xlabel("TR photon energy (keV)")
    fig.suptitle({"formula": "Paper formula reproduction",
                  "geant4": "Geant4 gammaM reproduction",
                  "comparison": "Equation (10) versus Geant4 gammaM"}[kind],
                 y=0.997)
    fig.tight_layout()
    fig.savefig(output, dpi=220)
    plt.close(fig)


def write_csv(energies, formula):
    data = HERE / "data"
    data.mkdir(exist_ok=True)
    with (data / "equation10_spectra.csv").open("w", newline="") as handle:
        writer = csv.writer(handle)
        writer.writerow(["group", "configuration", "energy_keV",
                         "yield_per_keV_per_electron"])
        for group, _, configs in GROUPS:
            for name, *_ in configs:
                for energy, value in zip(energies, formula[(group, name)]):
                    writer.writerow([group, name, f"{energy:.4f}", f"{value:.9g}"])
    with (data / "geant4_spectra.csv").open("w", newline="") as handle:
        writer = csv.writer(handle)
        writer.writerow(["group", "configuration", "energy_keV",
                         "yield_per_keV_per_electron", "stat_error", "events"])
        for group, _, configs in GROUPS:
            for name, *_ in configs:
                x, y, err, events = read_geant4(group, name)
                for values in zip(x, y, err):
                    writer.writerow([group, name, *(f"{v:.9g}" for v in values), events])

    with (data / "comparison_metrics.csv").open("w", newline="") as handle:
        writer = csv.writer(handle)
        writer.writerow(["group", "configuration", "events", "equation10_peak_keV",
                         "geant4_peak_keV", "equation10_peak_yield", "geant4_peak_yield",
                         "equation10_integral_1_32", "geant4_integral_1_32",
                         "geant4_to_equation_integral_ratio"])
        for group, _, configs in GROUPS:
            for name, *_ in configs:
                eq = formula[(group, name)]
                x, y, _, events = read_geant4(group, name)
                mask = (x >= 1.0) & (x <= 32.0)
                eq_integral = np.trapezoid(eq, energies)
                g4_integral = np.sum(y[mask] * 0.5)
                writer.writerow([
                    group, name, events, f"{energies[np.argmax(eq)]:.4f}",
                    f"{x[mask][np.argmax(y[mask])]:.4f}", f"{eq.max():.9g}",
                    f"{y[mask].max():.9g}", f"{eq_integral:.9g}",
                    f"{g4_integral:.9g}", f"{g4_integral / eq_integral:.6f}"])


def main():
    energies, formula = calculate_formula_grid()
    plots = HERE / "plots"
    plots.mkdir(exist_ok=True)
    make_three_panel("formula", energies, formula, plots / "equation10_fig3.png")
    make_three_panel("geant4", energies, formula, plots / "geant4_fig3.png")
    make_three_panel("comparison", energies, formula, plots / "comparison_fig3.png")
    write_csv(energies, formula)
    for path in sorted(plots.glob("*.png")):
        print(path)


if __name__ == "__main__":
    main()
