#ifndef DetectorConstruction_h
#define DetectorConstruction_h 1

#include "G4VUserDetectorConstruction.hh"
#include "globals.hh"

class G4LogicalVolume;
class G4Material;
class G4VPhysicalVolume;

// 几何构造类：RunManager 在 Initialize() 时调用 Construct()。
// 它负责建立真空世界、可选的 TR 辐射体，以及下游 21 层气体探测器。
class DetectorConstruction final : public G4VUserDetectorConstruction {
 public:
  // 兼容旧命令行的构造函数：参数主要来自编译默认值或环境变量。
  explicit DetectorConstruction(bool useRadiator);
  // .mac 模式使用的构造函数：几何参数已经由 SimulationConfig 读好。
  DetectorConstruction(bool useRadiator, G4double foilThickness,
      G4double radiatorGap, G4double targetLength,
      const G4String& foilMaterialName,
      const G4String& detectorGasName = "XeNeIsobutane");
  // Geant4 要求返回最外层 World 的物理体。
  G4VPhysicalVolume* Construct() override;

  // PhysicsList 通过这些只读接口取得 TR 模型所需的几何和材料参数。
  bool UseRadiator() const { return fUseRadiator; }
  bool TrOnly() const { return fTrOnly; }
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
  G4double fFoilThickness = 0.;
  G4double fRadiatorGap = 0.;
  G4int fRadiatorLayers = 0;
  G4double fRadiatorLength = 0.;
  G4String fFoilMaterialName = "G4_MYLAR";
  G4String fDetectorGasName = "XeNeIsobutane";
  G4Material* fFoilMaterial = nullptr;
  G4Material* fRadiatorGas = nullptr;
  // 以下指针指向 Geant4 管理的对象，本类不负责 delete。
  G4LogicalVolume* fRadiatorLogical = nullptr;
};

#endif
