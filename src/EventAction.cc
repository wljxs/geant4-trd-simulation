#include "EventAction.hh"
#include "RunAction.hh"

EventAction::EventAction(RunAction* runAction) : fRunAction(runAction) {}

void EventAction::BeginOfEventAction(const G4Event*)
{
  // 新入射粒子开始：清空上一个 event 的所有累计量。
  fEnergyDeposit.fill(0.0);
  fPrimaryEnergyLoss.fill(0.0);
  fTREnergyDeposit.fill(0.0);
  fTRPhotons.clear();
  fTRExitPhotons.clear();
  fTRTrackIDs.clear();
  fDirectTRPhotonIDs.clear();
  fScoredTRExitIDs.clear();
}

void EventAction::EndOfEventAction(const G4Event*)
{
  // 一个 event 对应 ROOT TTree 中的一条记录。
  fRunAction->FillEvent(fEnergyDeposit, fPrimaryEnergyLoss, fTREnergyDeposit,
                        fTRPhotons, fTRExitPhotons);
}

void EventAction::AddTREnergyDeposit(int region, double energy)
{
  // region 是 GasLayer 的 copy number，范围为 [0, kRegions)。
  fTREnergyDeposit[region] += energy;
}

void EventAction::AddEnergyDeposit(int region, double energy)
{
  fEnergyDeposit[region] += energy;
}

void EventAction::MarkTRTrack(int trackID)
{
  // unordered_set 让后续按 track ID 查询的平均复杂度接近 O(1)。
  fTRTrackIDs.insert(trackID);
}

void EventAction::MarkDirectTRPhoton(int trackID)
{
  fDirectTRPhotonIDs.insert(trackID);
}

bool EventAction::IsTRTrack(int trackID) const
{
  return fTRTrackIDs.find(trackID) != fTRTrackIDs.end();
}

bool EventAction::AddTRExitPhoton(int trackID, double energy,
                                  double dx, double dy, double dz)
{
  if (fDirectTRPhotonIDs.find(trackID) == fDirectTRPhotonIDs.end() ||
      !fScoredTRExitIDs.insert(trackID).second) {
    return false;
  }
  fTRExitPhotons.push_back({energy, dx, dy, dz});
  return true;
}

void EventAction::AddPrimaryEnergyLoss(int region, double energy)
{
  fPrimaryEnergyLoss[region] += energy;
}

void EventAction::AddTRPhoton(double energy, double dx, double dy, double dz)
{
  // 这里记录“生成时”的光子信息，而不是进入气体后的信息。
  fTRPhotons.push_back({energy, dx, dy, dz});
}
