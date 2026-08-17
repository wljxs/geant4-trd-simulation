#include "PrimaryGeneratorAction.hh"
#include "DetectorConstruction.hh"

#include "G4Event.hh"
#include "G4GenericMessenger.hh"
#include "G4ParticleGun.hh"
#include "G4ParticleTable.hh"
#include "G4StateManager.hh"
#include "G4SystemOfUnits.hh"

#include <cmath>
#include <cstdlib>
#include <string>

PrimaryGeneratorAction::PrimaryGeneratorAction(
    const DetectorConstruction* detector)
    : fDetector(detector), fMomentum(3.0 * GeV)
{
  // 参数 1 表示每个 event 由粒子枪发射一个初级粒子。
  fParticleGun = new G4ParticleGun(1);

  // 保留旧扫描脚本使用的环境变量；随后执行的 .mac 命令优先。
  if (const char* value = std::getenv("TRD_MOMENTUM_GEV"))
    fMomentum = std::stod(value) * GeV;

  fBeamMessenger = std::make_unique<G4GenericMessenger>(
      this, "/trd/beam/", "Primary beam configuration");
  auto& particle = fBeamMessenger->DeclareProperty(
      "particle", fParticleName, "Primary particle: e-, pi-, or mu-");
  auto& momentum = fBeamMessenger->DeclarePropertyWithUnit(
      "momentum", "GeV", fMomentum, "Primary momentum");
  particle.SetStates(G4State_PreInit);
  momentum.SetStates(G4State_PreInit);
}

PrimaryGeneratorAction::~PrimaryGeneratorAction()
{
  delete fParticleGun;
}

void PrimaryGeneratorAction::ConfigureGun()
{
  if (fParticleName != "e-" && fParticleName != "pi-" &&
      fParticleName != "mu-") {
    G4Exception("PrimaryGeneratorAction", "TRD101", FatalException,
        "Particle must be e-, pi-, or mu-.");
  }
  if (fMomentum <= 0.0) {
    G4Exception("PrimaryGeneratorAction", "TRD102", FatalException,
        "Primary momentum must be positive.");
  }

  auto* particle =
      G4ParticleTable::GetParticleTable()->FindParticle(fParticleName);
  fParticleGun->SetParticleDefinition(particle);

  // ParticleGun 接收动能，而宏给的是动量：
  // E_kin = sqrt(p^2 + m^2) - m（Geant4 使用 c=1 的自然单位）。
  const double mass = particle->GetPDGMass();
  fParticleGun->SetParticleEnergy(
      std::sqrt(fMomentum * fMomentum + mass * mass) - mass);
  fParticleGun->SetParticleMomentumDirection({0, 0, 1});

  // 几何此时已经初始化，可直接取得宏所决定的实际辐射体长度。
  const double startZ = fDetector->UseRadiator()
      ? -fDetector->GetRadiatorLength() - 5.0 * mm
      : -5.0 * mm;
  fParticleGun->SetParticlePosition({0, 0, startZ});

  G4cout << "Primary: " << fParticleName
         << ", momentum=" << fMomentum / GeV
         << " GeV/c, gamma="
         << std::sqrt(1.0 + fMomentum * fMomentum / (mass * mass))
         << G4endl;
  fConfigured = true;
}

void PrimaryGeneratorAction::GeneratePrimaries(G4Event* event)
{
  // 宏命令在构造函数之后执行，所以推迟到第一个 event 才配置粒子枪。
  if (!fConfigured)
    ConfigureGun();
  fParticleGun->GeneratePrimaryVertex(event);
}
