#ifndef PrimaryGeneratorAction_h
#define PrimaryGeneratorAction_h 1

#include "G4VUserPrimaryGeneratorAction.hh"
#include "globals.hh"

class G4Event;
class G4ParticleGun;

// 初级粒子源。每个 event 产生一个沿 +z 方向入射的粒子。
class PrimaryGeneratorAction final : public G4VUserPrimaryGeneratorAction {
 public:
  // 旧入口：动量取环境变量 TRD_MOMENTUM_GEV（默认 3 GeV/c）。
  PrimaryGeneratorAction(const G4String& particleName, G4double startZ);
  // .mac 入口：直接使用 SimulationConfig 给出的动量。
  PrimaryGeneratorAction(const G4String& particleName, G4double startZ,
                         G4double momentum);
  ~PrimaryGeneratorAction() override;
  // Geant4 在每个 event 开始时自动调用。
  void GeneratePrimaries(G4Event* event) override;

 private:
  G4ParticleGun* fParticleGun = nullptr;  // 本类拥有，析构时 delete
};

#endif
