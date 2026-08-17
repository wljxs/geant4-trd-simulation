#include "RunAction.hh"
#include "OutputManager.hh"

RunAction::RunAction(const G4String& outputFileName)
    // RunAction 与整个 run 同寿命，因此让它持有输出文件最合适。
    : fOutput(std::make_unique<OutputManager>(outputFileName)) {}

RunAction::~RunAction() = default;

void RunAction::FillEvent(const std::array<double, TRD::kRegions>& energy,
                          const std::array<double, TRD::kRegions>& primaryEnergyLoss,
                          const std::array<double, TRD::kRegions>& trEnergy,
                          const std::vector<TRPhoton>& trPhotons)
{
  // 保持 RunAction 很薄：数据格式、单位换算和 ROOT API 都集中在 OutputManager。
  fOutput->Fill(energy, primaryEnergyLoss, trEnergy, trPhotons);
}
