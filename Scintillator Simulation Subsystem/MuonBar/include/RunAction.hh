#pragma once
#include "G4UserRunAction.hh"
#include "G4Accumulable.hh"
#include <fstream>

class RunAction : public G4UserRunAction {
public:
  RunAction();
  ~RunAction() override;
  void BeginOfRunAction(const G4Run*) override;
  void EndOfRunAction(const G4Run*) override;

  void AddDetectedPhoton(G4int n=1) { fPhotonHits += n; }

  // Files for thesis outputs (kept public so existing code still works)
  std::ofstream fCountsCSV;
  std::ofstream fHistCSV;

  // NEW: small helpers used by EventAction.cc
  inline bool CountsCSVIsOpen() const { return fCountsCSV.is_open(); }
  inline std::ofstream& CountsCSV()   { return fCountsCSV; }

private:
  G4int fPhotonHits = 0;
};
