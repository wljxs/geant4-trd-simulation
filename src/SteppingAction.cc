#include "SteppingAction.hh"
#include "DetectorConstruction.hh"
#include "EventAction.hh"

#include "G4Gamma.hh"
#include "G4GeometryTolerance.hh"
#include "G4Step.hh"
#include "G4Track.hh"
#include "G4VPhysicalVolume.hh"

SteppingAction::SteppingAction(EventAction* eventAction,
                               const DetectorConstruction* detector)
    : fEventAction(eventAction), fDetector(detector) {}

void SteppingAction::UserSteppingAction(const G4Step* step)
{
  auto* track = step->GetTrack();
  if (fDetector->ScoreExitFlux() &&
      track->GetDefinition() == G4Gamma::Gamma()) {
    const auto* pre = step->GetPreStepPoint();
    const auto* post = step->GetPostStepPoint();
    const double planeZ = fDetector->GetExitPlaneZ();
    const double tolerance =
        G4GeometryTolerance::GetInstance()->GetSurfaceTolerance();
    const double preZ = pre->GetPosition().z();
    const double postZ = post->GetPosition().z();
    // 虚拟面不增加几何体或材料；对从 z=0 创建的出口通量也有效。
    if (track->GetMomentumDirection().z() > 0.0 &&
        preZ <= planeZ + 0.5 * tolerance &&
        postZ >= planeZ - 0.5 * tolerance) {
      const auto& direction = track->GetMomentumDirection();
      if (fEventAction->AddTRExitPhoton(track->GetTrackID(),
              pre->GetKineticEnergy(), direction.x(), direction.y(),
              direction.z()) && fDetector->TrOnly()) {
        track->SetTrackStatus(fStopAndKill);
      }
    }
  }

  // PreStepPoint 表示本 step 起点所在的体。只统计气体层，忽略世界和辐射体。
  auto* volume = step->GetPreStepPoint()->GetTouchableHandle()->GetVolume();
  if (!volume || volume->GetName() != "GasLayer") return;

  // DetectorConstruction 放置气体层时，用 copy number 保存了数组下标。
  const int region = volume->GetCopyNo();

  // 总沉积能包括本 step 中所有局域沉积，不等同于轨迹动能变化。
  fEventAction->AddEnergyDeposit(region, step->GetTotalEnergyDeposit());

  // 如果当前轨迹属于 TR 光子或其后代，同时计入 TR 分量。
  if (fEventAction->IsTRTrack(track->GetTrackID()))
    fEventAction->AddTREnergyDeposit(region, step->GetTotalEnergyDeposit());

  // ParentID==0 表示入射初级粒子。这里记录它在本 step 前后的动能差；
  // 该量可包含产生次级粒子带走的能量，因此与局域沉积能并不相同。
  if (track->GetParentID() == 0) {
    const double energyLoss = step->GetPreStepPoint()->GetKineticEnergy() -
        step->GetPostStepPoint()->GetKineticEnergy();
    if (energyLoss > 0.0)
      fEventAction->AddPrimaryEnergyLoss(region, energyLoss);
  }
}
