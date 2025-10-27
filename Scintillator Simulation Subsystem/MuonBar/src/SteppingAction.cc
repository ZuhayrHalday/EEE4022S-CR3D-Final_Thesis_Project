// SteppingAction.cc
#include "SteppingAction.hh"
#include "EventAction.hh"

#include "G4Step.hh"
#include "G4Track.hh"
#include "G4OpticalPhoton.hh"
#include "G4OpBoundaryProcess.hh"
#include "G4ProcessManager.hh"
#include "G4SystemOfUnits.hh"

SteppingAction::SteppingAction(EventAction* evt) : fEvent(evt) {}

void SteppingAction::UserSteppingAction(const G4Step* step) {
  auto* track = step->GetTrack();
  auto* pre   = step->GetPreStepPoint();
  auto* post  = step->GetPostStepPoint();

  // --- 1) Muon path length & energy deposition inside EJ-200 ---
  // Run this BEFORE restricting to optical photons, so muon steps are handled.
  if (track->GetDefinition()->GetParticleName() == "mu-" ||
      track->GetDefinition()->GetParticleName() == "mu+") {
    if (pre && pre->GetTouchableHandle()->GetVolume()) {
      auto* preLV = pre->GetTouchableHandle()->GetVolume()->GetLogicalVolume();
      if (preLV && preLV->GetName() == "RodLV") {
        if (fEvent) {
          fEvent->AddMuonPath(step->GetStepLength()/mm);        // mm
          fEvent->AddMuonDE(step->GetTotalEnergyDeposit()/MeV); // MeV
        }
      }
    }
  }

  // From here on, we only care about optical photons.
  if (track->GetDefinition() != G4OpticalPhoton::Definition()) return;

  // Only act at geometry boundaries.
  if (!post || post->GetStepStatus() != fGeomBoundary) return;

  // --- 2) Count arrivals at the SiPM window (transport, before PDE) ---
  if (post->GetTouchableHandle()->GetVolume()) {
    auto* postLV = post->GetTouchableHandle()->GetVolume()->GetLogicalVolume();
    if (postLV && postLV->GetName() == "SiPMWindowLV") {
      if (fEvent) fEvent->IncrementPhotonsArrived();
      // Do NOT kill: photon must continue to the window↔photocathode boundary for Detection.
    }
  }

  // --- 3) Detection at the window↔photocathode boundary via OpBoundary ---
  // Find OpBoundary for THIS thread; do not cache across threads.
  auto* pm = track->GetDefinition()->GetProcessManager();
  if (!pm) return;
  auto* postVec = pm->GetPostStepProcessVector(typeDoIt);
  G4OpBoundaryProcess* boundary = nullptr;
  for (size_t i = 0; i < postVec->size(); ++i) {
    auto* p = (*postVec)[i];
    if (p && p->GetProcessName() == "OpBoundary") {
      boundary = static_cast<G4OpBoundaryProcess*>(p);
      break;
    }
  }
  if (!boundary) return;

  if (boundary->GetStatus() == Detection) {
    if (fEvent) fEvent->RecordHitTime(post->GetGlobalTime() / ns);
    // Tidy up: stop tracking after a detection (matches your original behavior)
    track->SetTrackStatus(fStopAndKill);
  }
}
