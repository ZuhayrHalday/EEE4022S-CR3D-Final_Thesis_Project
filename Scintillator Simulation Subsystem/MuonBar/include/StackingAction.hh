// StackingAction.hh
#pragma once
#include "G4UserStackingAction.hh"
class EventAction;

class StackingAction : public G4UserStackingAction {
public:
  explicit StackingAction(EventAction* evt) : fEvent(evt) {}
  G4ClassificationOfNewTrack ClassifyNewTrack(const G4Track* track) override;
private:
  EventAction* fEvent;
};
