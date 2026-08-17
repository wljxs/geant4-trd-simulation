#include "PhysicsList.hh"
#include "DetectorConstruction.hh"
#include "TRDConstants.hh"

#include "G4DecayPhysics.hh"
#include "G4Electron.hh"
#include "G4EmConfigurator.hh"
#include "G4EmStandardPhysics.hh"
#include "G4GammaXTRadiator.hh"
#include "G4LossTableManager.hh"
#include "G4MuonMinus.hh"
#include "G4PAIModel.hh"
#include "G4PAIPhotModel.hh"
#include "G4PionMinus.hh"
#include "G4ProcessManager.hh"
#include "G4VPhysicsConstructor.hh"
#include "G4XTRTransparentRegRadModel.hh"
#include "G4XTRGammaRadModel.hh"
#include "G4ios.hh"

#include <cstdlib>
#include <string>

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

    // 默认 gammaM；旧扫描可通过环境变量选择另外两种 Geant4 XTR 实现。
    const char* requestedModelEnvironment = std::getenv("TRD_XTR_MODEL");
    const std::string requestedModel = requestedModelEnvironment ?
        requestedModelEnvironment : "gammaM";
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
          ("Unknown TRD_XTR_MODEL='" + requestedModel +
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

// 在名为 GasDetector 的 Region 内，用 PAI 或 PAIPhot 覆盖标准电离模型。
// PAI 更适合描述薄气体层中的能损涨落；PAIPhot 还显式处理光子通道。
class PAIPhysics final : public G4VPhysicsConstructor {
 public:
  PAIPhysics() : G4VPhysicsConstructor("GasPAI") {}
  void ConstructParticle() override {}
  void ConstructProcess() override {
    // EmConfigurator 可以只在指定 Region、指定能区替换某个 EM 模型。
    auto* configurator = G4LossTableManager::Instance()->EmConfigurator();
    const char* requestedIonisation = std::getenv("TRD_IONISATION_MODEL");
    const char* legacyPaiModel = std::getenv("TRD_PAI_MODEL");
    // 新环境变量优先；TRD_PAI_MODEL 仅为兼容旧脚本。
    const bool usePhotonModel = requestedIonisation ?
        std::string(requestedIonisation) != "pai" :
        (!legacyPaiModel || std::string(legacyPaiModel) != "pai");
    if (usePhotonModel) {
      // process 名必须和 Geant4 标准物理中对应粒子的电离过程名称一致。
      auto* muonModel = new G4PAIPhotModel(G4MuonMinus::MuonMinus());
      configurator->SetExtraEmModel("mu-", "muIoni", muonModel, "GasDetector",
          0.0, 100.0 * TeV, muonModel);
      auto* electronModel = new G4PAIPhotModel(G4Electron::Electron());
      configurator->SetExtraEmModel("e-", "eIoni", electronModel, "GasDetector",
          0.0, 100.0 * TeV, electronModel);
      auto* pionModel = new G4PAIPhotModel(G4PionMinus::PionMinus());
      configurator->SetExtraEmModel("pi-", "hIoni", pionModel, "GasDetector",
          0.0, 100.0 * TeV, pionModel);
      G4cout << "Gas ionisation model for mu-, pi-, and e-: "
             << "G4PAIPhotModel" << G4endl;
    } else {
      auto* muonModel = new G4PAIModel(G4MuonMinus::MuonMinus());
      configurator->SetExtraEmModel("mu-", "muIoni", muonModel, "GasDetector",
          0.0, 100.0 * TeV, muonModel);
      auto* electronModel = new G4PAIModel(G4Electron::Electron());
      configurator->SetExtraEmModel("e-", "eIoni", electronModel, "GasDetector",
          0.0, 100.0 * TeV, electronModel);
      auto* pionModel = new G4PAIModel(G4PionMinus::PionMinus());
      configurator->SetExtraEmModel("pi-", "hIoni", pionModel, "GasDetector",
          0.0, 100.0 * TeV, pionModel);
      G4cout << "Gas ionisation model for mu-, pi-, and e-: G4PAIModel"
             << G4endl;
    }
    // 把上面暂存的模型配置真正加入物理过程。
    configurator->AddModels();
  }
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
  const char* requestedIonisationEnvironment =
      std::getenv("TRD_IONISATION_MODEL");
  const char* legacyPaiModel = std::getenv("TRD_PAI_MODEL");

  // 选择顺序：TRD_IONISATION_MODEL > 旧变量 TRD_PAI_MODEL > standard。
  std::string requestedIonisation = requestedIonisationEnvironment ?
      requestedIonisationEnvironment : (legacyPaiModel ?
          (std::string(legacyPaiModel) == "pai" ? "pai" : "paiphot") :
          "standard");
  if (requestedIonisation != "standard" && requestedIonisation != "paiphot" &&
      requestedIonisation != "pai") {
    G4cerr << "Unknown TRD_IONISATION_MODEL='" << requestedIonisation
           << "'; using standard" << G4endl;
    requestedIonisation = "standard";
  }
  const bool useStandardIonisation = requestedIonisation == "standard";
  if (useStandardIonisation) {
    // 不注册覆盖模型，继续使用 G4EmStandardPhysics 自带的电离模型。
    G4cout << "Gas ionisation: Geant4 standard EM (no PAI override); "
           << "mu- uses G4MuIonisation/G4MuBetheBlochModel and e- uses "
           << "G4eIonisation/G4MollerBhabhaModel, with standard energy-loss "
           << "fluctuations" << G4endl;
  } else {
    RegisterPhysics(new PAIPhysics);
  }
  // 构造器始终注册；真正建立过程时再读取宏设置的 enabled 值。
  RegisterPhysics(new TransitionRadiationPhysics(detector));
}
