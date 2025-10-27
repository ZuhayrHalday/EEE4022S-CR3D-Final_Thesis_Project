// StackingAction.cc
#include "StackingAction.hh"
#include "EventAction.hh"
#include "G4Track.hh"
#include "G4VProcess.hh"
#include "G4OpticalPhoton.hh"
#include "G4SystemOfUnits.hh"

G4ClassificationOfNewTrack StackingAction::ClassifyNewTrack(const G4Track* track) {
  if (track->GetDefinition() == G4OpticalPhoton::OpticalPhotonDefinition()) {
    const auto* proc = track->GetCreatorProcess();
    if (proc && proc->GetProcessName() == "Scintillation") {
      // Count photon created by EJ-200 scintillation
      fEvent->IncrementPhotonsProduced();
    }
  }
  return fUrgent;
}
