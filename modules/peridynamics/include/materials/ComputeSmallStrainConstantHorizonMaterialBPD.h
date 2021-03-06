//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "ComputeSmallStrainMaterialBaseBPD.h"

class ComputeSmallStrainConstantHorizonMaterialBPD;

template <>
InputParameters validParams<ComputeSmallStrainConstantHorizonMaterialBPD>();

/**
 * Material class for bond based peridynamic solid mechanics model based on regular spatial
 * discretization
 */
class ComputeSmallStrainConstantHorizonMaterialBPD : public ComputeSmallStrainMaterialBaseBPD
{
public:
  static InputParameters validParams();

  ComputeSmallStrainConstantHorizonMaterialBPD(const InputParameters & parameters);

protected:
  virtual void computePeridynamicsParams() override;
};
