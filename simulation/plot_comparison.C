#include <TCanvas.h>
#include <TFile.h>
#include <TF1.h>
#include <TGraphErrors.h>
#include <TH1.h>
#include <TLegend.h>
#include <TStyle.h>
#include <TTree.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Config {
  std::string group;
  std::string name;
  std::string label;
  int color;
};

struct TheoryCurve {
  std::vector<double> energy;
  std::vector<double> yield;

  double Evaluate(double x) const
  {
    if (x <= energy.front()) return yield.front();
    if (x >= energy.back()) return yield.back();
    const auto upper = std::upper_bound(energy.begin(), energy.end(), x);
    const auto index = static_cast<std::size_t>(upper - energy.begin() - 1);
    const double fraction = (x - energy[index]) /
        (energy[index + 1] - energy[index]);
    return yield[index] + fraction * (yield[index + 1] - yield[index]);
  }
};

const std::vector<std::vector<Config>> kGroups = {
  {{"gamma", "gamma_3914", "#gamma=3914", kRed + 1},
   {"gamma", "gamma_1957", "#gamma=1957", kMagenta + 1},
   {"gamma", "gamma_978",  "#gamma=978",  kGreen + 2},
   {"gamma", "gamma_391",  "#gamma=391",  kBlue + 1}},
  {{"foil", "foil_40um", "l_{1}=40 #mum", kRed + 1},
   {"foil", "foil_20um", "l_{1}=20 #mum", kMagenta + 1},
   {"foil", "foil_10um", "l_{1}=10 #mum", kGreen + 2},
   {"foil", "foil_5um",  "l_{1}=5 #mum",  kBlue + 1}},
  {{"gap", "gap_900um", "l_{2}=900 #mum", kRed + 1},
   {"gap", "gap_300um", "l_{2}=300 #mum", kMagenta + 1},
   {"gap", "gap_100um", "l_{2}=100 #mum", kGreen + 2},
   {"gap", "gap_30um",  "l_{2}=30 #mum",  kBlue + 1}},
};

const std::vector<std::string> kTitles = {
  "Momentum scan: l_{1}=15 #mum, l_{2}=300 #mum",
  "Foil scan: l_{2}=300 #mum, #gamma=3914",
  "Gap scan: l_{1}=15 #mum, #gamma=3914",
};

std::map<std::string, TheoryCurve> ReadTheory(const std::string& path)
{
  std::ifstream input(path);
  if (!input) throw std::runtime_error("Cannot open theory CSV: " + path);

  std::map<std::string, TheoryCurve> curves;
  std::string line;
  std::getline(input, line);  // header
  while (std::getline(input, line)) {
    std::stringstream row(line);
    std::string group, name, energy, value;
    std::getline(row, group, ',');
    std::getline(row, name, ',');
    std::getline(row, energy, ',');
    std::getline(row, value, ',');
    auto& curve = curves[group + "/" + name];
    curve.energy.push_back(std::stod(energy));
    curve.yield.push_back(std::stod(value));
  }
  return curves;
}

TGraphErrors* ReadGeant4(const Config& config,
                         const std::string& simulationDirectory,
                         long long& events, double& integral, double& peak)
{
  const std::string path = simulationDirectory + "/root/" + config.group + "/" +
      config.name + "/trd_electron.root";
  TFile file(path.c_str(), "READ");
  if (file.IsZombie()) throw std::runtime_error("Cannot open " + path);

  auto* histogram = file.Get<TH1>("tr_exit_photon_energy");
  auto* tree = file.Get<TTree>("events");
  if (!histogram || !tree) throw std::runtime_error("Missing ROOT objects in " + path);
  events = tree->GetEntries();

  auto* graph = new TGraphErrors;
  graph->SetName(("g_" + config.group + "_" + config.name).c_str());
  integral = 0.0;
  peak = 0.0;
  double largest = -1.0;
  int point = 0;
  for (int bin = 1; bin <= histogram->GetNbinsX(); ++bin) {
    const double x = histogram->GetBinCenter(bin);
    if (x < 1.0 || x > 32.0) continue;
    const double count = histogram->GetBinContent(bin);
    const double width = histogram->GetBinWidth(bin);
    const double y = count / (events * width);
    const double error = std::sqrt(count) / (events * width);
    graph->SetPoint(point, x, y);
    graph->SetPointError(point++, 0.0, error);
    integral += count / events;
    if (y > largest) {
      largest = y;
      peak = x;
    }
  }
  graph->SetMarkerStyle(20);
  graph->SetMarkerSize(0.55);
  graph->SetMarkerColor(config.color);
  graph->SetLineColor(config.color);
  return graph;
}

}  // namespace

