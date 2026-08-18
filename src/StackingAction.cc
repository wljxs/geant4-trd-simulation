#include "StackingAction.hh"
#include "DetectorConstruction.hh"
#include "EventAction.hh"

#include "G4EmProcessSubType.hh"
#include "G4Gamma.hh"
#include "G4Track.hh"
#include "G4VProcess.hh"

StackingAction::StackingAction(EventAction* eventAction,
                               const DetectorConstruction* detector)
    : fEventAction(eventAction), fDetector(detector) {}

G4ClassificationOfNewTrack StackingAction::ClassifyNewTrack(const G4Track* track)
{
  // ParentID==0 是粒子枪产生的初级粒子，不可能是 TR 次级粒子。
  if (track->GetParentID() == 0)
    return fUrgent;

  // 若父轨迹属于 TR 链，其所有后代也标为 TR 来源。
  if (fEventAction->IsTRTrack(track->GetParentID()))
    fEventAction->MarkTRTrack(track->GetTrackID());

  // 只有 gamma 才可能是刚由 XTR 过程直接产生的 TR 光子。
  if (track->GetDefinition() != G4Gamma::Gamma())
    return fUrgent;

  // 优先检查标准的 process subtype，同时用名称兼容不同 XTR 实现。
  const auto* creator = track->GetCreatorProcess();
  if (creator && (creator->GetProcessSubType() == fTransitionRadiation ||
                  creator->GetProcessName().find("XTRadiator") != std::string::npos)) {
    const auto& direction = track->GetMomentumDirection();
    fEventAction->MarkTRTrack(track->GetTrackID());
    fEventAction->MarkDirectTRPhoton(track->GetTrackID());
    fEventAction->AddTRPhoton(track->GetKineticEnergy(),
        direction.x(), direction.y(), direction.z());
    // 启用出口计分时必须先输运到虚拟面，再由 SteppingAction 杀死。
    if (fDetector->TrOnly() && !fDetector->ScoreExitFlux()) return fKill;
  }

  return fUrgent;
}
