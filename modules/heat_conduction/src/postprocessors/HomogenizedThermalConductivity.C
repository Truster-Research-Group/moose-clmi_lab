//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "HomogenizedThermalConductivity.h"
#include "SubProblem.h"
#include "MooseMesh.h"

registerMooseObject("HeatConductionApp", HomogenizedThermalConductivity);

InputParameters
HomogenizedThermalConductivity::validParams()
{
  InputParameters params = ElementAverageValue::validParams();
  params.addClassDescription(
      "Postprocessor for asymptotic expansion homogenization for thermal conductivity");
  params.addRequiredCoupledVar("temp_x", "solution in x");
  params.addCoupledVar("temp_y", "solution in y");
  params.addCoupledVar("temp_z", "solution in z");
  params.addRequiredRangeCheckedParam<unsigned int>(
      "component",
      "component < 3",
      "An integer corresponding to the direction this pp acts in (0 for x, 1 for y, 2 for z)");
  params.addParam<Real>("scale_factor", 1, "Scale factor");
  params.addParam<MaterialPropertyName>(
      "diffusion_coefficient",
      "thermal_conductivity",
      "Property name of the diffusivity (Default: thermal_conductivity)");
  return params;
}

HomogenizedThermalConductivity::HomogenizedThermalConductivity(const InputParameters & parameters)
  : ElementAverageValue(parameters),
    _grad_temp_x(coupledGradient("temp_x")),
    _grad_temp_y(_subproblem.mesh().dimension() >= 2 ? coupledGradient("temp_y") : _grad_zero),
    _grad_temp_z(_subproblem.mesh().dimension() == 3 ? coupledGradient("temp_z") : _grad_zero),
    _component(getParam<unsigned int>("component")),
    _diffusion_coefficient(getMaterialProperty<Real>("diffusion_coefficient")),
    _scale(getParam<Real>("scale_factor"))
{
}

void
HomogenizedThermalConductivity::initialize()
{
  _integral_value = 0.0;
  _volume = 0.0;
}

void
HomogenizedThermalConductivity::execute()
{
  _integral_value += computeIntegral();
  _volume += _current_elem_volume;
}

Real
HomogenizedThermalConductivity::getValue()
{
  return (_integral_value / _volume);
}

void
HomogenizedThermalConductivity::finalize()
{
  gatherSum(_integral_value);
  gatherSum(_volume);
}

void
HomogenizedThermalConductivity::threadJoin(const UserObject & y)
{
  const HomogenizedThermalConductivity & pps =
      dynamic_cast<const HomogenizedThermalConductivity &>(y);

  _integral_value += pps._integral_value;
  _volume += pps._volume;
}

Real
HomogenizedThermalConductivity::computeQpIntegral()
{
  Real value = 1.0;

  switch (_component)
  {
    case 0:
      value += _grad_temp_x[_qp](0);
      break;

    case 1:
      value += _grad_temp_y[_qp](1);
      break;

    case 2:
      value += _grad_temp_z[_qp](2);
      break;

    default:
      mooseError("Internal error.");
  }

  return _scale * _diffusion_coefficient[_qp] * value;
}
