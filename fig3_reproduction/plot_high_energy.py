#!/usr/bin/env python3
"""Compare Eq. (10) and Geant4 above the energy range plotted in Fig. 3."""

import csv
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
import uproot

from plot_fig3 import equation10_yield, gamma_from_momentum

HERE = Path(__file__).resolve().parent
MOMENTA = [2, 5, 10, 20, 50, 100]
COLORS = plt.cm.viridis(np.linspace(0.08, 0.92, len(MOMENTA)))


def root_path(momentum):
    if momentum == 2:
        return HERE / "geant4/gamma/gamma_3914/trd_electron.root"
    return HERE / f"high_energy/p_{momentum}gev/trd_electron.root"


def read_spectrum(momentum):
    root = uproot.open(root_path(momentum))
    hist = root["tr_photon_energy"]
    counts = hist.values().astype(float)
    edges = hist.axis().edges()
    events = root["events"].num_entries
    widths = np.diff(edges)
    return (0.5 * (edges[:-1] + edges[1:]), counts / events / widths,
            np.sqrt(counts) / events / widths, events)


def main():
    energies = np.linspace(1, 32, 311)
    rows = []
    spectra = {}
    for momentum in MOMENTA:
        gamma = gamma_from_momentum(momentum)
        equation = np.array([equation10_yield(e, gamma, 15, 300)
                             for e in energies])
        x, geant4, error, events = read_spectrum(momentum)
        mask = (x >= 1) & (x <= 32)
        eq_integral = np.trapezoid(equation, energies)
        g4_integral = np.sum(geant4[mask] * 0.5)
        spectra[momentum] = (equation, x, geant4, error)
        rows.append((momentum, gamma, events, energies[np.argmax(equation)],
                     x[mask][np.argmax(geant4[mask])], eq_integral, g4_integral,
                     g4_integral / eq_integral))

    data = HERE / "data/high_energy_metrics.csv"
    with data.open("w", newline="") as handle:
        writer = csv.writer(handle)
        writer.writerow(["momentum_GeV_c", "gamma", "events", "equation_peak_keV",
                         "geant4_peak_keV", "equation_integral_1_32",
                         "geant4_integral_1_32", "geant4_to_equation_ratio"])
        writer.writerows(rows)

    fig, (ax_spectrum, ax_sat) = plt.subplots(1, 2, figsize=(13, 5.2))
    for color, momentum in zip(COLORS, MOMENTA):
        equation, x, geant4, error = spectra[momentum]
        mask = x <= 32
        label = f"{momentum} GeV/c"
        ax_spectrum.plot(energies, equation, color=color, lw=1.6, label=label)
        ax_spectrum.errorbar(x[mask], geant4[mask], yerr=error[mask], color=color,
                             fmt="o", ms=2, lw=0.6, alpha=0.7)
    ax_spectrum.set(xlim=(1, 32), ylim=(0, 0.26), xlabel="TR photon energy (keV)",
                    ylabel=r"TR yield (keV$^{-1}$ electron$^{-1}$)",
                    title=r"High-energy spectra ($l_1=15\,\mu$m, $l_2=300\,\mu$m, $N_f=100$)")
    ax_spectrum.legend(frameon=False, fontsize=8, ncol=2,
                       title="lines: Eq. (10); points: Geant4")
    momenta = np.array([r[0] for r in rows])
    eq_int = np.array([r[5] for r in rows])
    g4_int = np.array([r[6] for r in rows])
    ax_sat.semilogx(momenta, eq_int, "-o", label="Eq. (10)")
    ax_sat.semilogx(momenta, g4_int, "--s", label="Geant4")
    ax_sat.set(xlabel="Electron momentum (GeV/c)",
               ylabel="TR photons/electron (1–32 keV)",
               title="Saturation with incident energy")
    ax_sat.legend(frameon=False)
    for ax in (ax_spectrum, ax_sat):
        ax.grid(alpha=0.2)
    fig.tight_layout()
    output = HERE / "plots/high_energy_prediction.png"
    fig.savefig(output, dpi=220)
    print(output)
    print(data)


if __name__ == "__main__":
    main()
