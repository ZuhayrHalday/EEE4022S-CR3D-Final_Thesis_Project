#pragma once
#include "G4UserEventAction.hh"
#include "globals.hh"
#include <vector>

class G4Event;
class RunAction;

class EventAction : public G4UserEventAction {
public:
  explicit EventAction(RunAction* ra);
  ~EventAction() override;

  void BeginOfEventAction(const G4Event*) override;
  void EndOfEventAction(const G4Event*) override;

  // Record a detected-photon arrival time in ns
  void RecordHitTime(double t_ns);

  // ---- Per-event accumulator API
  inline void AddMuonPath(G4double dl_mm)   { fMuonPath_mm += dl_mm; }     // mm
  inline void AddMuonDE  (G4double dE_MeV)  { fMuonDE_MeV  += dE_MeV; }    // MeV

  inline void IncrementPhotonsProduced()    { ++fPhotonsProduced; }
  inline void IncrementPhotonsArrived()     { ++fPhotonsArrived;  }

  // Getters used internally by EventAction.cc at end of event
  inline G4double GetMuonPath_mm()    const { return fMuonPath_mm; }
  inline G4double GetMuonDE_MeV()     const { return fMuonDE_MeV;  }
  inline G4int    GetPhotonsProduced()const { return fPhotonsProduced; }
  inline G4int    GetPhotonsArrived() const { return fPhotonsArrived;  }

  std::vector<double> fTimes;

  // Time histogram window (ns) and bin (ns) used in EventAction.cc
  double fTmin = 0.0;
  double fTmax = 200.0;
  double fBin  = 1.0;

private:
  RunAction* fRun = nullptr;

  // Per-event accumulators
  G4double fMuonPath_mm     = 0.0;  // total muon step length in scintillator (mm)
  G4double fMuonDE_MeV      = 0.0;  // total energy deposit in scintillator (MeV)
  G4int    fPhotonsProduced = 0;    // scintillation-produced photons
  G4int    fPhotonsArrived  = 0;    // photons entering SiPM window
};
