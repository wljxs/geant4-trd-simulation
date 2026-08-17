# 5 GeV/c electron, 4 cm radiator scan

This scan uses 10 foil thicknesses and 22 air-gap thicknesses (220 points).
Only complete polyethylene/air periods are constructed, so the actual length
is the largest whole-period length not exceeding 4 cm.  Every ROOT event stores
the TR-origin energy deposited in each of the 21 gas layers, its 21-layer sum,
and the number and energies of generated TR photons.

Build and generate all macros:

```bash
cmake --build build -j4
python3 scan_5GeV_4cm/run_scan.py --generate-only
```

Run the full 50,000-event scan and make the 24 figures:

```bash
MPLCONFIGDIR=/tmp/tr_mpl python3 scan_5GeV_4cm/run_scan.py --events 50000 --jobs 4 --force
MPLCONFIGDIR=/tmp/tr_mpl python3 scan_5GeV_4cm/plot_results.py
```

For a fast end-to-end check, use `--events 10 --limit 1 --force`, followed by
`plot_results.py --allow-incomplete`. The figures comprise 21 layer-energy
maps, total detected TR energy, generated photon count, and mean generated
photon energy.
