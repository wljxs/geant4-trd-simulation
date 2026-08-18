# Reproduction of Andronic & Wessels Fig. 3

This directory contains three independently inspectable artifacts:

- `paper/paper_fig3.png`: Fig. 3 rendered directly from the supplied PDF.
- `paper/equations_10_11.png`: Eqs. (10)-(11) rendered directly from the PDF.
- `plots/equation10_fig3.png`: a numerical implementation of Eqs. (10)-(11).
- `plots/geant4_fig3.png`: twelve normalized Geant4 spectra.
- `plots/comparison_fig3.png`: Eq. (10) curves and Geant4 points overlaid.

The simulated radiator uses 100 polyethylene (`G4_POLYETHYLENE`) foils in
air. The three panels scan exactly the paper's four momenta/Lorentz factors,
four foil thicknesses, and four gap thicknesses. Each Geant4 configuration
contains 10,000 incident electrons. Transition radiation is generated with
Geant4's `gammaM` mode (`G4XTRGammaRadModel`), which represents the flux after
a Gamma-distributed radiator; each macro sets `/trd/radiator/model gammaM`
explicitly. The ROOT histogram is normalized as

`yield = bin counts / (incident electrons * bin width in keV)`.

`/trd/mode/trOnly true` disables the downstream gas detector and kills TR photons
after they cross the virtual radiator-exit scoring plane and have been recorded.
The plane is configured with `/trd/scoring/exitDistance 0 um`. This is appropriate here because
the selected XTR process generates the exit flux and Fig. 3 explicitly shows
the spectrum at the radiator exit. It also avoids spending time on a detector
response that is not part of Fig. 3.

## Re-run

From the `geant4simulation` directory:

```bash
cmake --build build -j4
PYTHONPATH=/tmp/tr_formula_libs python3 fig3_reproduction/run_fig3_scan.py --events 10000 --force
PYTHONPATH=/tmp/tr_formula_libs MPLCONFIGDIR=/tmp/tr_mpl python3 fig3_reproduction/plot_fig3.py
PYTHONPATH=/tmp/tr_pdf_render python3 fig3_reproduction/prepare_paper_images.py
```

The plotting environment needs NumPy, Matplotlib, Uproot and xraylib; PDF
rendering needs PyMuPDF. Equation (10) uses the polyethylene and dry-air
energy-dependent attenuation coefficients supplied by xraylib. Raw spectra
and statistical errors are exported under `data/`.

## Interpretation

The current plots and metrics compare the `gammaM` exit-flux scan with Eq.
(10). The integrated Geant4/formula ratios over 1--32 keV range from 0.930 to
1.030 across all twelve configurations; the baseline gamma=3914 ratio is
1.006. The corresponding peak energies are 6.25 keV in the 0.5-keV-wide
Geant4 histogram and 5.90 keV on the formula grid. See
`data/comparison_metrics.csv` for the machine-readable comparison. The
previous `gammaR` results are retained as mode-suffixed plots and metrics and
under `geant4_gammaR_20260814/`.

## Higher incident energies

The existing `plots/high_energy_prediction.png` predates the `gammaM` Fig. 3
rerun and uses the former transparent-regular model. It extends that baseline
radiator comparison to 2, 5, 10, 20, 50 and 100 GeV/c. The integrated
Geant4/Eq. (10) ratios are
1.011, 1.019, 1.014, 1.031, 1.015 and 1.018, respectively. The predicted yield
is already nearly saturated above 5-10 GeV/c. Note that the installed Geant4
XTR base class tabulates equivalent proton kinetic energies only through
100 TeV, corresponding to an electron gamma of order 1e5 (about 50 GeV/c).
Thus 50 GeV/c is the last tested point cleanly inside that declared table
range; the 100 GeV/c result is a useful saturation cross-check but should not
be treated as an independent validation of the Geant4 extrapolation.
