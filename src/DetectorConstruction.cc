#include "DetectorConstruction.hh"
#include "TRDConstants.hh"

#include "G4Box.hh"
#include "G4GenericMessenger.hh"
#include "G4LogicalVolume.hh"
#include "G4Material.hh"
#include "G4IonisParamMat.hh"
#include "G4NistManager.hh"
#include "G4PVPlacement.hh"
#include "G4ProductionCuts.hh"
#include "G4Region.hh"
#include "G4StateManager.hh"
#include "G4ios.hh"

#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <string>

// 默认值来自 TRDConstants.hh；随后 .mac 命令可在 /run/initialize 前直接修改。
DetectorConstruction::DetectorConstruction()
    : fFoilThickness(TRD::kFoilThickness),
      fRadiatorGap(TRD::kRadiatorGap),
      fTargetLength(TRD::kRadiatorLayers *
                    (TRD::kFoilThickness + TRD::kRadiatorGap)),
      fRadiatorLayers(TRD::kRadiatorLayers)//初始化
{
  fTrOnly = std::getenv("TRD_TR_ONLY") != nullptr;
  if (const char* value = std::getenv("TRD_FOIL_THICKNESS_UM"))//看环境变量
    fFoilThickness = std::stod(value) * um;
  if (const char* value = std::getenv("TRD_RADIATOR_GAP_UM"))
    fRadiatorGap = std::stod(value) * um;
  if (const char* value = std::getenv("TRD_RADIATOR_LAYERS"))
    fTargetLength = std::stoi(value) * (fFoilThickness + fRadiatorGap);
  if (const char* value = std::getenv("TRD_FOIL_MATERIAL"))
    fFoilMaterialName = value;
  if (const char* value = std::getenv("TRD_DETECTOR_GAS"))
    fDetectorGasName = value;

  fRadiatorMessenger = std::make_unique<G4GenericMessenger>(
      this, "/trd/radiator/", "TR radiator configuration");//创建一个新的 Geant4命令接口
  fDetectorMessenger = std::make_unique<G4GenericMessenger>(
      this, "/trd/detector/", "TRD gas detector configuration");

  auto& enabled = fRadiatorMessenger->DeclareProperty(
      "enabled", fUseRadiator, "Enable transition-radiation radiator");//声明可在宏中使用的命令
  auto& material = fRadiatorMessenger->DeclareProperty(
      "material", fFoilMaterialName, "Geant4 foil material name");
  auto& foil = fRadiatorMessenger->DeclarePropertyWithUnit(
      "foilThickness", "um", fFoilThickness, "Foil thickness");
  auto& gap = fRadiatorMessenger->DeclarePropertyWithUnit(
      "gapThickness", "um", fRadiatorGap, "Air-gap thickness");
  auto& length = fRadiatorMessenger->DeclarePropertyWithUnit(
      "totalLength", "cm", fTargetLength,
      "Target radiator length; only complete periods are built");
  auto& gas = fDetectorMessenger->DeclareProperty(
      "gas", fDetectorGasName,
      "Detector gas: XeNeIsobutane, XeCO2_85_15, or XeCO2_95_5");

  enabled.SetStates(G4State_PreInit);//限制命令生效阶段
  material.SetStates(G4State_PreInit);
  foil.SetStates(G4State_PreInit);
  gap.SetStates(G4State_PreInit);
  length.SetStates(G4State_PreInit);
  gas.SetStates(G4State_PreInit);
}

DetectorConstruction::~DetectorConstruction() = default;

