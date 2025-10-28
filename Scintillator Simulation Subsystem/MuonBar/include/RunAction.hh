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

  // Files for outputs
  std::ofstream fCountsCSV;
  std::ofstream fHistCSV;

  // Helpers used by EventAction.cc
  inline bool CountsCSVIsOpen() const { return fCountsCSV.is_open(); }
  inline std::ofstream& CountsCSV()   { return fCountsCSV; }

private:
  G4int fPhotonHits = 0;
};
