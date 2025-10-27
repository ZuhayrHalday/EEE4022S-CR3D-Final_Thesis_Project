#pragma once
#include "G4VUserPrimaryGeneratorAction.hh"

class G4Event;          // forward decl (pointer only)
class G4ParticleGun;    // forward decl

class PrimaryGeneratorAction : public G4VUserPrimaryGeneratorAction {
public:
  PrimaryGeneratorAction();
  ~PrimaryGeneratorAction() override;
  void GeneratePrimaries(G4Event* evt) override;

private:
  G4ParticleGun* fGun = nullptr;
};
