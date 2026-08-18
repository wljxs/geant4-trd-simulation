#ifndef DetectorConstruction_h
#define DetectorConstruction_h 1

#include "G4VUserDetectorConstruction.hh"
#include "globals.hh"

#include <memory>

class G4GenericMessenger;
class G4LogicalVolume;
class G4Material;
class G4VPhysicalVolume;

// 几何构造类：RunManager 在 Initialize() 时调用 Construct()。
// 它负责建立真空世界、可选的 TR 辐射体，以及下游 21 层气体探测器。
class DetectorConstruction final : public G4VUserDetectorConstruction {
 public:
  // 构造时注册 /trd/radiator/... 和 /trd/detector/... 宏命令。
  DetectorConstruction();
  ~DetectorConstruction() override;
  // Geant4 要求返回最外层 World 的物理体。
  G4VPhysicalVolume* Construct() override;

  // PhysicsList 通过这些只读接口取得 TR 模型所需的几何和材料参数。
  bool UseRadiator() const { return fUseRadiator; }
  bool TrOnly() const { return fTrOnly; }
  bool ScoreExitFlux() const { return fScoreExitFlux; }
  G4double GetExitPlaneZ() const { return fExitPlaneDistance; }
  const G4String& GetXTRModel() const { return fXTRModel; }
  G4LogicalVolume* GetRadiatorLogical() const { return fRadiatorLogical; }
  G4Material* GetFoilMaterial() const { return fFoilMaterial; }
  G4Material* GetRadiatorGas() const { return fRadiatorGas; }
  G4double GetFoilThickness() const { return fFoilThickness; }
  G4double GetRadiatorGap() const { return fRadiatorGap; }
  G4int GetRadiatorLayers() const { return fRadiatorLayers; }
  G4double GetRadiatorLength() const { return fRadiatorLength; }

 private:
  // fTrOnly=true 时只产生/统计 TR 光子，不建立下游气体探测器。
  bool fUseRadiator = true;
  bool fTrOnly = false;
  bool fScoreExitFlux = true;
  G4String fXTRModel = "gammaR";
  // 辐射体下游表面固定在 z=0；该值也是虚拟计分面的 z。
  G4double fExitPlaneDistance = 0.;
  G4double fFoilThickness = 0.;
  G4double fRadiatorGap = 0.;
  G4double fTargetLength = 0.;
  G4int fRadiatorLayers = 0;
  G4double fRadiatorLength = 0.;
  G4String fFoilMaterialName = "G4_MYLAR";
  G4String fDetectorGasName = "XeNeIsobutane";
  G4Material* fFoilMaterial = nullptr;
  G4Material* fRadiatorGas = nullptr;
  // 以下指针指向 Geant4 管理的对象，本类不负责 delete。
  G4LogicalVolume* fRadiatorLogical = nullptr;

  std::unique_ptr<G4GenericMessenger> fRadiatorMessenger;
  std::unique_ptr<G4GenericMessenger> fDetectorMessenger;
  std::unique_ptr<G4GenericMessenger> fModeMessenger;
  std::unique_ptr<G4GenericMessenger> fScoringMessenger;
};

#endif
