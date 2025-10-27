// DetectorConstruction.cc
#include "DetectorConstruction.hh"

#include "G4NistManager.hh"
#include "G4Material.hh"
#include "G4Element.hh"
#include "G4MaterialPropertiesTable.hh"

#include "G4Box.hh"
#include "G4LogicalVolume.hh"
#include "G4PVPlacement.hh"

#include "G4OpticalSurface.hh"
#include "G4LogicalBorderSurface.hh"

#include "G4GenericMessenger.hh"   // <-- needed for the /det/winXY messenger

#include "G4SystemOfUnits.hh"
#include "G4PhysicalConstants.hh"

#include <vector>

G4VPhysicalVolume* DetectorConstruction::Construct() {
  auto* nist = G4NistManager::Instance();

  // Messenger to tweak geometry before initialization (create once)
  if (!fMsg) {
    fMsg = std::make_unique<G4GenericMessenger>(this, "/det/", "Detector controls");
    fMsg->DeclarePropertyWithUnit("winXY", "mm", fWinHalfXY, "SiPM window half-size (square).");
  }

  // ---- Materials ----
  auto* elC = nist->FindOrBuildElement("C");
  auto* elH = nist->FindOrBuildElement("H");

  auto* Air    = nist->FindOrBuildMaterial("G4_AIR");
  auto* SiO2   = nist->FindOrBuildMaterial("G4_SILICON_DIOXIDE");
  auto* AlBase = nist->FindOrBuildMaterial("G4_Al");

  // EJ-200 (simplified CH)
  auto* EJ200 = new G4Material("EJ200", 1.023*g/cm3, 2);
  EJ200->AddElement(elC, 9);
  EJ200->AddElement(elH, 10);

  // Optical gel (n≈1.46)
  auto* Gel = new G4Material("OpticalGel", 1.0*g/cm3, 2);
  Gel->AddElement(elC, 5);
  Gel->AddElement(elH, 8);

  // Photon energy grid (~620…354 nm)
  const std::vector<G4double> E = {
    2.0*eV, 2.2*eV, 2.4*eV, 2.6*eV, 2.8*eV, 3.0*eV, 3.2*eV, 3.4*eV, 3.5*eV
  };

  // Indices
  const std::vector<G4double> RIAir(E.size(), 1.0003);
  const std::vector<G4double> RIEJ (E.size(), 1.58);
  const std::vector<G4double> RIGel(E.size(), 1.46);
  const std::vector<G4double> RIWin(E.size(), 1.52);

  // Absorption (generous defaults)
  const std::vector<G4double> ABS_Air(E.size(), 1e6*m);
  const std::vector<G4double> ABS_EJ (E.size(), 380*cm);
  const std::vector<G4double> ABS_Gel(E.size(), 5*m);
  const std::vector<G4double> ABS_Win(E.size(), 50*m);

  // EJ-200 emission (placeholder shape; replace with datasheet if you like)
  const std::vector<G4double> EM = {0.1, 0.25, 0.6, 1.0, 0.9, 0.6, 0.3, 0.1, 0.05};

  auto setBulkOptics = [&](G4Material* M,
                           const std::vector<G4double>& RI,
                           const std::vector<G4double>& ABS) {
    auto* mpt = new G4MaterialPropertiesTable();
    mpt->AddProperty("RINDEX",    E, RI);
    mpt->AddProperty("ABSLENGTH", E, ABS);
    M->SetMaterialPropertiesTable(mpt);
  };

  setBulkOptics(Air,  RIAir, ABS_Air);
  setBulkOptics(Gel,  RIGel, ABS_Gel);
  setBulkOptics(SiO2, RIWin, ABS_Win);

  {
    auto* mpt = new G4MaterialPropertiesTable();
    mpt->AddProperty("RINDEX",                   E, RIEJ);
    mpt->AddProperty("ABSLENGTH",                E, ABS_EJ);
    mpt->AddProperty("SCINTILLATIONCOMPONENT1",  E, EM);
    mpt->AddConstProperty("SCINTILLATIONYIELD",        10000./MeV);
    mpt->AddConstProperty("SCINTILLATIONTIMECONSTANT1", 2.1*ns);
    mpt->AddConstProperty("RESOLUTIONSCALE",            1.0);
    EJ200->SetMaterialPropertiesTable(mpt);
    EJ200->GetIonisation()->SetBirksConstant(0.156*mm/MeV);
  }

  // ---- World ----
  const G4double worldSize = 50*cm;
  auto* worldS  = new G4Box("World", worldSize/2, worldSize/2, worldSize/2);
  auto* worldLV = new G4LogicalVolume(worldS, Air, "WorldLV");
  auto* worldPV = new G4PVPlacement(nullptr, {}, worldLV, "WorldPV", nullptr, false, 0);

  // ---- EJ-200 bar: 25 x 1 x 1 cm; readout at +X ----
  const G4double Lx = 25*cm, Ly = 1*cm, Lz = 1*cm;

  auto* rodS  = new G4Box("Rod", Lx/2, Ly/2, Lz/2);
  auto* rodLV = new G4LogicalVolume(rodS, EJ200, "RodLV");
  auto* rodPV = new G4PVPlacement(nullptr, {}, rodLV, "RodPV", worldLV, false, 0);

  // ---- Reflective wrap on back & sides (leave +X open) ----
  const G4double w = 0.1*mm;

  auto* wrapYpS  = new G4Box("WrapYp", Lx/2, w/2, Lz/2);
  auto* wrapYpLV = new G4LogicalVolume(wrapYpS, Air, "WrapYpLV");
  auto* wrapYpPV = new G4PVPlacement(nullptr, {0, +Ly/2 + w/2, 0}, wrapYpLV, "WrapYpPV", worldLV, false, 0);

  auto* wrapYmS  = new G4Box("WrapYm", Lx/2, w/2, Lz/2);
  auto* wrapYmLV = new G4LogicalVolume(wrapYmS, Air, "WrapYmLV");
  auto* wrapYmPV = new G4PVPlacement(nullptr, {0, -Ly/2 - w/2, 0}, wrapYmLV, "WrapYmPV", worldLV, false, 0);

  auto* wrapZpS  = new G4Box("WrapZp", Lx/2, Ly/2, w/2);
  auto* wrapZpLV = new G4LogicalVolume(wrapZpS, Air, "WrapZpLV");
  auto* wrapZpPV = new G4PVPlacement(nullptr, {0, 0, +Lz/2 + w/2}, wrapZpLV, "WrapZpPV", worldLV, false, 0);

  auto* wrapZmS  = new G4Box("WrapZm", Lx/2, Ly/2, w/2);
  auto* wrapZmLV = new G4LogicalVolume(wrapZmS, Air, "WrapZmLV");
  auto* wrapZmPV = new G4PVPlacement(nullptr, {0, 0, -Lz/2 - w/2}, wrapZmLV, "WrapZmPV", worldLV, false, 0);

  auto* wrapBackS  = new G4Box("WrapBack", w/2, Ly/2, Lz/2);
  auto* wrapBackLV = new G4LogicalVolume(wrapBackS, Air, "WrapBackLV");
  auto* wrapBackPV = new G4PVPlacement(nullptr, {-Lx/2 - w/2, 0, 0}, wrapBackLV, "WrapBackPV", worldLV, false, 0);

  auto makeMirrorSurface = [&](const G4String& name) {
    auto* surf = new G4OpticalSurface(name, unified, groundfrontpainted, dielectric_metal);
    auto* mpt  = new G4MaterialPropertiesTable();
    const std::vector<G4double> R(E.size(), 0.98);
    mpt->AddProperty("REFLECTIVITY", E, R);
    surf->SetMaterialPropertiesTable(mpt);
    return surf;
  };

  auto* surfYp   = makeMirrorSurface("WrapYpSurf");
  auto* surfYm   = makeMirrorSurface("WrapYmSurf");
  auto* surfZp   = makeMirrorSurface("WrapZpSurf");
  auto* surfZm   = makeMirrorSurface("WrapZmSurf");
  auto* surfBack = makeMirrorSurface("WrapBackSurf");

  new G4LogicalBorderSurface("WrapYp_Rod",     rodPV,    wrapYpPV,  surfYp);
  new G4LogicalBorderSurface("WrapYp_Rod_r",   wrapYpPV, rodPV,     surfYp);
  new G4LogicalBorderSurface("WrapYm_Rod",     rodPV,    wrapYmPV,  surfYm);
  new G4LogicalBorderSurface("WrapYm_Rod_r",   wrapYmPV, rodPV,     surfYm);
  new G4LogicalBorderSurface("WrapZp_Rod",     rodPV,    wrapZpPV,  surfZp);
  new G4LogicalBorderSurface("WrapZp_Rod_r",   wrapZpPV, rodPV,     surfZp);
  new G4LogicalBorderSurface("WrapZm_Rod",     rodPV,    wrapZmPV,  surfZm);
  new G4LogicalBorderSurface("WrapZm_Rod_r",   wrapZmPV, rodPV,     surfZm);
  new G4LogicalBorderSurface("WrapBack_Rod",   rodPV,    wrapBackPV,  surfBack);
  new G4LogicalBorderSurface("WrapBack_Rod_r", wrapBackPV, rodPV,     surfBack);

  // ---- Readout stack: Gel -> Window (square fWinHalfXY x fWinHalfXY) -> Photocathode ----
  const G4double gelT = 0.10*mm;
  const G4double winT = 0.50*mm;
  const G4double pcT  = 0.01*mm;

  // Use fWinHalfXY in Y and Z so /det/winXY controls the clear aperture
  auto* gelS  = new G4Box("Gel",          gelT/2,  fWinHalfXY, fWinHalfXY);
  auto* winS  = new G4Box("SiPMWindow",   winT/2,  fWinHalfXY, fWinHalfXY);
  auto* pcS   = new G4Box("Photocathode", pcT/2,   fWinHalfXY, fWinHalfXY);

  auto* gelLV = new G4LogicalVolume(gelS, Gel,    "GelLV");
  auto* winLV = new G4LogicalVolume(winS, SiO2,   "SiPMWindowLV");
  auto* pcLV  = new G4LogicalVolume(pcS, AlBase,  "PhotocathodeLV");

  auto* gelPV = new G4PVPlacement(nullptr, {+Lx/2 + gelT/2,              0, 0}, gelLV, "GelPV",          worldLV, false, 0);
  auto* winPV = new G4PVPlacement(nullptr, {+Lx/2 + gelT + winT/2,       0, 0}, winLV, "SiPMWindowPV",   worldLV, false, 0);
  auto* pcPV  = new G4PVPlacement(nullptr, {+Lx/2 + gelT + winT + pcT/2, 0, 0}, pcLV,  "PhotocathodePV", worldLV, false, 0);

  // Photocathode border surface with EFFICIENCY (PDE)
  auto* pcSurf = new G4OpticalSurface("PhotocathodeSurface", unified, polished, dielectric_metal);
  {
    auto* pcMPT = new G4MaterialPropertiesTable();
    // Flat PDE placeholder; replace with PDE(λ) when ready
    const std::vector<G4double> PDE(E.size(), 0.60); // ~60%
    const std::vector<G4double> RPC(E.size(), 0.0);  // no reflection on miss
    pcMPT->AddProperty("EFFICIENCY",   E, PDE);
    pcMPT->AddProperty("REFLECTIVITY", E, RPC);
    pcSurf->SetMaterialPropertiesTable(pcMPT);
  }
  new G4LogicalBorderSurface("PC_win_to_pc", winPV, pcPV, pcSurf);
  new G4LogicalBorderSurface("PC_pc_to_win", pcPV, winPV, pcSurf);

  return worldPV;
}
