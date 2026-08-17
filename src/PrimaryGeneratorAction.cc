#include "PrimaryGeneratorAction.hh"

#include "G4Event.hh"
#include "G4ParticleGun.hh"
#include "G4ParticleTable.hh"
#include "G4SystemOfUnits.hh"

#include <cmath>
#include <cstdlib>
#include <string>

PrimaryGeneratorAction::PrimaryGeneratorAction(const G4String& particleName,
                                               G4double startZ)
{
  // 参数 1 表示每个 event 由粒子枪发射一个初级粒子。
  fParticleGun = new G4ParticleGun(1);
  auto* particle = G4ParticleTable::GetParticleTable()->FindParticle(particleName);
  fParticleGun->SetParticleDefinition(particle);
  double momentumGeV = 3.0;
  if (const char* value = std::getenv("TRD_MOMENTUM_GEV"))
    momentumGeV = std::stod(value);
  if (momentumGeV <= 0.0)
    G4Exception("PrimaryGeneratorAction", "TRD003", FatalException,
                "TRD_MOMENTUM_GEV must be positive.");
  const double momentum = momentumGeV * GeV;
  const double mass = particle->GetPDGMass();
  // ParticleGun 接收动能，而用户给的是动量：
  // E_kin = sqrt(p^2 + m^2) - m（Geant4 使用 c=1 的自然单位）。
  fParticleGun->SetParticleEnergy(std::sqrt(momentum * momentum + mass * mass) - mass);
  // 束流从辐射体上游沿 +z 方向、从横向中心 (x,y)=(0,0) 入射。
  fParticleGun->SetParticleMomentumDirection({0, 0, 1});
  fParticleGun->SetParticlePosition({0, 0, startZ});
  G4cout << "Primary: " << particleName << ", momentum=" << momentumGeV
         << " GeV/c, gamma=" << std::sqrt(1.0 + momentum * momentum /
         (mass * mass)) << G4endl;
}

PrimaryGeneratorAction::PrimaryGeneratorAction(const G4String& particleName,
                                               G4double startZ,
                                               G4double momentum)
{
  // 与上一个构造函数物理含义相同，只是动量直接来自 .mac。
  fParticleGun = new G4ParticleGun(1);
  auto* particle = G4ParticleTable::GetParticleTable()->FindParticle(particleName);
  fParticleGun->SetParticleDefinition(particle);
  if (momentum <= 0.0)
    G4Exception("PrimaryGeneratorAction", "TRD003", FatalException,
                "Primary momentum must be positive.");
  const double mass = particle->GetPDGMass();
  fParticleGun->SetParticleEnergy(std::sqrt(momentum * momentum + mass * mass) - mass);
  fParticleGun->SetParticleMomentumDirection({0, 0, 1});
  fParticleGun->SetParticlePosition({0, 0, startZ});
  G4cout << "Primary: " << particleName << ", momentum=" << momentum / GeV
         << " GeV/c, gamma=" << std::sqrt(1.0 + momentum * momentum /
         (mass * mass)) << G4endl;
}

PrimaryGeneratorAction::~PrimaryGeneratorAction() { delete fParticleGun; }

void PrimaryGeneratorAction::GeneratePrimaries(G4Event* event)
{
  // 在当前 event 中真正创建 primary vertex；粒子枪的定义在构造时已完成。
  fParticleGun->GeneratePrimaryVertex(event);
}
