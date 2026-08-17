#ifndef RunAction_h
#define RunAction_h 1

#include "G4UserRunAction.hh"
#include "TRDConstants.hh"
#include "globals.hh"

#include <array>
#include <memory>
#include <vector>

#include "EventAction.hh"

class G4GenericMessenger;
class G4Run;
class OutputManager;
class PrimaryGeneratorAction;

// Run 级动作：直接提供 /trd/output/directory，并管理 ROOT 文件生命周期。
class RunAction final : public G4UserRunAction {
 public:
  explicit RunAction(const PrimaryGeneratorAction* generator);
  ~RunAction() override;

  void BeginOfRunAction(const G4Run*) override;
  void EndOfRunAction(const G4Run*) override;

  // EventAction 在每个 event 结束时调用此函数。
  void FillEvent(const std::array<double, TRD::kRegions>& energy,
                 const std::array<double, TRD::kRegions>& primaryEnergyLoss,
                 const std::array<double, TRD::kRegions>& trEnergy,
                 const std::vector<TRPhoton>& trPhotons);

 private:
  const PrimaryGeneratorAction* fGenerator = nullptr;  // 不拥有
  G4String fOutputDirectory = "output/electron_radiator";
  std::unique_ptr<G4GenericMessenger> fOutputMessenger;
  std::unique_ptr<OutputManager> fOutput;
};

#endif
