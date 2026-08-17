#ifndef SimulationConfig_h
#define SimulationConfig_h 1

#include "globals.hh"

#include <memory>

class G4GenericMessenger;

// 集中保存一次模拟的可调参数，并把它们映射成 /trd/... 宏命令。
// 它必须活到宏执行完毕，否则 G4GenericMessenger 绑定的变量会失效。
class SimulationConfig {
 public:
  SimulationConfig();
  ~SimulationConfig();

  void LoadEnvironment();  // 兼容旧脚本；随后读取的 .mac 参数优先
  void Validate() const;   // 在创建几何前检查参数

  G4String particle = "e-";
  G4String foilMaterial = "G4_MYLAR";
  G4String detectorGas = "XeNeIsobutane";
  G4String outputDirectory = "output/electron_radiator";
  G4bool useRadiator = true;
  G4int events = 50000;
  G4double momentum = 0.0;
  G4double foilThickness = 0.0;
  G4double gapThickness = 0.0;
  G4double totalLength = 0.0;

 private:
  // 每个 Messenger 提供一组宏命令，例如 fBeamMessenger 对应 /trd/beam/。
  std::unique_ptr<G4GenericMessenger> fBeamMessenger;
  std::unique_ptr<G4GenericMessenger> fRadiatorMessenger;
  std::unique_ptr<G4GenericMessenger> fDetectorMessenger;
  std::unique_ptr<G4GenericMessenger> fRunMessenger;
  std::unique_ptr<G4GenericMessenger> fOutputMessenger;
};

#endif
