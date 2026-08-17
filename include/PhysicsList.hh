#ifndef PhysicsList_h
#define PhysicsList_h 1

#include "G4VModularPhysicsList.hh"

class DetectorConstruction;

// 物理过程清单：组合标准电磁/衰变物理，并按配置增加气体电离和 TR 模型。
// 这里决定“粒子能发生什么”，DetectorConstruction 决定“粒子在哪里运动”。
class PhysicsList final : public G4VModularPhysicsList {
 public:
  explicit PhysicsList(const DetectorConstruction* detector);
};

#endif
