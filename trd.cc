#include "DetectorConstruction.hh"
#include "EventAction.hh"
#include "PhysicsList.hh"
#include "PrimaryGeneratorAction.hh"
#include "RunAction.hh"
#include "StackingAction.hh"
#include "SteppingAction.hh"

#include "G4ProductionCutsTable.hh"
#include "G4RunManager.hh"
#include "G4SystemOfUnits.hh"
#include "G4UIExecutive.hh"
#include "G4UImanager.hh"
#include "G4VisExecutive.hh"
#include "G4ios.hh"

#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>

namespace {

void ConfigureProductionCutEnergyRange()
{
  double lowEdge = 12.5 * eV;
  if (const char* value = std::getenv("TRD_CUT_LOW_EDGE_EV"))
    lowEdge = std::stod(value) * eV;

  G4ProductionCutsTable::GetProductionCutsTable()->SetEnergyRange(
      lowEdge, 100.0 * TeV);
}

void PrintUsage()
{
  G4cerr << "Usage:\n"
         << "  ./trd path/to/run.mac   # batch mode\n"
         << "  ./trd --ui              # interactive mode" << G4endl;
}

}  // namespace

int main(int argc, char** argv)
{
  const bool interactive = argc == 2 && std::string(argv[1]) == "--ui";
  const bool macroMode = argc == 2 &&
      std::filesystem::path(argv[1]).extension() == ".mac";
  if (!interactive && !macroMode) {
    PrintUsage();
    return 1;
  }

  // 先创建所有 Geant4 对象。它们的构造函数会注册各自负责的 /trd/... 命令，
  // 因而后面执行宏时，参数可以直接写入真正使用它们的类。
  auto runManager = std::make_unique<G4RunManager>();

  auto* detector = new DetectorConstruction;
  runManager->SetUserInitialization(detector);
  runManager->SetUserInitialization(new PhysicsList(detector));

  auto* generator = new PrimaryGeneratorAction(detector);
  auto* runAction = new RunAction(generator);
  auto* eventAction = new EventAction(runAction);
  runManager->SetUserAction(generator);
  runManager->SetUserAction(runAction);
  runManager->SetUserAction(eventAction);
  runManager->SetUserAction(new StackingAction(eventAction, detector));
  runManager->SetUserAction(new SteppingAction(eventAction, detector));

  // 必须在宏中的 /run/initialize 之前设置能量范围。
  ConfigureProductionCutEnergyRange();
  auto* commands = G4UImanager::GetUIpointer();

  if (macroMode) {
    // 宏现在采用标准 Geant4 顺序，并在文件末尾自行调用：
    //   /run/initialize
    //   /run/beamOn N
    const int status = commands->ApplyCommand(
        G4String("/control/execute ") + argv[1]);
    if (status != 0) {
      G4cerr << "Failed to execute macro " << argv[1]
             << " (status " << status << ")" << G4endl;
      return 1;
    }
  } else {
    auto visManager = std::make_unique<G4VisExecutive>();
    visManager->Initialize();
    int uiArgc = 1;
    auto ui = std::make_unique<G4UIExecutive>(uiArgc, argv);
    G4cout << "Set /trd/... parameters, then run /run/initialize and "
           << "/run/beamOn N." << G4endl;
    ui->SessionStart();
  }

  if (std::getenv("TRD_DUMP_CUTS"))
    G4ProductionCutsTable::GetProductionCutsTable()->DumpCouples();
  return 0;
}
