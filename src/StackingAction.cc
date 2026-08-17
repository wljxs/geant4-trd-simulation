#include "StackingAction.hh"
#include "EventAction.hh"

#include "G4EmProcessSubType.hh"
#include "G4Gamma.hh"
#include "G4Track.hh"
#include "G4VProcess.hh"

#include <cstdlib>

StackingAction::StackingAction(EventAction* eventAction)
    : fEventAction(eventAction) {}

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
    fEventAction->AddTRPhoton(track->GetKineticEnergy(),
        direction.x(), direction.y(), direction.z());
    // 只研究生成谱时，信息记录完即可杀死光子，以节省输运时间。
    if (std::getenv("TRD_TR_ONLY")) return fKill;
  }

  return fUrgent;
}
