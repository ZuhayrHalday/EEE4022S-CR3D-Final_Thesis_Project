#include "RunAction.hh"
#include "G4Run.hh"
#include "G4SystemOfUnits.hh"
#include <filesystem>
#include <fstream>

RunAction::RunAction() {}

RunAction::~RunAction() {}

void RunAction::BeginOfRunAction(const G4Run*) {
  // reset per-run accumulator used for the console summary
  fPhotonHits = 0;

  // Ensure output folder exists
  std::filesystem::create_directory("output");

  // ----- photon_counts.csv (append; write header only if new/empty)
  const char* countsPath = "output/photon_counts.csv";
  bool writeCountsHeader = !std::filesystem::exists(countsPath)
                        || std::filesystem::file_size(countsPath) == 0;

  fCountsCSV.open(countsPath, std::ios::out | std::ios::app);
  if (writeCountsHeader) {
    // Keep this header EXACTLY in sync with EventAction.cc row layout
    fCountsCSV
      << "event_id"
      << ",muon_path_mm"
      << ",muon_dEdx_MeV_per_cm"
      << ",photons_produced"
      << ",photons_arrived_window"
      << ",photons_detected"
      << ",sipm_charge_C"
      << ",sipm_est_current_A"
      << "\n";
    fCountsCSV.flush();
  }

  // ----- time_histograms.csv (append; write header only if new/empty)
  const char* histPath = "output/time_histograms.csv";
  bool writeHistHeader = !std::filesystem::exists(histPath)
                      || std::filesystem::file_size(histPath) == 0;

  fHistCSV.open(histPath, std::ios::out | std::ios::app);
  if (writeHistHeader) {
    fHistCSV << "event_id,bin_ns,count\n";
    fHistCSV.flush();
  }
}

void RunAction::EndOfRunAction(const G4Run* run) {
  G4cout << "=== Run summary ===\n";
  G4cout << "Events: " << run->GetNumberOfEvent() << G4endl;
  G4cout << "Total detected photons: " << fPhotonHits << G4endl;

  if (fCountsCSV.is_open()) {
    fCountsCSV.flush();
    fCountsCSV.close();
  }
  if (fHistCSV.is_open()) {
    fHistCSV.flush();
    fHistCSV.close();
  }
}
