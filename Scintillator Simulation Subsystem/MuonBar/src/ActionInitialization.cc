#include "ActionInitialization.hh"
#include "PrimaryGeneratorAction.hh"
#include "RunAction.hh"
#include "EventAction.hh"
#include "SteppingAction.hh"
#include "StackingAction.hh"  // <-- required

void ActionInitialization::Build() const {

  auto* primary = new PrimaryGeneratorAction();
  SetUserAction(primary);

  auto* run = new RunAction();
  SetUserAction(run);

  auto* evt = new EventAction(run);
  SetUserAction(evt);

  auto* step = new SteppingAction(evt);
  SetUserAction(step);

  // NEW: counts scintillation-produced optical photons at track creation
  auto* stack = new StackingAction(evt);
  SetUserAction(stack);

  // NOTE: We intentionally do NOT create any sensitive detector here.
  // Counting happens in SteppingAction at the window↔photocathode boundary.
}
