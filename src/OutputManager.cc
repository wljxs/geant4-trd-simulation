#include "OutputManager.hh"

#include <TFile.h>
#include <TH1.h>
#include <TH1D.h>
#include <TH2D.h>
#include <TTree.h>

#include <algorithm>
#include <cmath>

OutputManager::OutputManager(const G4String& fileName)
{
  // 让落在绘图范围外的罕见 delta 电子事例也进入 ROOT 统计量，
  // 从而使直方图 mean 与 TTree 全范围计算结果保持一致。
  TH1::StatOverflows(true);

  // RECREATE：目标文件存在时会重新创建。输出目录已由 trd.cc 预先建立。
  fFile = std::make_unique<TFile>(fileName.c_str(), "RECREATE");

  // ===== TTree：每个 event 一行，适合后续精确、灵活地重新分析 =====
  fTree = new TTree("events", "TRD gas energy depositions");
  // [kRegions] 数组分支保存每一层的数据，单位统一转换为 keV。
  fTree->Branch("energy_keV", fEnergy.data(),
      Form("energy_keV[%d]/D", TRD::kRegions));
  fTree->Branch("primary_energy_loss_keV", fPrimaryEnergyLoss.data(),
      Form("primary_energy_loss_keV[%d]/D", TRD::kRegions));
  fTree->Branch("tr_energy_keV", fTREnergy.data(),
      Form("tr_energy_keV[%d]/D", TRD::kRegions));
  // “电离沉积”定义为总沉积减去已标记的 TR 谱系沉积。
  // 它代表非 TR 背景；通常由入射粒子及其电离次级粒子贡献。
  fTree->Branch("ionization_energy_keV", fIonizationEnergy.data(),
      Form("ionization_energy_keV[%d]/D", TRD::kRegions));
  // 标量分支保存所有气体层之和。
  fTree->Branch("tr_total_energy_keV", &fTRTotalEnergy,
      "tr_total_energy_keV/D");
  fTree->Branch("ionization_total_energy_keV", &fIonizationTotalEnergy,
      "ionization_total_energy_keV/D");
  fTree->Branch("tr_photon_total_energy_keV", &fTRPhotonTotalEnergy,
      "tr_photon_total_energy_keV/D");
  fTree->Branch("total_energy_keV", &fTotalEnergy, "total_energy_keV/D");
  fTree->Branch("primary_total_energy_loss_keV", &fPrimaryTotalEnergyLoss,
      "primary_total_energy_loss_keV/D");
  fTree->Branch("tr_photon_count", &fTRPhotonCount, "tr_photon_count/I");
  // vector 分支长度随每个 event 产生的 TR 光子数变化。
  fTree->Branch("tr_photon_energy_keV", &fTRPhotonEnergy);
  fTree->Branch("tr_photon_dir_x", &fTRPhotonDirectionX);
  fTree->Branch("tr_photon_dir_y", &fTRPhotonDirectionY);
  fTree->Branch("tr_photon_dir_z", &fTRPhotonDirectionZ);
  // ===== 直方图：便于打开 ROOT 文件后立即查看常用分布 =====
  const double regionLengthMm = TRD::kRegionLength / mm;
  // 二维能谱统一显示 0--20 keV，每个 bin 宽 0.1 keV。
  // 这只限制二维图的显示/统计范围，不会截断 TTree 中保存的原始数据。
  constexpr double mapEnergyMaximumKeV = 20.0;
  constexpr double mapEnergyBinWidthKeV = 0.1;
  constexpr int mapEnergyBins =
      static_cast<int>(mapEnergyMaximumKeV / mapEnergyBinWidthKeV);

  // 一维图继续保留原来的较宽范围和 0.05 keV bin，方便观察高能尾部。
  const double requestedEnergyMaximumKeV =
      std::max(100.0, 7.0 * regionLengthMm / 2.0);
  const int energyBins = static_cast<int>(std::ceil(requestedEnergyMaximumKeV / 0.05));
  const double energyMaximumKeV = energyBins * 0.05;
  // 二维图横轴是层号，纵轴是该层每 event 的能量/能损。
  fMap = new TH2D("region_energy",
      Form(";Gas region;Energy deposited in %.g mm (keV)", regionLengthMm),
      TRD::kRegions, 0.5, TRD::kRegions + 0.5,
      mapEnergyBins, 0.0, mapEnergyMaximumKeV);
  fTRMap = new TH2D("region_tr_energy",
      Form(";Gas region;TR energy deposited in %.g mm (keV)", regionLengthMm),
      TRD::kRegions, 0.5, TRD::kRegions + 0.5,
      mapEnergyBins, 0.0, mapEnergyMaximumKeV);
  fIonizationMap = new TH2D("region_ionization_energy",
      Form(";Gas region;Non-TR ionization energy deposited in %.g mm (keV)",
           regionLengthMm),
      TRD::kRegions, 0.5, TRD::kRegions + 0.5,
      mapEnergyBins, 0.0, mapEnergyMaximumKeV);
  fPrimaryLossMap = new TH2D("region_primary_energy_loss",
      Form(";Gas region;Primary energy loss in %.g mm (keV)", regionLengthMm),
      TRD::kRegions, 0.5, TRD::kRegions + 0.5,
      mapEnergyBins, 0.0, mapEnergyMaximumKeV);
  // TR 光子数、产生总能、TR 后代沉积能，以及所有来源的总沉积能。
  fTRPhotonCountHistogram = new TH1D("tr_photon_count",
      ";Number of TR photons per event;Events", 101, -0.5, 100.5);
  fTRPhotonEnergyHistogram = new TH1D("tr_photon_energy",
      ";TR photon energy (keV);Photons", 200, 0.0, 100.0);
  fTRPhotonTotalEnergyHistogram = new TH1D("tr_photon_total_energy",
      ";Total generated TR photon energy per event (keV);Events",
      1000, 0.0, 500.0);
  fTREnergyDepositionHistogram = new TH1D("tr_energy_deposition",
      ";Total energy deposited by TR descendants per event (keV);Events",
      1000, 0.0, 500.0);
  fTotalEnergyDepositionHistogram = new TH1D("total_energy_deposition",
      ";Total energy deposited in gas per event (keV);Events",
      1000, 0.0, 500.0);
  // 每个读出层再各建一张一维图。
  for (int region = 0; region < TRD::kRegions; ++region) {
    fRegion[region] = new TH1D(Form("region_%02d", region + 1),
        Form("Region %02d;Energy deposited (keV);Events", region + 1),
        energyBins, 0.0, energyMaximumKeV);
    fPrimaryLossRegion[region] = new TH1D(
        Form("primary_loss_region_%02d", region + 1),
        Form("Region %02d;Primary energy loss (keV);Events", region + 1),
        energyBins, 0.0, energyMaximumKeV);
  }
}

