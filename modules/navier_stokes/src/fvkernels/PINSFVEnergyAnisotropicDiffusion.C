//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "PINSFVEnergyAnisotropicDiffusion.h"
#include "INSFVEnergyVariable.h"
#include "NS.h"

registerMooseObject("NavierStokesApp", PINSFVEnergyAnisotropicDiffusion);

InputParameters
PINSFVEnergyAnisotropicDiffusion::validParams()
{
  auto params = FVFluxKernel::validParams();
  params.addClassDescription(
      "Anisotropic diffusion term in the porous media incompressible Navier-Stokes "
      "equations : -div(kappa grad(T))");
  params.addRequiredParam<MooseFunctorName>(NS::kappa, "Vector of effective thermal conductivity");
  params.addRequiredParam<MooseFunctorName>(NS::porosity, "Porosity");
  params.addParam<bool>(
      "effective_diffusivity",
      true,
      "Whether the diffusivity should be multiplied by porosity, or whether the provided "
      "diffusivity is an effective diffusivity taking porosity effects into account");

  params.set<unsigned short>("ghost_layers") = 2;
  return params;
}

PINSFVEnergyAnisotropicDiffusion::PINSFVEnergyAnisotropicDiffusion(const InputParameters & params)
  : FVFluxKernel(params),
    _k(getFunctor<ADRealVectorValue>(NS::kappa)),
    _eps(getFunctor<ADReal>(NS::porosity)),
    _porosity_factored_in(getParam<bool>("effective_diffusivity"))
{
#ifndef MOOSE_GLOBAL_AD_INDEXING
  mooseError("PINSFV is not supported by local AD indexing. In order to use PINSFV, please run the "
             "configure script in the root MOOSE directory with the configure option "
             "'--with-ad-indexing-type=global'");
#endif
  if (!dynamic_cast<INSFVEnergyVariable *>(&_var))
    mooseError(
        "PINSFVEnergyAnisotropicDiffusion may only be used with a fluid temperature variable, "
        "of variable type INSFVEnergyVariable.");
}

ADReal
PINSFVEnergyAnisotropicDiffusion::computeQpResidual()
{
  // Interpolate thermal conductivity times porosity on the face
  ADRealVectorValue k_eps_face;
  const auto face_elem = elemFromFace();
  const auto face_neighbor = neighborFromFace();

  if (!_porosity_factored_in)
    Moose::FV::interpolate(Moose::FV::InterpMethod::Average,
                           k_eps_face,
                           _k(face_elem) * _eps(face_elem),
                           _k(face_neighbor) * _eps(face_neighbor),
                           *_face_info,
                           true);
  else
    Moose::FV::interpolate(Moose::FV::InterpMethod::Average,
                           k_eps_face,
                           _k(face_elem),
                           _k(face_neighbor),
                           *_face_info,
                           true);

  // Compute the temperature gradient times the conductivity tensor
  ADRealVectorValue kappa_grad_T;
  const auto & grad_T = _var.adGradSln(*_face_info);
  for (std::size_t i = 0; i < LIBMESH_DIM; i++)
    kappa_grad_T(i) = k_eps_face(i) * grad_T(i);

  return -kappa_grad_T * _normal;
}
