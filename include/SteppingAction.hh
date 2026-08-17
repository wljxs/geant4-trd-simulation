#ifndef SteppingAction_h
#define SteppingAction_h 1

#include "G4UserSteppingAction.hh"

class EventAction;
class G4Step;

// 步级回调：粒子每走完一个 step 都会调用。
// 这里只关心名为 GasLayer 的体，并按层累计能量沉积和初级粒子能损。
class SteppingAction final : public G4UserSteppingAction {
 public:
  explicit SteppingAction(EventAction* eventAction);
  void UserSteppingAction(const G4Step* step) override;

 private:
  EventAction* fEventAction = nullptr;
};

#endif
