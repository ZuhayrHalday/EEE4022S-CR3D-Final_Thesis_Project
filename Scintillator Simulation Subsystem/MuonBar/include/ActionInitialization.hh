#pragma once
#include "G4VUserActionInitialization.hh"

class ActionInitialization : public G4VUserActionInitialization {
public:
  ActionInitialization() = default;
  ~ActionInitialization() override = default;
  void Build() const override;
};
