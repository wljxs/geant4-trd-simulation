#ifndef EventAction_h
#define EventAction_h 1

#include "G4UserEventAction.hh"
#include "TRDConstants.hh"

#include <array>
#include <unordered_set>
#include <vector>

// 一个在辐射体中产生的 TR 光子：保存能量及单位方向向量。
struct TRPhoton {
  double energy = 0.0;
  double dx = 0.0;
  double dy = 0.0;
  double dz = 1.0;
};

class RunAction;
class G4Event;

// 事例级“临时记事本”。
// 每个入射粒子对应一个 event；SteppingAction/StackingAction 把数据累加到这里，
// event 结束时再一次性送给 RunAction 写入 ROOT。
class EventAction final : public G4UserEventAction {
 public:
  explicit EventAction(RunAction* runAction);
  void BeginOfEventAction(const G4Event*) override;
  void EndOfEventAction(const G4Event*) override;
  void AddEnergyDeposit(int region, double energy);       // 所有粒子的沉积能
  void AddPrimaryEnergyLoss(int region, double energy);   // 初级粒子的动能损失
  void AddTREnergyDeposit(int region, double energy);     // TR 光子及其后代的沉积能
  void AddTRPhoton(double energy, double dx, double dy, double dz);
  void MarkTRTrack(int trackID);
  bool IsTRTrack(int trackID) const;

 private:
  // 每个数组元素对应一个 GasLayer；单位仍是 Geant4 内部能量单位。
  RunAction* fRunAction = nullptr;
  std::array<double, TRD::kRegions> fEnergyDeposit{};
  std::array<double, TRD::kRegions> fPrimaryEnergyLoss{};
  std::array<double, TRD::kRegions> fTREnergyDeposit{};
  std::vector<TRPhoton> fTRPhotons;
  std::unordered_set<int> fTRTrackIDs;
};

#endif
