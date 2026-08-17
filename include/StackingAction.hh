#ifndef StackingAction_h
#define StackingAction_h 1

#include "G4UserStackingAction.hh"

class EventAction;
class G4Track;

// 新轨迹分类回调：每当 Geant4 创建一个次级粒子时调用。
// 本项目用它识别 TR 光子，并把“TR 来源”标记传递给光子的后代粒子。
class StackingAction final : public G4UserStackingAction {
 public:
  explicit StackingAction(EventAction* eventAction);
  // 返回 fUrgent 表示正常追踪；fKill 表示直接丢弃该轨迹。
  G4ClassificationOfNewTrack ClassifyNewTrack(const G4Track* track) override;

 private:
  EventAction* fEventAction = nullptr;
};

#endif
