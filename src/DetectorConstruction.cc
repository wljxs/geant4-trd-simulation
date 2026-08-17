#include "DetectorConstruction.hh"
#include "TRDConstants.hh"

#include "G4Box.hh"
#include "G4LogicalVolume.hh"
#include "G4Material.hh"
#include "G4IonisParamMat.hh"
#include "G4NistManager.hh"
#include "G4PVPlacement.hh"
#include "G4ProductionCuts.hh"
#include "G4Region.hh"
#include "G4ios.hh"

#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <string>

// 旧命令行入口：先取 TRDConstants.hh 的默认值，再允许环境变量覆盖。
DetectorConstruction::DetectorConstruction(bool useRadiator)
    : fUseRadiator(useRadiator),
      fFoilThickness(TRD::kFoilThickness),
      fRadiatorGap(TRD::kRadiatorGap),
      fRadiatorLayers(TRD::kRadiatorLayers)
{
  fTrOnly = std::getenv("TRD_TR_ONLY") != nullptr;
  if (const char* value = std::getenv("TRD_FOIL_THICKNESS_UM"))
    fFoilThickness = std::stod(value) * um;
  if (const char* value = std::getenv("TRD_RADIATOR_GAP_UM"))
    fRadiatorGap = std::stod(value) * um;
  if (const char* value = std::getenv("TRD_RADIATOR_LAYERS"))
    fRadiatorLayers = std::stoi(value);
  if (const char* value = std::getenv("TRD_FOIL_MATERIAL"))
    fFoilMaterialName = value;
  if (const char* value = std::getenv("TRD_DETECTOR_GAS"))
    fDetectorGasName = value;
  if (fFoilThickness <= 0. || fRadiatorGap <= 0. || fRadiatorLayers <= 0) {
    G4Exception("DetectorConstruction", "TRD001", FatalException,
        "Radiator foil thickness, gap thickness and layer count must be positive.");
  }
  fRadiatorLength = fRadiatorLayers * (fFoilThickness + fRadiatorGap);
}

// .mac 入口：targetLength 是目标长度，只保留能容纳的完整“薄膜+间隙”周期。
DetectorConstruction::DetectorConstruction(bool useRadiator,
    G4double foilThickness, G4double radiatorGap, G4double targetLength,
    const G4String& foilMaterialName, const G4String& detectorGasName)
    : fUseRadiator(useRadiator),
      fFoilThickness(foilThickness),
      fRadiatorGap(radiatorGap),
      fRadiatorLayers(static_cast<G4int>(
          std::floor(targetLength / (foilThickness + radiatorGap)))),
      fFoilMaterialName(foilMaterialName),
      fDetectorGasName(detectorGasName)
{
  fTrOnly = std::getenv("TRD_TR_ONLY") != nullptr;
  if (fFoilThickness <= 0. || fRadiatorGap <= 0. || fRadiatorLayers <= 0) {
    G4Exception("DetectorConstruction", "TRD001", FatalException,
        "Radiator foil thickness, gap thickness and layer count must be positive.");
  }
  fRadiatorLength = fRadiatorLayers * (fFoilThickness + fRadiatorGap);
}

G4VPhysicalVolume* DetectorConstruction::Construct()
{
  // ===== 1. 准备基础材料 =====
  // NIST 管理器能按标准名称创建常用材料，例如 G4_AIR、G4_MYLAR。
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

  // ===== 4. 配制气体材料 =====
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

  // 名称 GasDetector 很重要：PhysicsList 用这个 Region 名称只在气体中
  // 覆盖 PAI/PAIPhot 电离模型。
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
  // 可选诊断：打印 3 GeV/c μ- 对应的 beta*gamma 和密度效应修正。
  if (std::getenv("TRD_DUMP_BETHE")) {
    constexpr double muonMomentum = 3.0 * GeV;
    constexpr double muonMass = 105.6583755 * MeV;
    const double betaGamma = muonMomentum / muonMass;
    G4cout << "Bethe-Bloch material input at 3 GeV/c mu-: beta*gamma = "
           << betaGamma << ", density correction = "
           << gas->GetIonisation()->DensityCorrection(std::log10(betaGamma))
           << G4endl;
  }
  // Construct() 必须把最外层物理体交还给 RunManager。
  return world;
}
