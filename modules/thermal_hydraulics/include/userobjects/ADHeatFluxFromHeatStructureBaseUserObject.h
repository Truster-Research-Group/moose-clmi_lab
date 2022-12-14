//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "ElementUserObject.h"
#include "FlowChannelAlignment.h"

/**
 * Base class for caching heat flux between a flow channel and a heat structure.
 *
 * We provide an API for child classes that they need to implement:
 * 1. computeQpHeatFlux() to compute heat flux at a quadrature point
 * 2. computeQpHeatFluxJacobian() to compute the jacobian of the heat flux
 *    computed by the computeQpHeatFlux() method.
 *
 * There are 2 different clients to the values we cached:
 * 1. BoundaryFluxXYZBC to apply the heat flux on a heat structure boundary
 * 2. OneDXYZEnergyHeatFlux to apply the heat flux on the flow channel side.
 */
class ADHeatFluxFromHeatStructureBaseUserObject : public ElementUserObject
{
public:
  ADHeatFluxFromHeatStructureBaseUserObject(const InputParameters & parameters);

  virtual void initialize() override;
  virtual void execute() override;
  virtual void finalize() override;
  virtual void threadJoin(const UserObject & y) override;

  const std::vector<ADReal> & getHeatedPerimeter(dof_id_type element_id) const;
  const std::vector<ADReal> & getHeatFlux(dof_id_type element_id) const;

  /**
   * Get the nearest element ID for given element ID
   *
   * Used when a heat structure element needs to know what its nearest element is and vice versa.
   * @param elem_id Element ID either from a flow channel or a heat structure
   * @return Nearest element corresponding to a `elem_id`
   */
  const dof_id_type & getNearestElem(dof_id_type elem_id) const
  {
    return _fch_alignment.getNearestElemID(elem_id);
  }

protected:
  virtual ADReal computeQpHeatFlux() = 0;

  /**
   * Parallel gather of all local contributions into one global map
   */
  void allGatherMap(std::map<dof_id_type, std::vector<ADReal>> & data);

  /// Flow channel alignment object
  const FlowChannelAlignment & _fch_alignment;
  /// Quadrature point index
  unsigned int _qp;
  /// How qpoint indices are mapped from slave side to master side per element
  std::map<dof_id_type, std::vector<unsigned int>> _elem_qp_map;
  /// Cached heated perimeter
  std::map<dof_id_type, std::vector<ADReal>> _heated_perimeter;
  /// Cached heat flux
  std::map<dof_id_type, std::vector<ADReal>> _heat_flux;
  /// Coupled heated perimeter variable
  const ADVariableValue & _P_hf;

public:
  static InputParameters validParams();
};
