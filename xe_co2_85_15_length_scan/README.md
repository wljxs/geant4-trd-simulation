# Xe/CO2 85:15 radiator-length scan

Configuration:

- primary beam: 5 GeV/c electrons;
- radiator: polyethylene/air, 25 um foil and 100 um air gap;
- requested radiator thicknesses: every integer thickness from 1 through 10 cm;
- detector gas: Xe/CO2 = 85:15 by mole (equivalently volume for ideal gases),
  at 293.15 K and 1 atm;
- gas gap: 4 cm, split into 21 equal readout regions (1.90476 mm each).

All ten lengths contain an integer number of 125 um periods, so requested and
modeled radiator thicknesses are identical. The ROOT tree stores each event's
21-region total energy deposition (`energy_keV`), total summed deposition
(`total_energy_keV`), TR-origin deposition (`tr_energy_keV` and
`tr_total_energy_keV`), and primary energy loss.

From the `geant4simulation` directory:

```bash
cmake --build build -j4
python3 xe_co2_85_15_length_scan/run_scan.py --events 50000 --jobs 4 --force
MPLCONFIGDIR=/tmp/tr_mpl python3 xe_co2_85_15_length_scan/analyze_results.py
```

For a fast end-to-end test, replace `--events 50000` with `--events 10`, then
run the analyzer with `--allow-incomplete` if only a subset was simulated.
Numerical outputs are written to `data/summary.csv` and `data/regions.csv`;
plots are written under `plots/`.

The event-level total energy-deposition spectra are provided in three forms:

- `data/total_energy_spectra.root`: ten ROOT `TH1D` histograms;
- `data/total_energy_spectra.csv`: common 2 keV bins with counts and probabilities;
- `plots/total_energy_spectra.png` and
  `plots/total_energy_spectra_overlay.png`: individual and normalized-overlay
  spectrum figures with linear vertical axes.
- `plots/total_energy_spectra_7to10cm.png` and
  `plots/total_energy_spectra_7to10cm_overlay.png`: focused linear-axis views
  of the four thickest radiators.

## 50,000-event results

The following values are the mean total energy deposited in all 21 gas
regions per incident electron. Uncertainties are standard errors of the mean.

| Radiator thickness (cm) | Periods | Total gas energy deposit (keV/event) | TR-origin part (keV/event) |
|---:|---:|---:|---:|
| 1 | 80  | 41.912 +/- 0.087 | 8.996 +/- 0.051 |
| 2 | 160 | 48.610 +/- 0.101 | 15.280 +/- 0.069 |
| 3 | 240 | 53.653 +/- 0.109 | 20.153 +/- 0.081 |
| 4 | 320 | 58.169 +/- 0.118 | 24.560 +/- 0.091 |
| 5 | 400 | 62.215 +/- 0.125 | 28.240 +/- 0.099 |
| 6 | 480 | 65.851 +/- 0.132 | 31.818 +/- 0.107 |
| 7 | 560 | 69.345 +/- 0.138 | 35.156 +/- 0.114 |
| 8 | 640 | 72.096 +/- 0.143 | 37.715 +/- 0.119 |
| 9 | 720 | 74.852 +/- 0.150 | 40.247 +/- 0.125 |
| 10 | 800 | 77.427 +/- 0.152 | 42.788 +/- 0.129 |

The complete per-region means and uncertainties are in `data/regions.csv`.

TR-origin total energy-deposition spectra are also available as
`data/tr_total_energy_spectra.root`, `data/tr_total_energy_spectra.csv`,
`plots/tr_total_energy_spectra.png`, and
`plots/tr_total_energy_spectra_overlay.png`. Here, "TR-origin" means energy
deposited by transition-radiation photons and all descendants whose tracks are
tagged from those photons.
