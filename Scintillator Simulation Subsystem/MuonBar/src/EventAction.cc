#include "EventAction.hh"
#include "RunAction.hh"
#include "G4Event.hh"
#include "G4SystemOfUnits.hh"

#include <vector>

EventAction::EventAction(RunAction* ra) : fRun(ra) {}
EventAction::~EventAction() {}

void EventAction::BeginOfEventAction(const G4Event*) {
  fTimes.clear();

  // Reset per-event accumulators
  fMuonPath_mm      = 0.0;
  fMuonDE_MeV       = 0.0;
  fPhotonsProduced  = 0;
  fPhotonsArrived   = 0;
}

void EventAction::RecordHitTime(double t_ns) {
  // collect times within window
  if (t_ns >= fTmin && t_ns < fTmax) fTimes.push_back(t_ns);
}

void EventAction::EndOfEventAction(const G4Event* evt) {
  // --- Derived observables ---
  const int    Ndet = static_cast<int>(fTimes.size());  // detected photons (via OpBoundary::Detection)
  const double path_cm = fMuonPath_mm * 0.1;            // mm → cm
  const double dedx    = (path_cm > 0.) ? (fMuonDE_MeV / path_cm) : 0.0; // MeV/cm

  // SiPM charge/current proxy (document Gain & tau as assumptions)
  const double q_e  = 1.602176634e-19 * coulomb; // C
  const double Gain = 1.0e6;
  const double tau  = 30.0 * ns;
  const double Q    = Ndet * Gain * q_e;         // Coulombs
  const double Iest = (tau > 0.) ? (Q / tau) : 0.0; // Amperes

  // --- Write expanded per-event row ---
  if (fRun && fRun->CountsCSVIsOpen()) {
    fRun->CountsCSV()
      << evt->GetEventID()
      << "," << fMuonPath_mm             // muon_path_mm
      << "," << dedx                     // muon_dEdx_MeV_per_cm
      << "," << fPhotonsProduced         // photons_produced
      << "," << fPhotonsArrived          // photons_arrived_window
      << "," << Ndet                     // photons_detected
      << "," << Q                        // sipm_charge_C
      << "," << Iest                     // sipm_est_current_A
      << "\n";
  }

  // --- Time histogram for detected photons ---
  const int nbins = static_cast<int>((fTmax - fTmin) / fBin);
  std::vector<int> h(nbins, 0);
  for (double t : fTimes) {
    const int b = static_cast<int>((t - fTmin) / fBin);
    if (b >= 0 && b < nbins) h[b]++;
  }
  if (fRun && fRun->fHistCSV.is_open()) {
    for (int i = 0; i < nbins; ++i) {
      const double binCenter = fTmin + (i + 0.5) * fBin;
      fRun->fHistCSV << evt->GetEventID() << "," << binCenter << "," << h[i] << "\n";
    }
  }

  // --- Run accumulator (total hits) ---
  if (fRun) fRun->AddDetectedPhoton(Ndet);
}