OutputManager::~OutputManager()
{
  // OutputManager 随 RunAction 析构，因此所有 event 都填完后才写盘。
  fFile->Write();
  fFile->Close();
}

void OutputManager::Fill(const std::array<double, TRD::kRegions>& energy,
                         const std::array<double, TRD::kRegions>& primaryEnergyLoss,
                         const std::array<double, TRD::kRegions>& trEnergy,
                         const std::vector<TRPhoton>& trPhotons)
{
  // ===== 1. 将每层数据从 Geant4 内部单位换算成 keV =====
  fTRTotalEnergy = 0.0;
  fIonizationTotalEnergy = 0.0;
  fTotalEnergy = 0.0;
  fPrimaryTotalEnergyLoss = 0.0;
  for (int region = 0; region < TRD::kRegions; ++region) {
    const double value = energy[region] / keV;
    const double lossValue = primaryEnergyLoss[region] / keV;
    fEnergy[region] = value;
    fPrimaryEnergyLoss[region] = lossValue;
    fTREnergy[region] = trEnergy[region] / keV;
    // 理论上 TR 是 total 的子集；max 防止浮点舍入产生极小负数。
    fIonizationEnergy[region] =
        std::max(0.0, fEnergy[region] - fTREnergy[region]);
    fTRTotalEnergy += fTREnergy[region];
    fIonizationTotalEnergy += fIonizationEnergy[region];
    fTotalEnergy += value;
    fPrimaryTotalEnergyLoss += lossValue;
    fMap->Fill(region + 1, value);
    fTRMap->Fill(region + 1, fTREnergy[region]);
    fIonizationMap->Fill(region + 1, fIonizationEnergy[region]);
    fPrimaryLossMap->Fill(region + 1, lossValue);
    fRegion[region]->Fill(value);
    fPrimaryLossRegion[region]->Fill(lossValue);
  }
  // ===== 2. 整理本 event 在辐射体中“生成时”的 TR 光子 =====
  fTRPhotonCount = static_cast<int>(trPhotons.size());
  fTRPhotonTotalEnergy = 0.0;
  fTRPhotonEnergy.clear();
  fTRPhotonDirectionX.clear();
  fTRPhotonDirectionY.clear();
  fTRPhotonDirectionZ.clear();
  // reserve 只预分配容量，不改变 vector 长度，可减少反复扩容。
  fTRPhotonEnergy.reserve(trPhotons.size());
  fTRPhotonDirectionX.reserve(trPhotons.size());
  fTRPhotonDirectionY.reserve(trPhotons.size());
  fTRPhotonDirectionZ.reserve(trPhotons.size());
  for (const auto& photon : trPhotons) {
    const double value = photon.energy / keV;
    fTRPhotonEnergy.push_back(value);
    fTRPhotonTotalEnergy += value;
    fTRPhotonDirectionX.push_back(photon.dx);
    fTRPhotonDirectionY.push_back(photon.dy);
    fTRPhotonDirectionZ.push_back(photon.dz);
    fTRPhotonEnergyHistogram->Fill(value);
  }
  // ===== 3. 每个 event 给汇总直方图和 TTree 各填一次 =====
  fTRPhotonCountHistogram->Fill(fTRPhotonCount);
  fTRPhotonTotalEnergyHistogram->Fill(fTRPhotonTotalEnergy);
  fTREnergyDepositionHistogram->Fill(fTRTotalEnergy);
  fTotalEnergyDepositionHistogram->Fill(fTotalEnergy);
  fTree->Fill();
}
