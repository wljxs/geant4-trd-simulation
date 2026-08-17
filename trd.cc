#include "DetectorConstruction.hh"
#include "EventAction.hh"
#include "PhysicsList.hh"
#include "PrimaryGeneratorAction.hh"
#include "RunAction.hh"
#include "SimulationConfig.hh"
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

// ROOT 文件名使用易读的英文粒子名；Geant4 内部仍使用 e-、pi-、mu-。
std::string ParticleLabel(const G4String& particle)
{
  if (particle == "e-") return "electron";
  if (particle == "pi-") return "pion";
  return "muon";
}

// Geant4 默认的低能端可能高于本模拟关心的 X 射线能区，所以在初始化前
// 把产生截断表的能量范围下限设为 12.5 eV。环境变量可覆盖这个数值。
void ConfigureProductionCutEnergyRange()
{
  double lowEdge = 12.5 * eV;
  if (const char* value = std::getenv("TRD_CUT_LOW_EDGE_EV"))
    lowEdge = std::stod(value) * eV;

  G4ProductionCutsTable::GetProductionCutsTable()->SetEnergyRange(
      lowEdge, 100.0 * TeV);
}

// 建立一次完整模拟。理解 Geant4 程序时，可以把这一段记成四步：
//   1. RunManager 管理模拟；2. 注册几何和物理；
//   3. 注册粒子源及数据记录动作；4. Initialize 后开始运行。
int RunSimulation(const SimulationConfig& config,
                  const std::filesystem::path& outputFile,
                  bool interactive,
                  char** argv)
{
  std::filesystem::create_directories(outputFile.parent_path());

  // unique_ptr 只负责最终删除 RunManager。注册给 RunManager 的对象由它接管。
  auto runManager = std::make_unique<G4RunManager>();

  // UserInitialization：描述“实验装置是什么”和“允许哪些物理过程”。
  auto* detector = new DetectorConstruction(
      config.useRadiator, config.foilThickness, config.gapThickness,
      config.totalLength, config.foilMaterial, config.detectorGas);
  runManager->SetUserInitialization(detector);
  runManager->SetUserInitialization(new PhysicsList(detector));

  // UserAction：描述“粒子怎样入射”和“每个 run/event/step 记录什么”。
  const double startZ = config.useRadiator
      ? -detector->GetRadiatorLength() - 5.0 * mm
      : -5.0 * mm;
  auto* runAction = new RunAction(outputFile.string());
  auto* eventAction = new EventAction(runAction);
  runManager->SetUserAction(new PrimaryGeneratorAction(
      config.particle, startZ, config.momentum));
  runManager->SetUserAction(runAction);
  runManager->SetUserAction(eventAction);
  runManager->SetUserAction(new StackingAction(eventAction));
  runManager->SetUserAction(new SteppingAction(eventAction));

  // 截断范围必须在 Initialize() 之前设置。
  ConfigureProductionCutEnergyRange();
  runManager->Initialize();

  if (!interactive) {
    // .mac 中的 /trd/run/events 最终在这里变成真正的事件循环。
    runManager->BeamOn(config.events);
  } else {
    // 可视化模式进入 Geant4 命令行；可在窗口里输入 /run/beamOn 10。
    auto visManager = std::make_unique<G4VisExecutive>();
    visManager->Initialize();
    int uiArgc = 1;
    auto ui = std::make_unique<G4UIExecutive>(uiArgc, argv);
    auto* commands = G4UImanager::GetUIpointer();
    commands->ApplyCommand("/vis/open TSG_QT_ZB");
    commands->ApplyCommand("/vis/drawVolume");
    commands->ApplyCommand("/vis/viewer/set/autoRefresh true");
    commands->ApplyCommand("/vis/scene/add/trajectories smooth");
    commands->ApplyCommand("/vis/scene/endOfEventAction accumulate 10");
    ui->SessionStart();
  }

  if (std::getenv("TRD_DUMP_CUTS"))
    G4ProductionCutsTable::GetProductionCutsTable()->DumpCouples();
  return 0;
}

// 推荐入口：先执行宏，让 /trd/... 命令写入 config；再据此创建几何。
int RunMacro(const std::filesystem::path& macroFile, char** argv)
{
  SimulationConfig config;  // 构造时注册所有 /trd/... 命令
  config.LoadEnvironment(); // 环境变量是可选覆盖项，通常不需要使用

  const int status = G4UImanager::GetUIpointer()->ApplyCommand(
      G4String("/control/execute ") + macroFile.string());
  if (status != 0) {
    G4cerr << "Failed to execute macro " << macroFile
           << " (status " << status << ")" << G4endl;
    return 1;
  }
  config.Validate();

  const auto outputFile = std::filesystem::path(config.outputDirectory.c_str()) /
      ("trd_" + ParticleLabel(config.particle) + ".root");
  return RunSimulation(config, outputFile, false, argv);
}

void PrintUsage()
{
  G4cerr << "Recommended (.mac):\n"
         << "  ./trd ../macros/example.mac\n\n"
         << "Other supported modes:\n"
         << "  ./trd [e-|pi-|mu-] [number_of_events] [no-radiator]\n"
         << "  ./trd --ui [e-|pi-|mu-] [no-radiator]" << G4endl;
}

}  // namespace

int main(int argc, char** argv)
{
  // 只给一个 .mac 文件时，走最适合批量模拟的宏入口。
  if (argc == 2 && std::filesystem::path(argv[1]).extension() == ".mac")
    return RunMacro(argv[1], argv);

  // 以下保留原来的命令行/可视化兼容入口；新手可以暂时只看上面的宏入口。
  const bool interactive = argc > 1 && std::string(argv[1]) == "--ui";
  const int particleIndex = interactive ? 2 : 1;
  const G4String particle = argc > particleIndex ? argv[particleIndex] : "e-";
  const int events = !interactive && argc > particleIndex + 1
      ? std::atoi(argv[particleIndex + 1]) : 50000;
  const int radiatorIndex = particleIndex + (interactive ? 1 : 2);
  const bool useRadiator = !(argc > radiatorIndex &&
      std::string(argv[radiatorIndex]) == "no-radiator");

  const bool badParticle = particle != "e-" && particle != "pi-" &&
      particle != "mu-";
  const bool badRadiatorOption = argc > radiatorIndex && useRadiator;
  const bool tooManyArguments = argc > radiatorIndex + 1;
  if (badParticle || events <= 0 || badRadiatorOption || tooManyArguments) {
    PrintUsage();
    return 1;
  }

  SimulationConfig config;
  config.LoadEnvironment();
  config.particle = particle;
  config.events = events;
  config.useRadiator = useRadiator;

  const std::string label = ParticleLabel(particle);
  if (!std::getenv("TRD_OUTPUT_DIR")) {
    config.outputDirectory = "output/" + label +
        (useRadiator ? "_radiator" : "_no_radiator");
  }
  config.Validate();

  const auto outputFile = std::filesystem::path(config.outputDirectory.c_str()) /
      ("trd_" + label + (useRadiator ? ".root" : "_no_radiator.root"));
  return RunSimulation(config, outputFile, interactive, argv);
}
