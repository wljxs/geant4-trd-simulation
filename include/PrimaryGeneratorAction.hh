#ifndef PrimaryGeneratorAction_h
#define PrimaryGeneratorAction_h 1

#include "G4VUserPrimaryGeneratorAction.hh"
#include "globals.hh"

#include <memory>

class DetectorConstruction;
class G4Event;
class G4GenericMessenger;
class G4ParticleGun;

// 初级粒子源。它直接拥有 /trd/beam/... 宏命令，不再经过配置中间层。
class PrimaryGeneratorAction final : public G4VUserPrimaryGeneratorAction {
 public:
  explicit PrimaryGeneratorAction(const DetectorConstruction* detector);
  ~PrimaryGeneratorAction() override;

  // Geant4 在每个 event 开始时自动调用。
  void GeneratePrimaries(G4Event* event) override;
  const G4String& GetParticleName() const { return fParticleName; }

 private:
  void ConfigureGun();

  const DetectorConstruction* fDetector = nullptr;  // 不拥有
  G4ParticleGun* fParticleGun = nullptr;             // 本类拥有
  G4String fParticleName = "e-";
  G4double fMomentum = 0.0;
  G4bool fConfigured = false;
  std::unique_ptr<G4GenericMessenger> fBeamMessenger;
};

#endif
