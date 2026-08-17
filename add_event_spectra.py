#!/usr/bin/env python3
"""Add per-event TR and gas-deposition spectra to existing simulation ROOT files."""

import argparse
from pathlib import Path

import ROOT


HISTOGRAMS = (
    (
        "tr_photon_total_energy",
        "Total generated TR photon energy per event (keV)",
        lambda event: sum(event.tr_photon_energy_keV),
    ),
    (
        "tr_energy_deposition",
        "Total energy deposited by TR descendants per event (keV)",
        lambda event: event.tr_total_energy_keV,
    ),
    (
        "total_energy_deposition",
        "Total energy deposited in gas per event (keV)",
        lambda event: event.total_energy_keV,
    ),
)


def add_spectra(path: Path) -> None:
    root_file = ROOT.TFile.Open(str(path), "UPDATE")
    if not root_file or root_file.IsZombie():
        raise RuntimeError(f"Cannot open ROOT file: {path}")

    tree = root_file.Get("events")
    if not tree:
        root_file.Close()
        raise RuntimeError(f"Missing events tree: {path}")

    histograms = []
    for name, axis_title, value_getter in HISTOGRAMS:
        histogram = ROOT.TH1D(name, f";{axis_title};Events", 1000, 0.0, 500.0)
        histogram.SetDirectory(0)
        histograms.append((histogram, value_getter))

    for event in tree:
        for histogram, value_getter in histograms:
            histogram.Fill(value_getter(event))

    root_file.cd()
    for histogram, _ in histograms:
        histogram.Write(histogram.GetName(), ROOT.TObject.kOverwrite)
    root_file.Close()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root_files", nargs="+", type=Path)
    args = parser.parse_args()

    ROOT.gROOT.SetBatch(True)
    ROOT.TH1.StatOverflows(True)
    for path in args.root_files:
        add_spectra(path)
        print(f"updated {path}")


if __name__ == "__main__":
    main()
