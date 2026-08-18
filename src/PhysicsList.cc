#include "PhysicsList.hh"
#include "DetectorConstruction.hh"
#include "TRDConstants.hh"

#include "G4DecayPhysics.hh"
#include "G4Electron.hh"
#include "G4EmStandardPhysics.hh"
#include "G4GammaXTRadiator.hh"
#include "G4ProcessManager.hh"
#include "G4VPhysicsConstructor.hh"
#include "G4XTRTransparentRegRadModel.hh"
#include "G4XTRGammaRadModel.hh"
#include "G4ios.hh"

namespace {

// 把 TR 过程封装成可注册的 PhysicsConstructor。
// 当前只把 XTR 过程加给电子；模型利用 DetectorConstruction 提供的材料、
// 薄膜厚度、间隙厚度和周期数计算界面辐射。
class TransitionRadiationPhysics final : public G4VPhysicsConstructor {
 public:
  explicit TransitionRadiationPhysics(const DetectorConstruction* detector)
      : G4VPhysicsConstructor("TransitionRadiation"), fDetector(detector) {}
  void ConstructParticle() override {}
  void ConstructProcess() override {
    // enabled 命令在 PhysicsList 构造之后才由宏执行，因此必须在这里判断。
    if (!fDetector->UseRadiator())
      return;

    // .mac 中的 /trd/radiator/model 在 /run/initialize 前已写入。
    const auto& requestedModel = fDetector->GetXTRModel();
    G4VDiscreteProcess* process = nullptr;
    if (requestedModel == "gammaR") {
      // Gamma 分布辐射体模型。
      process = new G4GammaXTRadiator(fDetector->GetRadiatorLogical(),
          100., 100., fDetector->GetFoilMaterial(),
          fDetector->GetRadiatorGas(), fDetector->GetFoilThickness(),
          fDetector->GetRadiatorGap(), fDetector->GetRadiatorLayers(),
          "GammaXTRadiator");
    } else if (requestedModel == "gammaM") {
      // 本项目默认使用的 gamma 辐射模型。
      process = new G4XTRGammaRadModel(fDetector->GetRadiatorLogical(),
          100., 100., fDetector->GetFoilMaterial(),
          fDetector->GetRadiatorGas(), fDetector->GetFoilThickness(),
          fDetector->GetRadiatorGap(), fDetector->GetRadiatorLayers(),
          "GammaXTRadiator");
    } else if (requestedModel == "transpR") {
      // 规则透明辐射体模型。
      process = new G4XTRTransparentRegRadModel(
          fDetector->GetRadiatorLogical(), fDetector->GetFoilMaterial(),
          fDetector->GetRadiatorGas(), fDetector->GetFoilThickness(),
          fDetector->GetRadiatorGap(), fDetector->GetRadiatorLayers(),
          "RegularXTRadiator");
    } else {
      G4Exception("TransitionRadiationPhysics", "TRD201", FatalException,
          ("Unknown XTR model '" + requestedModel +
           "'; expected gammaR, gammaM, or transpR.").c_str());
      return;
    }
    process->SetVerboseLevel(0);
    // 离散过程挂到电子的 ProcessManager 后，电子穿过辐射体时才会调用它。
    G4Electron::Electron()->GetProcessManager()->AddDiscreteProcess(process);
    G4cout << "Transition-radiation model: " << requestedModel << G4endl;
  }
 private:
  const DetectorConstruction* fDetector;
};

}

PhysicsList::PhysicsList(const DetectorConstruction* detector)
{
  // 全局默认 production cut；辐射体和气体在几何中另设更小的 Region cut。
  SetDefaultCutValue(1.0 * mm);
  SetVerboseLevel(0);

  // 标准 EM 提供电离、轫致辐射、光电效应、康普顿等基础过程；
  // DecayPhysics 提供不稳定粒子的衰变。
  RegisterPhysics(new G4EmStandardPhysics(0));
  RegisterPhysics(new G4DecayPhysics(0));
  // 不注册 Region 级别的覆盖模型，所有介质都使用标准 EM 电离。
  G4cout << "Ionisation model: Geant4 standard EM (no PAI override)"
         << G4endl;
  // 构造器始终注册；真正建立过程时再读取宏设置的 enabled 值。
  RegisterPhysics(new TransitionRadiationPhysics(detector));
}
