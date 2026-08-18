#ifndef SteppingAction_h
#define SteppingAction_h 1

#include "G4UserSteppingAction.hh"

class EventAction;
class DetectorConstruction;
class G4Step;

// 步级回调：记录直接 TR 光子穿过虚拟出口面的通量；存在气体时，
// 还会按 GasLayer 累计能量沉积和初级粒子能损。
class SteppingAction final : public G4UserSteppingAction {
 public:
  SteppingAction(EventAction* eventAction,
                 const DetectorConstruction* detector);
  void UserSteppingAction(const G4Step* step) override;

 private:
  EventAction* fEventAction = nullptr;
  const DetectorConstruction* fDetector = nullptr;
};

#endif
