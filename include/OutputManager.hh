#ifndef OutputManager_h
#define OutputManager_h 1

#include "TRDConstants.hh"
#include "globals.hh"

#include <array>
#include <memory>
#include <vector>
#include "EventAction.hh"

class TFile;
class TH1D;
class TH2D;
class TTree;

// ROOT 输出的唯一管理者。
// 构造时创建文件、TTree 和直方图；每个 event 调一次 Fill；析构时写盘关闭。
class OutputManager {
 public:
  explicit OutputManager(const G4String& fileName);
  ~OutputManager();
  void Fill(const std::array<double, TRD::kRegions>& energy,
            const std::array<double, TRD::kRegions>& primaryEnergyLoss,
            const std::array<double, TRD::kRegions>& trEnergy,
            const std::vector<TRPhoton>& trPhotons,
            const std::vector<TRPhoton>& trExitPhotons);

 private:
  // fFile 拥有 ROOT 文件；其余 ROOT 对象会挂到当前文件目录下，由 ROOT 管理。
  std::unique_ptr<TFile> fFile;
  TTree* fTree = nullptr;
  TH2D* fMap = nullptr;                 // 所有来源的沉积能
  TH2D* fTRMap = nullptr;               // TR 光子及其后代的沉积能
  TH2D* fIonizationMap = nullptr;       // 非 TR（电离背景）沉积能
  TH2D* fPrimaryLossMap = nullptr;
  TH1D* fTRPhotonCountHistogram = nullptr;
  TH1D* fTRPhotonEnergyHistogram = nullptr;
  TH1D* fTRPhotonTotalEnergyHistogram = nullptr;
  TH1D* fTRExitPhotonCountHistogram = nullptr;
  TH1D* fTRExitPhotonEnergyHistogram = nullptr;
  TH1D* fTRExitPhotonTotalEnergyHistogram = nullptr;
  TH1D* fTREnergyDepositionHistogram = nullptr;
  TH1D* fTotalEnergyDepositionHistogram = nullptr;
  std::array<TH1D*, TRD::kRegions> fRegion{};
  std::array<TH1D*, TRD::kRegions> fPrimaryLossRegion{};
  // 下面是 TTree 分支绑定的缓冲区。Fill() 先赋值，再调用 TTree::Fill()。
  std::array<double, TRD::kRegions> fEnergy{};
  std::array<double, TRD::kRegions> fPrimaryEnergyLoss{};
  std::array<double, TRD::kRegions> fTREnergy{};
  std::array<double, TRD::kRegions> fIonizationEnergy{};
  double fTRTotalEnergy = 0.0;
  double fIonizationTotalEnergy = 0.0;
  double fTRPhotonTotalEnergy = 0.0;
  double fTRExitPhotonTotalEnergy = 0.0;
  double fTotalEnergy = 0.0;
  double fPrimaryTotalEnergyLoss = 0.0;
  int fTRPhotonCount = 0;
  int fTRExitPhotonCount = 0;
  std::vector<double> fTRPhotonEnergy;
  std::vector<double> fTRPhotonDirectionX;
  std::vector<double> fTRPhotonDirectionY;
  std::vector<double> fTRPhotonDirectionZ;
  std::vector<double> fTRExitPhotonEnergy;
  std::vector<double> fTRExitPhotonDirectionX;
  std::vector<double> fTRExitPhotonDirectionY;
  std::vector<double> fTRExitPhotonDirectionZ;
};

#endif