G4VPhysicalVolume* DetectorConstruction::Construct()
{
  // 宏已经执行完毕，现在统一检查参数并计算完整周期数。
  if (fFoilThickness <= 0. || fRadiatorGap <= 0. ||
      fTargetLength < fFoilThickness + fRadiatorGap) {
    G4Exception("DetectorConstruction", "TRD001", FatalException,
        "Foil thickness, gap thickness and total length must be positive; "
        "the total length must contain at least one complete period.");
  }
  if (fDetectorGasName != "XeNeIsobutane" &&
      fDetectorGasName != "XeCO2_85_15" &&
      fDetectorGasName != "XeCO2_95_5") {
    G4Exception("DetectorConstruction", "TRD003", FatalException,
        "Gas must be XeNeIsobutane, XeCO2_85_15, or XeCO2_95_5.");
  }
  fRadiatorLayers = static_cast<G4int>(
      std::floor(fTargetLength / (fFoilThickness + fRadiatorGap)));//向下取整
  fRadiatorLength =
      fRadiatorLayers * (fFoilThickness + fRadiatorGap);

  // ===== 1. 准备基础材料 =====
  // NIST 管理器能按标准名称创建常用材料，例如 G4_AIR、G4_MYLAR。还有目前手动定的ROHACELL_HF71
  auto* nist = G4NistManager::Instance();
  auto* air = nist->FindOrBuildMaterial("G4_AIR");
  auto* vacuum = nist->FindOrBuildMaterial("G4_Galactic");
  if (fFoilMaterialName == "ROHACELL_HF71") {
    // XTR 模型需要致密的泡沫胞壁材料。这里用 C8H11NO2 近似 PMI；
    // HF71 的体密度由宏中“胞壁厚度/空气间隙”的体积分数体现。
    fFoilMaterial = new G4Material("ROHACELL_HF71_PMI", 1.10 * g / cm3, 4);
    fFoilMaterial->AddElement(nist->FindOrBuildElement("C"), 8);
    fFoilMaterial->AddElement(nist->FindOrBuildElement("H"), 11);
    fFoilMaterial->AddElement(nist->FindOrBuildElement("N"), 1);
    fFoilMaterial->AddElement(nist->FindOrBuildElement("O"), 2);
  } else {
    fFoilMaterial = nist->FindOrBuildMaterial(fFoilMaterialName);
  }
  if (!fFoilMaterial) {
    G4Exception("DetectorConstruction", "TRD002", FatalException,
        ("Unknown foil material: " + fFoilMaterialName).c_str());
  }

  // Geant4 XTR 过程需要薄膜与间隙的独立参数；几何导航中则用按体积分数
  // 加权的等效材料表示整块辐射体，避免显式建立数百层薄膜。
  const double foilFraction = fFoilThickness /
      (fFoilThickness + fRadiatorGap);
  const double radiatorDensity = fFoilMaterial->GetDensity() * foilFraction +
      air->GetDensity() * (1.0 - foilFraction);
  auto* radiatorMaterial = new G4Material("FoilAirRadiator", radiatorDensity, 2);
  radiatorMaterial->AddMaterial(fFoilMaterial,
      fFoilMaterial->GetDensity() * foilFraction / radiatorDensity);
  radiatorMaterial->AddMaterial(air,
      air->GetDensity() * (1.0 - foilFraction) / radiatorDensity);
  fRadiatorGas = air;

  // ===== 2. 建立 World =====
  // G4Box 的后三个尺寸都是“半长”；世界取真空并留出额外边界。
  const double worldHalfLength = std::max(fRadiatorLength + 10.0 * mm,
                                           TRD::kGasLength + 10.0 * mm);
  auto* worldSolid = new G4Box("World", 50 * mm, 50 * mm, worldHalfLength);
  auto* worldLogical = new G4LogicalVolume(worldSolid, vacuum, "World");
  auto* world = new G4PVPlacement(nullptr, {}, worldLogical, "World", nullptr, false, 0);

  // ===== 3. 建立辐射体（z<0）=====
  if (fUseRadiator) {
    auto* radiatorSolid = new G4Box("Radiator", 30 * mm, 30 * mm, fRadiatorLength / 2);
    fRadiatorLogical = new G4LogicalVolume(radiatorSolid, radiatorMaterial, "Radiator");
    new G4PVPlacement(nullptr, {0, 0, -fRadiatorLength / 2}, fRadiatorLogical,
        "Radiator", worldLogical, false, 0);
    // Region 可拥有独立 production cut。较小截断有利于低能 X 射线模拟，
    // 但也会产生更多次级粒子、显著增加运行时间。
    auto* radiatorRegion = new G4Region("RadiatorRegion");
    auto* radiatorCuts = new G4ProductionCuts;
    radiatorCuts->SetProductionCut(TRD::kRadiatorProductionCut);
    radiatorRegion->SetProductionCuts(radiatorCuts);
    fRadiatorLogical->SetRegion(radiatorRegion);
    radiatorRegion->AddRootLogicalVolume(fRadiatorLogical);
    G4cout << "Radiator: " << fRadiatorLayers << " periods, material="
           << fFoilMaterialName << ", foil="
           << fFoilThickness / um << " um, gap="
           << fRadiatorGap / um << " um, modeled stack length="
           << fRadiatorLength / mm
           << " mm, production cut="
           << radiatorCuts->GetProductionCut("gamma") / um << " um" << G4endl;
  }

  // TR-only 用于只研究辐射体出口光子谱：没有气体层，TR 光子被记录后杀死。
  // 正常探测器响应模拟不会进入这个分支。
  if (fTrOnly) {
    G4cout << "TR-only mode: downstream gas detector disabled" << G4endl;
    return world;
  }

  // ===== 4. 配制气体材料 =====(目前只有三种气体混合物)
  G4Material* gas = nullptr;
  if (fDetectorGasName == "XeCO2_85_15" ||
      fDetectorGasName == "XeCO2_95_5") {
    auto* xenon = nist->FindOrBuildMaterial("G4_Xe");
    auto* carbonDioxide = nist->FindOrBuildMaterial("G4_CARBON_DIOXIDE");
    const double xenonMoleFraction =
        fDetectorGasName == "XeCO2_95_5" ? 0.95 : 0.85;
    const double carbonDioxideMoleFraction = 1.0 - xenonMoleFraction;
    const double density = xenonMoleFraction * xenon->GetDensity() +
        carbonDioxideMoleFraction * carbonDioxide->GetDensity();
    // 输入名称表示摩尔/体积分数，而 AddMaterial 需要质量分数，因此先换算。
    const double xenonMassFraction =
        xenonMoleFraction * xenon->GetDensity() / density;
    gas = new G4Material(fDetectorGasName, density, 2, kStateGas,
        293.15 * kelvin, atmosphere);
    gas->AddMaterial(xenon, xenonMassFraction);
    gas->AddMaterial(carbonDioxide, 1.0 - xenonMassFraction);
  } else {
    // Xe/Ne/异丁烷混合气；三个数字是质量分数。
    auto* xenon = nist->FindOrBuildMaterial("G4_Xe");
    auto* neon = nist->FindOrBuildMaterial("G4_Ne");
    auto* isobutane = new G4Material("isobutane", 2.51 * mg / cm3, 2);
    isobutane->AddElement(nist->FindOrBuildElement("C"), 4);
    isobutane->AddElement(nist->FindOrBuildElement("H"), 10);
    gas = new G4Material("XeNeIsobutane", 3.13 * mg / cm3, 3,
        kStateGas, 293.15 * kelvin, atmosphere);
    gas->AddMaterial(xenon, 0.8513);
    gas->AddMaterial(neon, 0.1309);
    gas->AddMaterial(isobutane, 0.0178);
  }
  // ===== 5. 沿 +z 放置气体读出层 =====
  // 所有层复用同一个逻辑体，用 copy number 区分层号（0 到 kRegions-1）。
  auto* layerSolid = new G4Box("GasLayer", 30 * mm, 30 * mm, TRD::kRegionLength / 2);
  auto* layerLogical = new G4LogicalVolume(layerSolid, gas, "GasLayer");
  for (int region = 0; region < TRD::kRegions; ++region) {
    const double z = 0.5 * TRD::kRegionLength + region * TRD::kRegionLength;
    new G4PVPlacement(nullptr, {0, 0, z}, layerLogical, "GasLayer",
        worldLogical, false, region);
  }

  // 气体 Region 使用独立的 production cut；电离仍由标准 EM 物理处理。
  auto* gasRegion = new G4Region("GasDetector");
  auto* gasCuts = new G4ProductionCuts;
  double gasProductionCut = TRD::kGasProductionCut;
  if (const char* value = std::getenv("TRD_GAS_CUT_MM"))
    gasProductionCut = std::stod(value) * mm;
  gasCuts->SetProductionCut(gasProductionCut);
  gasRegion->SetProductionCuts(gasCuts);
  layerLogical->SetRegion(gasRegion);
  gasRegion->AddRootLogicalVolume(layerLogical);
  G4cout << "GasDetector: material=" << gas->GetName() << ", density="
         << gas->GetDensity() / (mg / cm3) << " mg/cm3, "
         << TRD::kRegions << " layers, "
         << TRD::kRegionLength / mm << " mm/layer, production cut = "
         << gasCuts->GetProductionCut("e-") / mm << " mm, mean excitation energy = "
         << gas->GetIonisation()->GetMeanExcitationEnergy() / eV << " eV" << G4endl;
  // Construct() 必须把最外层物理体交还给 RunManager。
  return world;
}