void plot_comparison(const char* directory = "simulation",
                     const char* model = "gammaM")
{
  gStyle->SetOptStat(0);
  const std::string simulationDirectory = directory;
  const std::string modelName = model;
  const auto theory = ReadTheory("fig3_reproduction/data/equation10_spectra.csv");
  std::filesystem::create_directories(simulationDirectory + "/plots");
  std::filesystem::create_directories(simulationDirectory + "/data");

  const std::string metricsPath =
      simulationDirectory + "/data/comparison_metrics_root.csv";
  std::ofstream metrics(metricsPath);
  metrics << "group,configuration,events,theory_peak_keV,geant4_peak_keV,"
             "theory_integral_1_32,geant4_integral_1_32,"
             "geant4_to_theory_ratio\n";

  auto* canvas = new TCanvas("comparison",
      ("Fig. 3 comparison (" + modelName + ")").c_str(), 1100, 1350);
  canvas->Divide(1, 3, 0.0, 0.01);

  // ROOT owns the drawn objects until the canvas is saved.
  std::vector<TF1*> functions;
  std::vector<TGraphErrors*> graphs;
  std::vector<TH1D*> frames;
  std::vector<TLegend*> legends;

  for (std::size_t panel = 0; panel < kGroups.size(); ++panel) {
    canvas->cd(panel + 1);
    gPad->SetGrid(1, 1);
    gPad->SetLeftMargin(0.11);
    gPad->SetRightMargin(0.025);

    auto* frame = new TH1D(("frame_" + std::to_string(panel)).c_str(),
        (kTitles[panel] + " (" + modelName + ");TR photon energy (keV);"
         "TR yield (keV^{-1} electron^{-1})").c_str(), 100, 0.5, 32.0);
    frame->SetMinimum(0.0);
    frame->SetMaximum(panel == 2 ? 0.26 : 0.23);
    frame->Draw();
    frames.push_back(frame);

    auto* legend = new TLegend(0.68, 0.72, 0.96, 0.91);
    legend->SetBorderSize(0);
    legend->SetFillStyle(0);
    legend->SetNColumns(2);
    legends.push_back(legend);

    for (const auto& config : kGroups[panel]) {
      const auto key = config.group + "/" + config.name;
      const auto found = theory.find(key);
      if (found == theory.end()) throw std::runtime_error("Missing theory curve: " + key);
      const TheoryCurve* curve = &found->second;

      auto* function = new TF1(("f_" + config.group + "_" + config.name).c_str(),
          [curve](double* x, double*) { return curve->Evaluate(x[0]); },
          1.0, 32.0, 0);
      function->SetNpx(620);
      function->SetLineColor(config.color);
      function->SetLineWidth(3);
      function->Draw("SAME");
      functions.push_back(function);

      long long events = 0;
      double geant4Integral = 0.0;
      double geant4Peak = 0.0;
      auto* graph = ReadGeant4(config, simulationDirectory, events,
                               geant4Integral, geant4Peak);
      graph->Draw("P SAME");
      graphs.push_back(graph);
      legend->AddEntry(function, config.label.c_str(), "l");

      const double theoryIntegral = function->Integral(1.0, 32.0, 1.0e-4);
      metrics << config.group << ',' << config.name << ',' << events << ','
              << function->GetMaximumX(1.0, 32.0) << ',' << geant4Peak << ','
              << theoryIntegral << ',' << geant4Integral << ','
              << geant4Integral / theoryIntegral << '\n';
    }
    legend->Draw();
  }

  const std::string plotPath =
      simulationDirectory + "/plots/fig3_theory_vs_geant4_root.png";
  canvas->SaveAs(plotPath.c_str());
  std::cout << plotPath << '\n' << metricsPath << std::endl;
}
