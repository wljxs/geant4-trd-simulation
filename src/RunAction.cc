#include "RunAction.hh"
#include "OutputManager.hh"
#include "PrimaryGeneratorAction.hh"

#include "G4GenericMessenger.hh"
#include "G4Run.hh"
#include "G4StateManager.hh"

#include <cstdlib>
#include <filesystem>
#include <string>

namespace {

std::string ParticleLabel(const G4String& particle)
{
  if (particle == "e-") return "electron";
  if (particle == "pi-") return "pion";
  if (particle == "mu-") return "muon";
  G4Exception("RunAction", "TRD301", FatalException,
      "Particle must be e-, pi-, or mu-.");
  return "unknown";
}

}  // namespace

RunAction::RunAction(const PrimaryGeneratorAction* generator)
    : fGenerator(generator)
{
  // 兼容旧脚本；.mac 中的同名命令在构造以后执行，因此优先级更高。
  if (const char* value = std::getenv("TRD_OUTPUT_DIR"))
    fOutputDirectory = value;

  fOutputMessenger = std::make_unique<G4GenericMessenger>(
      this, "/trd/output/", "ROOT output configuration");
  auto& directory = fOutputMessenger->DeclareProperty(
      "directory", fOutputDirectory,
      "Directory for trd_<particle>.root");
  directory.SetStates(G4State_PreInit);
}

RunAction::~RunAction() = default;

void RunAction::BeginOfRunAction(const G4Run*)
{
  if (fOutputDirectory.empty()) {
    G4Exception("RunAction", "TRD302", FatalException,
        "Output directory must not be empty.");
  }

  const std::filesystem::path directory(fOutputDirectory.c_str());
  std::filesystem::create_directories(directory);
  const auto file = directory /
      ("trd_" + ParticleLabel(fGenerator->GetParticleName()) + ".root");

  // 延迟到 run 开始才打开文件，因此宏有机会先设置输出目录和粒子类型。
  fOutput = std::make_unique<OutputManager>(file.string());
  G4cout << "ROOT output: " << file << G4endl;
}

void RunAction::EndOfRunAction(const G4Run*)
{
  // reset 触发 OutputManager 析构，将所有 TTree/直方图写入并关闭文件。
  fOutput.reset();
}

void RunAction::FillEvent(
    const std::array<double, TRD::kRegions>& energy,
    const std::array<double, TRD::kRegions>& primaryEnergyLoss,
    const std::array<double, TRD::kRegions>& trEnergy,
    const std::vector<TRPhoton>& trPhotons)
{
  if (!fOutput) {
    G4Exception("RunAction", "TRD303", FatalException,
        "ROOT output is not open; call /run/beamOn inside a valid run.");
  }
  fOutput->Fill(energy, primaryEnergyLoss, trEnergy, trPhotons);
}
