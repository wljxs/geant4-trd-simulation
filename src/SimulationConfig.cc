#include "SimulationConfig.hh"
#include "TRDConstants.hh"

#include "G4GenericMessenger.hh"
#include "G4StateManager.hh"
#include "G4SystemOfUnits.hh"

#include <cstdlib>
#include <string>

SimulationConfig::SimulationConfig()
    // 先设置默认物理量。这里必须乘 Geant4 单位，不能只保存裸数字。
    : momentum(3.0 * GeV),
      foilThickness(TRD::kFoilThickness),
      gapThickness(TRD::kRadiatorGap),
      totalLength(TRD::kRadiatorLayers *
                  (TRD::kFoilThickness + TRD::kRadiatorGap))
{
  // GenericMessenger 把 C++ 成员变量直接暴露成宏命令。例如：
  // /trd/beam/momentum 3 GeV 会把带单位的数值写入 momentum。
  fBeamMessenger = std::make_unique<G4GenericMessenger>(this, "/trd/beam/",
      "Primary beam configuration");
  fRadiatorMessenger = std::make_unique<G4GenericMessenger>(
      this, "/trd/radiator/", "TR radiator configuration");
  fDetectorMessenger = std::make_unique<G4GenericMessenger>(
      this, "/trd/detector/", "TRD gas detector configuration");
  fRunMessenger = std::make_unique<G4GenericMessenger>(this, "/trd/run/",
      "TRD run configuration");
  fOutputMessenger = std::make_unique<G4GenericMessenger>(
      this, "/trd/output/", "TRD output configuration");

  // ===== 束流命令 =====
  auto& particleCommand = fBeamMessenger->DeclareProperty("particle", particle,
      "Primary particle: e-, pi-, or mu-");
  particleCommand.SetStates(G4State_PreInit);
  auto& momentumCommand = fBeamMessenger->DeclarePropertyWithUnit(
      "momentum", "GeV", momentum, "Primary momentum");
  momentumCommand.SetStates(G4State_PreInit);
  // ===== 辐射体命令 =====
  auto& materialCommand = fRadiatorMessenger->DeclareProperty(
      "material", foilMaterial, "Geant4 foil material name");
  materialCommand.SetStates(G4State_PreInit);
  auto& enabledCommand = fRadiatorMessenger->DeclareProperty(
      "enabled", useRadiator, "Enable transition-radiation radiator");
  enabledCommand.SetStates(G4State_PreInit);
  auto& foilCommand = fRadiatorMessenger->DeclarePropertyWithUnit(
      "foilThickness", "um", foilThickness, "Foil thickness");
  foilCommand.SetStates(G4State_PreInit);
  auto& gapCommand = fRadiatorMessenger->DeclarePropertyWithUnit(
      "gapThickness", "um", gapThickness, "Air-gap thickness");
  gapCommand.SetStates(G4State_PreInit);
  auto& lengthCommand = fRadiatorMessenger->DeclarePropertyWithUnit(
      "totalLength", "cm", totalLength,
      "Target radiator length; only complete periods are built");
  lengthCommand.SetStates(G4State_PreInit);
  // ===== 探测器、事例数和输出命令 =====
  auto& gasCommand = fDetectorMessenger->DeclareProperty(
      "gas", detectorGas,
      "Detector gas: XeNeIsobutane, XeCO2_85_15, or XeCO2_95_5 "
      "(mole/volume fractions)");
  gasCommand.SetStates(G4State_PreInit);
  auto& eventCommand = fRunMessenger->DeclareProperty(
      "events", events, "Number of incident particles");
  eventCommand.SetStates(G4State_PreInit);
  auto& outputCommand = fOutputMessenger->DeclareProperty(
      "directory", outputDirectory, "Directory for trd_<particle>.root");
  // 所有命令仅允许在 PreInit 状态使用，因为它们会影响初始化时建立的几何/
  // 物理过程。因此本项目先执行宏，再调用 RunManager::Initialize()。
  outputCommand.SetStates(G4State_PreInit);
}

SimulationConfig::~SimulationConfig() = default;

void SimulationConfig::LoadEnvironment()
{
  // 兼容项目原有扫描脚本。宏在此函数之后执行，所以同一参数以宏值为准。
  if (const char* value = std::getenv("TRD_FOIL_THICKNESS_UM"))
    foilThickness = std::stod(value) * um;
  if (const char* value = std::getenv("TRD_RADIATOR_GAP_UM"))
    gapThickness = std::stod(value) * um;
  if (const char* value = std::getenv("TRD_RADIATOR_LAYERS"))
    totalLength = std::stoi(value) * (foilThickness + gapThickness);
  if (const char* value = std::getenv("TRD_FOIL_MATERIAL"))
    foilMaterial = value;
  if (const char* value = std::getenv("TRD_DETECTOR_GAS"))
    detectorGas = value;
  if (const char* value = std::getenv("TRD_MOMENTUM_GEV"))
    momentum = std::stod(value) * GeV;
  if (const char* value = std::getenv("TRD_OUTPUT_DIR"))
    outputDirectory = value;
}

void SimulationConfig::Validate() const
{
  // 尽早失败：不要等到 Geant4 深层初始化时才暴露拼写或负数参数。
  if (particle != "e-" && particle != "pi-" && particle != "mu-")
    G4Exception("SimulationConfig", "TRD101", FatalException,
        "beam/particle must be e-, pi-, or mu-.");
  if (detectorGas != "XeNeIsobutane" && detectorGas != "XeCO2_85_15" &&
      detectorGas != "XeCO2_95_5")
    G4Exception("SimulationConfig", "TRD103", FatalException,
        "detector/gas must be XeNeIsobutane, XeCO2_85_15, or XeCO2_95_5.");
  if (momentum <= 0.0 || foilThickness <= 0.0 || gapThickness <= 0.0 ||
      totalLength < foilThickness + gapThickness || events <= 0 ||
      outputDirectory.empty())
    G4Exception("SimulationConfig", "TRD102", FatalException,
        "Momentum, dimensions, events, and output directory must be positive; "
        "the total length must contain at least one complete period.");
}
