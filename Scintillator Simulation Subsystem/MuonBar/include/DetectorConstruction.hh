#pragma once
#include "G4VUserDetectorConstruction.hh"
#include "globals.hh"
#include "G4GenericMessenger.hh"     // needed because destructor is defaulted here
#include "G4SystemOfUnits.hh"        // for 'mm' in member initialization
#include <memory>                    // for std::unique_ptr

class G4LogicalVolume;
class G4VPhysicalVolume;

class DetectorConstruction : public G4VUserDetectorConstruction {
public:
  DetectorConstruction() = default;
  ~DetectorConstruction() override = default;
  G4VPhysicalVolume* Construct() override;

  // Helpers for names used in vis/macros
  static inline const char* WorldLVName()       { return "WorldLV"; }
  static inline const char* RodLVName()         { return "RodLV"; }
  static inline const char* SiPMWindowLVName()  { return "SiPMWindowLV"; }
  static inline const char* PhotocathodeLVName(){ return "PhotocathodeLV"; }

private:
  G4double fWinHalfXY = 3.0 * mm;                 // SiPM window half-size (square)
  std::unique_ptr<G4GenericMessenger> fMsg;       // /det/winXY messenger
};
