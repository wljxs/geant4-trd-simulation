#ifndef RunAction_h
#define RunAction_h 1

#include "G4UserRunAction.hh"
#include "TRDConstants.hh"
#include "globals.hh"

#include <array>
#include <memory>
#include <vector>
#include "EventAction.hh"

class OutputManager;

// Run 级动作。一个 BeamOn() 对应一个 run。
// 本项目把真正的 ROOT 写出委托给 OutputManager。
class RunAction final : public G4UserRunAction {
 public:
  explicit RunAction(const G4String& outputFileName);
  ~RunAction() override;
  // EventAction 在每个 event 结束时调用此函数。
  void FillEvent(const std::array<double, TRD::kRegions>& energy,
                 const std::array<double, TRD::kRegions>& primaryEnergyLoss,
                 const std::array<double, TRD::kRegions>& trEnergy,
                 const std::vector<TRPhoton>& trPhotons);

 private:
  std::unique_ptr<OutputManager> fOutput;
};

#endif
