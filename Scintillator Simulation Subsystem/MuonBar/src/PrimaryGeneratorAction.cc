#include "PrimaryGeneratorAction.hh"
#include "G4Event.hh"
#include "G4ParticleGun.hh"
#include "G4ParticleTable.hh"
#include "G4SystemOfUnits.hh"

PrimaryGeneratorAction::PrimaryGeneratorAction()
: G4VUserPrimaryGeneratorAction()
, fGun(new G4ParticleGun(1))
{
  auto mu = G4ParticleTable::GetParticleTable()->FindParticle("mu-");
  fGun->SetParticleDefinition(mu);
  // Defaults; macros can override
  fGun->SetParticleEnergy(3.*GeV);
  fGun->SetParticlePosition(G4ThreeVector(-150.*mm, 0., 0.));
  fGun->SetParticleMomentumDirection(G4ThreeVector(1., 0., 0.));
}

PrimaryGeneratorAction::~PrimaryGeneratorAction() { delete fGun; }

void PrimaryGeneratorAction::GeneratePrimaries(G4Event* event) {
  fGun->GeneratePrimaryVertex(event);
}
