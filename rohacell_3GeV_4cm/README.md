# 3 GeV/c e-/pi- with a 4 cm HF71-equivalent radiator

This reproduces the four normalized total-energy-deposition spectra for
electrons and pions, with and without a radiator, in a 4 cm Xe/CO2 95:5 gas
volume.

ROHACELL HF71 is represented by a regular effective-cell approximation:
15 um of compact PMI (`C8H11NO2`, 1.10 g/cm3) and 200 um of air. Its 6.98%
solid volume fraction gives 76.7 mg/cm3, close to the nominal HF71 density of
75 +/- 15 mg/cm3. This approximation captures the average TR response but not
the stochastic topology of the real closed-cell foam.

Gas ionisation uses the models and energy-loss fluctuations supplied by
Geant4's standard EM physics. No PAI or PAIPhot override is installed.
Radiator runs use the `gammaM` exit-flux model (`G4XTRGammaRadModel`).

```bash
cmake --build build -j4
python3 rohacell_3GeV_4cm/run_simulation.py --events 50000 --jobs 4 --force
MPLCONFIGDIR=/tmp/tr_mpl python3 rohacell_3GeV_4cm/plot_results.py
```

The plotting step writes PNG/PDF figures plus `data/summary.csv` and the
normalized 1 keV-bin contents in `data/spectra.csv`.

`plot_primary_energy_loss.py` separately plots the primary particle's kinetic
energy loss in the detector gas (`primary_total_energy_loss_keV`). Its upper
panel shows the 0--200 keV core and its lower panel shows the rare hard-loss
tail as a log-log survival distribution, so it is not confused with the
actually deposited detector energy plotted by `plot_results.py`.

To run another effective wall/gap pair without overwriting the default result,
provide a tag. For example, the 2.15/80 um test is:

```bash
python3 rohacell_3GeV_4cm/run_simulation.py --events 50000 --jobs 4 \
  --foil-um 2.15 --gap-um 80 --material ROHACELL_HF71 \
  --tag 2p15_80 --force
MPLCONFIGDIR=/tmp/tr_mpl python3 rohacell_3GeV_4cm/plot_results.py \
  --tag 2p15_80 --radiator-label "2.15/80 um"
```

For polypropylene cell walls, use `--material G4_POLYPROPYLENE` and pass
`--radiator-name polypropylene` to the plotting command.

The `2p15_80_pp_gammaM` result uses 2.15 um polypropylene walls and 80 um air
gaps: 486 complete periods in 39.9249 mm. Its 2.617% solid volume fraction
corresponds to an effective polypropylene density of 23.554 mg/cm3. This is a
polypropylene-foam approximation and should not be confused with the compact
PMI chemistry or nominal bulk density of ROHACELL HF71.
