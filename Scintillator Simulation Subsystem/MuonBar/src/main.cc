#include "G4RunManagerFactory.hh"
#include "G4UImanager.hh"
#include "QGSP_BERT.hh"
#include "G4OpticalPhysics.hh"
#include "G4OpticalParameters.hh"
#include "G4LossTableManager.hh"
#include "G4EmSaturation.hh"
#include "ActionInitialization.hh"
#include "DetectorConstruction.hh"

#include "G4UIExecutive.hh"
#include "G4VisExecutive.hh"

int main(int argc, char** argv) {
  // Run manager
  auto* runManager =
      G4RunManagerFactory::CreateRunManager(G4RunManagerType::Default);

  // Geometry
  runManager->SetUserInitialization(new DetectorConstruction());

  // Physics list with optical processes added
  auto* phys = new QGSP_BERT();
  phys->RegisterPhysics(new G4OpticalPhysics());
  G4LossTableManager::Instance()->EmSaturation();
  runManager->SetUserInitialization(phys);

  // Optical parameters
  auto* op = G4OpticalParameters::Instance();

  // If you ONLY want scintillation, keep Cherenkov off:
  op->SetProcessActivation("Cerenkov", false);
  // op->SetProcessActivation("Cerenkov", true);
  // op->SetCerenkovMaxPhotonsPerStep(100);
  // op->SetCerenkovTrackSecondariesFirst(true);

  op->SetScintTrackSecondariesFirst(true);
  op->SetScintVerboseLevel(0);
  op->SetScintStackPhotons(true);

  // Actions
  runManager->SetUserInitialization(new ActionInitialization());

  // UI / Vis
  G4UIExecutive* ui = (argc == 1) ? new G4UIExecutive(argc, argv) : nullptr;

  if (ui) {
    // Interactive session with visualization
    auto* visManager = new G4VisExecutive();
    visManager->Initialize();

    auto* UImanager = G4UImanager::GetUIpointer();
    UImanager->ApplyCommand("/control/execute vis_win.mac");
    ui->SessionStart();

    delete visManager;
    delete ui;
  } else {
    // Pure batch mode (no vis initialisation)
    auto* UImanager = G4UImanager::GetUIpointer();
    G4String cmd   = "/control/execute ";
    G4String macro = argv[1];
    UImanager->ApplyCommand(cmd + macro);
  }

  delete runManager;
  return 0;
}
