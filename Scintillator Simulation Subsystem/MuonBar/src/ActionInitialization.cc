#include "ActionInitialization.hh"
#include "PrimaryGeneratorAction.hh"
#include "RunAction.hh"
#include "EventAction.hh"
#include "SteppingAction.hh"
#include "StackingAction.hh" 

void ActionInitialization::Build() const {

  auto* primary = new PrimaryGeneratorAction();
  SetUserAction(primary);

  auto* run = new RunAction();
  SetUserAction(run);

  auto* evt = new EventAction(run);
  SetUserAction(evt);

  auto* step = new SteppingAction(evt);
  SetUserAction(step);

  // Counts scintillation-produced optical photons at track creation
  auto* stack = new StackingAction(evt);
  SetUserAction(stack);
}
