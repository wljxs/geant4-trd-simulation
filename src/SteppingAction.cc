#include "SteppingAction.hh"
#include "EventAction.hh"

#include "G4Step.hh"
#include "G4Track.hh"
#include "G4VPhysicalVolume.hh"

SteppingAction::SteppingAction(EventAction* eventAction) : fEventAction(eventAction) {}

void SteppingAction::UserSteppingAction(const G4Step* step)
{
  // PreStepPoint 表示本 step 起点所在的体。只统计气体层，忽略世界和辐射体。
  auto* volume = step->GetPreStepPoint()->GetTouchableHandle()->GetVolume();
  if (!volume || volume->GetName() != "GasLayer") return;

  // DetectorConstruction 放置气体层时，用 copy number 保存了数组下标。
  const int region = volume->GetCopyNo();

  // 总沉积能包括本 step 中所有局域沉积，不等同于轨迹动能变化。
  fEventAction->AddEnergyDeposit(region, step->GetTotalEnergyDeposit());

  // 如果当前轨迹属于 TR 光子或其后代，同时计入 TR 分量。
  if (fEventAction->IsTRTrack(step->GetTrack()->GetTrackID()))
    fEventAction->AddTREnergyDeposit(region, step->GetTotalEnergyDeposit());

  // ParentID==0 表示入射初级粒子。这里记录它在本 step 前后的动能差；
  // 该量可包含产生次级粒子带走的能量，因此与局域沉积能并不相同。
  if (step->GetTrack()->GetParentID() == 0) {
    const double energyLoss = step->GetPreStepPoint()->GetKineticEnergy() -
        step->GetPostStepPoint()->GetKineticEnergy();
    if (energyLoss > 0.0)
      fEventAction->AddPrimaryEnergyLoss(region, energyLoss);
  }
}
