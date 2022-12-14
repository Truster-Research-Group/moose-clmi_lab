//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "MooseVariableFV.h"
#include "TimeIntegrator.h"
#include "NonlinearSystemBase.h"
#include "DisplacedSystem.h"
#include "SystemBase.h"
#include "SubProblem.h"
#include "Assembly.h"
#include "MathFVUtils.h"
#include "FVUtils.h"
#include "FVFluxBC.h"
#include "FVDirichletBCBase.h"
#include "GreenGaussGradient.h"

#include "libmesh/numeric_vector.h"

#include <climits>
#include <typeinfo>

using namespace Moose;

registerMooseObject("MooseApp", MooseVariableFVReal);

template <typename OutputType>
InputParameters
MooseVariableFV<OutputType>::validParams()
{
  InputParameters params = MooseVariableField<OutputType>::validParams();
  params.set<bool>("fv") = true;
  params.set<MooseEnum>("family") = "MONOMIAL";
  params.set<MooseEnum>("order") = "CONSTANT";
  params.template addParam<bool>(
      "two_term_boundary_expansion",
#ifdef MOOSE_GLOBAL_AD_INDEXING
      true,
#else
      false,
#endif
      "Whether to use a two-term Taylor expansion to calculate boundary face values. "
      "If the two-term expansion is used, then the boundary face value depends on the "
      "adjoining cell center gradient, which itself depends on the boundary face value. "
      "Consequently an implicit solve is used to simultaneously solve for the adjoining cell "
      "center gradient and boundary face value(s).");
#ifdef MOOSE_GLOBAL_AD_INDEXING
  MooseEnum face_interp_method("average skewness-corrected", "average");
  params.template addParam<MooseEnum>("face_interp_method",
                                      face_interp_method,
                                      "Switch that can select between face interpoaltion methods.");
  params.template addParam<bool>(
      "cache_cell_gradients", true, "Whether to cache cell gradients or re-compute them.");
#endif
  return params;
}

template <typename OutputType>
MooseVariableFV<OutputType>::MooseVariableFV(const InputParameters & parameters)
  : MooseVariableField<OutputType>(parameters),
    _solution(this->_sys.currentSolution()),
    _phi(this->_assembly.template fePhi<OutputShape>(FEType(CONSTANT, MONOMIAL))),
    _grad_phi(this->_assembly.template feGradPhi<OutputShape>(FEType(CONSTANT, MONOMIAL))),
    _phi_face(this->_assembly.template fePhiFace<OutputShape>(FEType(CONSTANT, MONOMIAL))),
    _grad_phi_face(this->_assembly.template feGradPhiFace<OutputShape>(FEType(CONSTANT, MONOMIAL))),
    _phi_face_neighbor(
        this->_assembly.template fePhiFaceNeighbor<OutputShape>(FEType(CONSTANT, MONOMIAL))),
    _grad_phi_face_neighbor(
        this->_assembly.template feGradPhiFaceNeighbor<OutputShape>(FEType(CONSTANT, MONOMIAL))),
    _phi_neighbor(this->_assembly.template fePhiNeighbor<OutputShape>(FEType(CONSTANT, MONOMIAL))),
    _grad_phi_neighbor(
        this->_assembly.template feGradPhiNeighbor<OutputShape>(FEType(CONSTANT, MONOMIAL))),
    _two_term_boundary_expansion(this->isParamValid("two_term_boundary_expansion")
                                     ? this->template getParam<bool>("two_term_boundary_expansion")
                                     :
#ifdef MOOSE_GLOBAL_AD_INDEXING
                                     true),
#else
                                     false),
#endif
    _cache_cell_gradients(this->isParamValid("cache_cell_gradients")
                              ? this->template getParam<bool>("cache_cell_gradients")
                              : true)
{
#ifndef MOOSE_GLOBAL_AD_INDEXING
  if (_two_term_boundary_expansion)
    this->paramError(
        "two_term_boundary_expansion",
        "Two term boundary expansion only works for global AD indexing configurations.");
#endif
  _element_data = std::make_unique<MooseVariableDataFV<OutputType>>(
      *this, _sys, _tid, Moose::ElementType::Element, this->_assembly.elem());
  _neighbor_data = std::make_unique<MooseVariableDataFV<OutputType>>(
      *this, _sys, _tid, Moose::ElementType::Neighbor, this->_assembly.neighbor());

  if (this->isParamValid("face_interp_method"))
  {
    const auto & interp_method = this->template getParam<MooseEnum>("face_interp_method");
    if (interp_method == "average")
      _face_interp_method = Moose::FV::InterpMethod::Average;
    else if (interp_method == "skewness-corrected")
      _face_interp_method = Moose::FV::InterpMethod::SkewCorrectedAverage;
  }
  else
    _face_interp_method = Moose::FV::InterpMethod::Average;
}

template <typename OutputType>
const std::set<SubdomainID> &
MooseVariableFV<OutputType>::activeSubdomains() const
{
  return this->_sys.system().variable(_var_num).active_subdomains();
}

template <typename OutputType>
Moose::VarFieldType
MooseVariableFV<OutputType>::fieldType() const
{
  if (std::is_same<OutputType, Real>::value)
    return Moose::VarFieldType::VAR_FIELD_STANDARD;
  else if (std::is_same<OutputType, RealVectorValue>::value)
    return Moose::VarFieldType::VAR_FIELD_VECTOR;
  else if (std::is_same<OutputType, RealEigenVector>::value)
    return Moose::VarFieldType::VAR_FIELD_ARRAY;
  else
    mooseError("Unknown variable field type");
}

template <typename OutputType>
bool
MooseVariableFV<OutputType>::activeOnSubdomain(SubdomainID subdomain) const
{
  return this->_sys.system().variable(_var_num).active_on_subdomain(subdomain);
}

template <typename OutputType>
void
MooseVariableFV<OutputType>::clearDofIndices()
{
  _element_data->clearDofIndices();
}

template <typename OutputType>
typename MooseVariableFV<OutputType>::OutputData
MooseVariableFV<OutputType>::getElementalValue(const Elem * elem, unsigned int idx) const
{
  return _element_data->getElementalValue(elem, Moose::Current, idx);
}

template <typename OutputType>
typename MooseVariableFV<OutputType>::OutputData
MooseVariableFV<OutputType>::getElementalValueOld(const Elem * elem, unsigned int idx) const
{
  return _element_data->getElementalValue(elem, Moose::Old, idx);
}

template <typename OutputType>
typename MooseVariableFV<OutputType>::OutputData
MooseVariableFV<OutputType>::getElementalValueOlder(const Elem * elem, unsigned int idx) const
{
  return _element_data->getElementalValue(elem, Moose::Older, idx);
}

template <typename OutputType>
void
MooseVariableFV<OutputType>::insert(NumericVector<Number> & residual)
{
  _element_data->insert(residual);
}

template <typename OutputType>
void
MooseVariableFV<OutputType>::add(NumericVector<Number> & residual)
{
  _element_data->add(residual);
}

template <typename OutputType>
const typename MooseVariableFV<OutputType>::DoFValue &
MooseVariableFV<OutputType>::dofValues() const
{
  return _element_data->dofValues();
}

template <typename OutputType>
const typename MooseVariableFV<OutputType>::DoFValue &
MooseVariableFV<OutputType>::dofValuesOld() const
{
  return _element_data->dofValuesOld();
}

template <typename OutputType>
const typename MooseVariableFV<OutputType>::DoFValue &
MooseVariableFV<OutputType>::dofValuesOlder() const
{
  return _element_data->dofValuesOlder();
}

template <typename OutputType>
const typename MooseVariableFV<OutputType>::DoFValue &
MooseVariableFV<OutputType>::dofValuesPreviousNL() const
{
  return _element_data->dofValuesPreviousNL();
}

template <typename OutputType>
const typename MooseVariableFV<OutputType>::DoFValue &
MooseVariableFV<OutputType>::dofValuesNeighbor() const
{
  return _neighbor_data->dofValues();
}

template <typename OutputType>
const typename MooseVariableFV<OutputType>::DoFValue &
MooseVariableFV<OutputType>::dofValuesOldNeighbor() const
{
  return _neighbor_data->dofValuesOld();
}

template <typename OutputType>
const typename MooseVariableFV<OutputType>::DoFValue &
MooseVariableFV<OutputType>::dofValuesOlderNeighbor() const
{
  return _neighbor_data->dofValuesOlder();
}

template <typename OutputType>
const typename MooseVariableFV<OutputType>::DoFValue &
MooseVariableFV<OutputType>::dofValuesPreviousNLNeighbor() const
{
  return _neighbor_data->dofValuesPreviousNL();
}

template <typename OutputType>
const typename MooseVariableFV<OutputType>::DoFValue &
MooseVariableFV<OutputType>::dofValuesDot() const
{
  return _element_data->dofValuesDot();
}

template <typename OutputType>
const typename MooseVariableFV<OutputType>::DoFValue &
MooseVariableFV<OutputType>::dofValuesDotDot() const
{
  return _element_data->dofValuesDotDot();
}

template <typename OutputType>
const typename MooseVariableFV<OutputType>::DoFValue &
MooseVariableFV<OutputType>::dofValuesDotOld() const
{
  return _element_data->dofValuesDotOld();
}

template <typename OutputType>
const typename MooseVariableFV<OutputType>::DoFValue &
MooseVariableFV<OutputType>::dofValuesDotDotOld() const
{
  return _element_data->dofValuesDotDotOld();
}

template <typename OutputType>
const typename MooseVariableFV<OutputType>::DoFValue &
MooseVariableFV<OutputType>::dofValuesDotNeighbor() const
{
  return _neighbor_data->dofValuesDot();
}

template <typename OutputType>
const typename MooseVariableFV<OutputType>::DoFValue &
MooseVariableFV<OutputType>::dofValuesDotDotNeighbor() const
{
  return _neighbor_data->dofValuesDotDot();
}

template <typename OutputType>
const typename MooseVariableFV<OutputType>::DoFValue &
MooseVariableFV<OutputType>::dofValuesDotOldNeighbor() const
{
  return _neighbor_data->dofValuesDotOld();
}

template <typename OutputType>
const typename MooseVariableFV<OutputType>::DoFValue &
MooseVariableFV<OutputType>::dofValuesDotDotOldNeighbor() const
{
  return _neighbor_data->dofValuesDotDotOld();
}

template <typename OutputType>
const MooseArray<Number> &
MooseVariableFV<OutputType>::dofValuesDuDotDu() const
{
  return _element_data->dofValuesDuDotDu();
}

template <typename OutputType>
const MooseArray<Number> &
MooseVariableFV<OutputType>::dofValuesDuDotDotDu() const
{
  return _element_data->dofValuesDuDotDotDu();
}

template <typename OutputType>
const MooseArray<Number> &
MooseVariableFV<OutputType>::dofValuesDuDotDuNeighbor() const
{
  return _neighbor_data->dofValuesDuDotDu();
}

template <typename OutputType>
const MooseArray<Number> &
MooseVariableFV<OutputType>::dofValuesDuDotDotDuNeighbor() const
{
  return _neighbor_data->dofValuesDuDotDotDu();
}

template <typename OutputType>
void
MooseVariableFV<OutputType>::prepareIC()
{
  _element_data->prepareIC();
}

template <typename OutputType>
void
MooseVariableFV<OutputType>::computeElemValues()
{
  _element_data->setGeometry(Moose::Volume);
  _element_data->computeValues();
}

template <typename OutputType>
void
MooseVariableFV<OutputType>::computeElemValuesFace()
{
  _element_data->setGeometry(Moose::Face);
  _element_data->computeValues();
}

template <typename OutputType>
void
MooseVariableFV<OutputType>::computeNeighborValuesFace()
{
  _neighbor_data->setGeometry(Moose::Face);
  _neighbor_data->computeValues();
}

template <typename OutputType>
void
MooseVariableFV<OutputType>::computeNeighborValues()
{
  _neighbor_data->setGeometry(Moose::Volume);
  _neighbor_data->computeValues();
}

template <typename OutputType>
void
MooseVariableFV<OutputType>::computeFaceValues(const FaceInfo & fi)
{
  _element_data->setGeometry(Moose::Face);
  _neighbor_data->setGeometry(Moose::Face);

  auto facetype = fi.faceType(_var_name);
  if (facetype == FaceInfo::VarFaceNeighbors::NEITHER)
    return;
  else if (facetype == FaceInfo::VarFaceNeighbors::BOTH)
  {
    _element_data->computeValuesFace(fi);
    _neighbor_data->computeValuesFace(fi);
  }
  else if (facetype == FaceInfo::VarFaceNeighbors::ELEM)
  {
    _element_data->computeValuesFace(fi);
    _neighbor_data->computeGhostValuesFace(fi, *_element_data);
  }
  else if (facetype == FaceInfo::VarFaceNeighbors::NEIGHBOR)
  {
    _neighbor_data->computeValuesFace(fi);
    _element_data->computeGhostValuesFace(fi, *_neighbor_data);
  }
  else
    mooseError("robert wrote broken MooseVariableFV code");
}

template <typename OutputType>
OutputType
MooseVariableFV<OutputType>::getValue(const Elem * elem) const
{
  std::vector<dof_id_type> dof_indices;
  this->_dof_map.dof_indices(elem, dof_indices, _var_num);
  mooseAssert(dof_indices.size() == 1, "Wrong size for dof indices");
  OutputType value = (*this->_sys.currentSolution())(dof_indices[0]);
  return value;
}

template <typename OutputType>
typename OutputTools<OutputType>::OutputGradient
MooseVariableFV<OutputType>::getGradient(const Elem * /*elem*/) const
{
  return {};
}

template <typename OutputType>
void
MooseVariableFV<OutputType>::setNodalValue(const OutputType & /*value*/, unsigned int /*idx*/)
{
  mooseError("FV variables do not support setNodalValue");
}

template <typename OutputType>
void
MooseVariableFV<OutputType>::setDofValue(const OutputData & value, unsigned int index)
{
  _element_data->setDofValue(value, index);
}

template <typename OutputType>
void
MooseVariableFV<OutputType>::setDofValues(const DenseVector<OutputData> & values)
{
  _element_data->setDofValues(values);
}

template <typename OutputType>
bool
MooseVariableFV<OutputType>::isArray() const
{
  return std::is_same<OutputType, RealEigenVector>::value;
}

template <typename OutputType>
bool
MooseVariableFV<OutputType>::isVector() const
{
  return std::is_same<OutputType, RealVectorValue>::value;
}

template <typename OutputType>
std::pair<bool, const FVDirichletBCBase *>
MooseVariableFV<OutputType>::getDirichletBC(const FaceInfo & fi) const
{
  std::vector<FVDirichletBCBase *> bcs;

  this->_subproblem.getMooseApp()
      .theWarehouse()
      .query()
      .template condition<AttribSystem>("FVDirichletBC")
      .template condition<AttribThread>(_tid)
      .template condition<AttribBoundaries>(fi.boundaryIDs())
      .template condition<AttribVar>(_var_num)
      .template condition<AttribSysNum>(this->_sys.number())
      .queryInto(bcs);
  mooseAssert(bcs.size() <= 1, "cannot have multiple dirichlet BCs on the same boundary");

  bool has_dirichlet_bc = bcs.size() > 0;

  if (has_dirichlet_bc)
  {
    mooseAssert(bcs[0], "The FVDirichletBC is null!");

    return std::make_pair(true, bcs[0]);
  }
  else
    return std::make_pair(false, nullptr);
}

template <typename OutputType>
std::pair<bool, std::vector<const FVFluxBC *>>
MooseVariableFV<OutputType>::getFluxBCs(const FaceInfo & fi) const
{
  std::vector<const FVFluxBC *> bcs;

  this->_subproblem.getMooseApp()
      .theWarehouse()
      .query()
      .template condition<AttribSystem>("FVFluxBC")
      .template condition<AttribThread>(_tid)
      .template condition<AttribBoundaries>(fi.boundaryIDs())
      .template condition<AttribVar>(_var_num)
      .template condition<AttribSysNum>(this->_sys.number())
      .queryInto(bcs);

  bool has_flux_bc = bcs.size() > 0;

  if (has_flux_bc)
    return std::make_pair(true, bcs);
  else
    return std::make_pair(false, std::vector<const FVFluxBC *>());
}

template <typename OutputType>
ADReal
MooseVariableFV<OutputType>::getElemValue(const Elem * const elem) const
{
#ifndef MOOSE_GLOBAL_AD_INDEXING
  mooseError("MooseVariableFV::getElemValue only supported for global AD indexing");
#endif

  std::vector<dof_id_type> dof_indices;
  this->_dof_map.dof_indices(elem, dof_indices, _var_num);

  mooseAssert(
      dof_indices.size() == 1,
      "There should only be one dof-index for a constant monomial variable on any given element");

  dof_id_type index = dof_indices[0];

  ADReal value = (*_solution)(index);

  if (ADReal::do_derivatives && _var_kind == Moose::VAR_NONLINEAR)
    Moose::derivInsert(value.derivatives(), index, 1.);

  return value;
}

template <typename OutputType>
ADReal
MooseVariableFV<OutputType>::getNeighborValue(const Elem * const neighbor,
                                              const FaceInfo & fi,
                                              const ADReal & elem_value) const
{
#ifndef MOOSE_GLOBAL_AD_INDEXING
  mooseError("MooseVariableFV::getNeighborValue only supported for global AD indexing");
#endif

  if (neighbor && this->hasBlocks(neighbor->subdomain_id()))
    return getElemValue(neighbor);
  else
    // If we don't have a neighbor, then we're along a boundary
    // Linear interpolation: face_value = (elem_value + neighbor_value) / 2. Note that weights of
    // 1/2 are perfectly appropriate here because we can arbitrarily put our ghost cell centroid
    // anywhere and by convention we locate it such that a line drawn between the ghost cell
    // centroid and the element centroid is perfectly bisected by the face centroid
    return 2. * getBoundaryFaceValue(fi) - elem_value;
}

template <typename OutputType>
bool
MooseVariableFV<OutputType>::isInternalFace(const FaceInfo & fi) const
{
  const bool is_internal_face = fi.faceType(this->name()) == FaceInfo::VarFaceNeighbors::BOTH;
  mooseAssert(is_internal_face == (this->hasBlocks(fi.elem().subdomain_id()) && fi.neighborPtr() &&
                                   this->hasBlocks(fi.neighborPtr()->subdomain_id())),
              "Sanity checking whether we are indeed an internal face");
  return is_internal_face;
}

template <typename OutputType>
ADReal
MooseVariableFV<OutputType>::getInternalFaceValue(const FaceInfo & fi,
                                                  const bool correct_skewness) const
{
#ifndef MOOSE_GLOBAL_AD_INDEXING
  mooseError("MooseVariableFV::getInternalFaceValue only supported for global AD indexing");
#endif

  mooseAssert(isInternalFace(fi), "This function shall only be called on internal faces.");

  return Moose::FV::linearInterpolation(*this, Moose::FV::makeCDFace(fi, correct_skewness));
}

template <typename OutputType>
bool
MooseVariableFV<OutputType>::isDirichletBoundaryFace(const FaceInfo & fi) const
{
  const auto & pr = getDirichletBC(fi);

  // First member of this pair indicates whether we have a DirichletBC
  return pr.first;
}

template <typename OutputType>
ADReal
MooseVariableFV<OutputType>::getDirichletBoundaryFaceValue(const FaceInfo & fi) const
{
#ifndef MOOSE_GLOBAL_AD_INDEXING
  mooseError(
      "MooseVariableFV::getDirichletBoundaryFaceValue only supported for global AD indexing");
#endif

  mooseAssert(isDirichletBoundaryFace(fi),
              "This function should only be called on Dirichlet boundary faces.");

  const auto & diri_pr = getDirichletBC(fi);

  mooseAssert(diri_pr.first,
              "This functor should only be called if we are on a Dirichlet boundary face.");

  const FVDirichletBCBase & bc = *diri_pr.second;

  return ADReal(bc.boundaryValue(fi));
}

template <typename OutputType>
std::pair<bool, const Elem *>
MooseVariableFV<OutputType>::isExtrapolatedBoundaryFace(const FaceInfo & fi) const
{
  const bool extrapolated = !isDirichletBoundaryFace(fi) && !isInternalFace(fi);

  if (!fi.neighborPtr())
    return std::make_pair(extrapolated, &fi.elem());

  const bool defined_on_elem = this->hasBlocks(fi.elem().subdomain_id());
#ifndef NDEBUG
  const bool defined_on_neighbor = this->hasBlocks(fi.neighbor().subdomain_id());

  mooseAssert(defined_on_elem || defined_on_neighbor,
              "This shouldn't be called if we aren't defined on either side.");
#endif
  const Elem * const ret_elem = defined_on_elem ? &fi.elem() : fi.neighborPtr();
  return std::make_pair(extrapolated, ret_elem);
}

template <typename OutputType>
ADReal
MooseVariableFV<OutputType>::getExtrapolatedBoundaryFaceValue(const FaceInfo & fi) const
{
#ifndef MOOSE_GLOBAL_AD_INDEXING
  mooseError(
      "MooseVariableFV::getExtrapolatedBoundaryFaceValue only supported for global AD indexing");
#endif

  mooseAssert(isExtrapolatedBoundaryFace(fi).first,
              "This function should only be called on extrapolated boundary faces");

  ADReal boundary_value;
  const auto & tup = Moose::FV::determineElemOneAndTwo(fi, *this);
  const Elem * const elem = std::get<0>(tup);
  if (_two_term_boundary_expansion)
  {
    const Point vector_to_face = std::get<2>(tup) ? (fi.faceCentroid() - fi.elemCentroid())
                                                  : (fi.faceCentroid() - fi.neighborCentroid());
    boundary_value = adGradSln(elem) * vector_to_face + getElemValue(elem);
  }
  else
    boundary_value = getElemValue(elem);

  return boundary_value;
}

template <typename OutputType>
ADReal
MooseVariableFV<OutputType>::getBoundaryFaceValue(const FaceInfo & fi) const
{
#ifndef MOOSE_GLOBAL_AD_INDEXING
  mooseError("MooseVariableFV::getBoundaryFaceValue only supported for global AD indexing");
#endif

  mooseAssert(!isInternalFace(fi), "A boundary face value has been requested on an internal face.");

  if (isDirichletBoundaryFace(fi))
    return getDirichletBoundaryFaceValue(fi);
  else if (isExtrapolatedBoundaryFace(fi).first)
    return getExtrapolatedBoundaryFaceValue(fi);

  mooseError("Unknown boundary face type!");
}

template <typename OutputType>
const VectorValue<ADReal> &
MooseVariableFV<OutputType>::adGradSln(const Elem * const elem, const bool correct_skewness) const
{
#ifndef MOOSE_GLOBAL_AD_INDEXING
  mooseError("MooseVariableFV::adGradSln only supported for global AD indexing");
#endif

  // We ensure that no caching takes place when we compute skewness-corrected
  // quantities.
  if (_cache_cell_gradients && !correct_skewness)
  {
    auto it = _elem_to_grad.find(elem);

    if (it != _elem_to_grad.end())
      return it->second;
  }

  auto grad = FV::greenGaussGradient(
      ElemArg({elem, correct_skewness}), *this, _two_term_boundary_expansion, this->_mesh);

  if (_cache_cell_gradients && !correct_skewness)
  {
    auto pr = _elem_to_grad.emplace(elem, std::move(grad));
    mooseAssert(pr.second, "Insertion should have just happened.");
    return pr.first->second;
  }
  else
  {
    _temp_cell_gradient = std::move(grad);
    return _temp_cell_gradient;
  }
}

template <typename OutputType>
VectorValue<ADReal>
MooseVariableFV<OutputType>::uncorrectedAdGradSln(const FaceInfo & fi,
                                                  const bool correct_skewness) const
{
#ifndef MOOSE_GLOBAL_AD_INDEXING
  mooseError("MooseVariableFV::uncorrectedAdGradSln only supported for global AD indexing");
#endif

  auto tup = Moose::FV::determineElemOneAndTwo(fi, *this);
  const Elem * const elem_one = std::get<0>(tup);
  const Elem * const elem_two = std::get<1>(tup);
  const bool elem_one_is_fi_elem = std::get<2>(tup);

  const VectorValue<ADReal> elem_one_grad = adGradSln(elem_one, correct_skewness);

  // If we have a neighbor then we interpolate between the two to the face. If we do not, then we
  // apply a zero Hessian assumption and use the element centroid gradient as the uncorrected face
  // gradient
  if (elem_two && this->hasBlocks(elem_two->subdomain_id()))
  {
    const VectorValue<ADReal> & elem_two_grad = adGradSln(elem_two, correct_skewness);

    // Uncorrected gradient value
    return Moose::FV::linearInterpolation(elem_one_grad, elem_two_grad, fi, elem_one_is_fi_elem);
  }
  else
    return elem_one_grad;
}

template <typename OutputType>
VectorValue<ADReal>
MooseVariableFV<OutputType>::adGradSln(const FaceInfo & fi, const bool correct_skewness) const
{
#ifndef MOOSE_GLOBAL_AD_INDEXING
  mooseError("MooseVariableFV::adGradSln only supported for global AD indexing");
#endif

  // Use a pointer to choose the right reference
  auto face_grad = uncorrectedAdGradSln(fi, correct_skewness);

  auto tup = Moose::FV::determineElemOneAndTwo(fi, *this);
  const Elem * const elem_one = std::get<0>(tup);
  const Elem * const elem_two = std::get<1>(tup);
  const bool elem_is_elem_one = std::get<2>(tup);
  mooseAssert(elem_is_elem_one ? elem_one == &fi.elem() && elem_two == fi.neighborPtr()
                               : elem_one == fi.neighborPtr() && elem_two == &fi.elem(),
              "The determineElemOneAndTwo utility got the elem_is_elem_one value wrong.");

  const ADReal elem_one_value = getElemValue(elem_one);
  const ADReal elem_two_value = getNeighborValue(elem_two, fi, elem_one_value);
  const ADReal & elem_value = elem_is_elem_one ? elem_one_value : elem_two_value;
  const ADReal & neighbor_value = elem_is_elem_one ? elem_two_value : elem_one_value;

  // perform the correction. Note that direction is important here because we have a minus sign.
  // Neighbor has to be neighbor, and elem has to be elem. Hence all the elem_is_elem_one logic
  // above
  face_grad += ((neighbor_value - elem_value) / fi.dCNMag() - face_grad * fi.eCN()) * fi.eCN();

  return face_grad;
}

template <typename OutputType>
void
MooseVariableFV<OutputType>::residualSetup()
{
  clearCaches();
}

template <typename OutputType>
void
MooseVariableFV<OutputType>::jacobianSetup()
{
  clearCaches();
}

template <typename OutputType>
void
MooseVariableFV<OutputType>::clearCaches()
{
  _elem_to_grad.clear();
}

template <typename OutputType>
unsigned int
MooseVariableFV<OutputType>::oldestSolutionStateRequested() const
{
  unsigned int state = 0;
  state = std::max(state, _element_data->oldestSolutionStateRequested());
  state = std::max(state, _neighbor_data->oldestSolutionStateRequested());
  return state;
}

template <typename OutputType>
void
MooseVariableFV<OutputType>::clearAllDofIndices()
{
  _element_data->clearDofIndices();
  _neighbor_data->clearDofIndices();
}

template <typename OutputType>
typename MooseVariableFV<OutputType>::ValueType
MooseVariableFV<OutputType>::evaluate(const FaceArg & face,
                                      const unsigned int libmesh_dbg_var(state)) const
{
  mooseAssert(state == 0, "Only current time state supported.");
  const FaceInfo * const fi = face.fi;
  mooseAssert(fi, "The face information must be non-null");
  if (isExtrapolatedBoundaryFace(*fi).first)
    return getExtrapolatedBoundaryFaceValue(*fi);
  else if (isInternalFace(*fi))
    return Moose::FV::interpolate(*this, face);
  else
  {
    mooseAssert(isDirichletBoundaryFace(*fi), "We've run out of face types");
    return getDirichletBoundaryFaceValue(*fi);
  }
}

template <typename OutputType>
typename MooseVariableFV<OutputType>::ValueType
MooseVariableFV<OutputType>::evaluate(const SingleSidedFaceArg & face,
                                      const unsigned int libmesh_dbg_var(state)) const
{
  mooseAssert(state == 0, "Only current time state supported.");
  const FaceInfo * const fi = face.fi;
  mooseAssert(fi, "The face information must be non-null");
  if (isExtrapolatedBoundaryFace(*fi).first)
    return getExtrapolatedBoundaryFaceValue(*fi);
  else if (isInternalFace(*fi))
    return getInternalFaceValue(face);
  else
  {
    mooseAssert(isDirichletBoundaryFace(*fi), "We've run out of face types");
    return getDirichletBoundaryFaceValue(*fi);
  }
}

template <typename OutputType>
typename MooseVariableFV<OutputType>::ValueType
MooseVariableFV<OutputType>::evaluate(const ElemFromFaceArg & elem_from_face, unsigned int) const
{
  const Elem * const requested_elem = elem_from_face.elem;
  mooseAssert(requested_elem != remote_elem,
              "If the requested element is remote then I think we've messed up our ghosting");

  if (requested_elem && this->hasBlocks(requested_elem->subdomain_id()))
    return getElemValue(requested_elem);
  else
  {
    const FaceInfo * const fi = elem_from_face.fi;
    mooseAssert(fi, "We need a FaceInfo");
    mooseAssert((requested_elem == &fi->elem()) || (requested_elem == fi->neighborPtr()),
                "The requested element should match something from the FaceInfo");
    const Elem * const elem_across =
        (requested_elem == &fi->elem()) ? fi->neighborPtr() : &fi->elem();
    return getNeighborValue(requested_elem, *fi, getElemValue(elem_across));
  }
}

template <typename OutputType>
typename MooseVariableFV<OutputType>::GradientType
MooseVariableFV<OutputType>::evaluateGradient(const ElemFromFaceArg & elem_from_face,
                                              unsigned int) const
{
  const Elem * const requested_elem = elem_from_face.elem;
  mooseAssert(requested_elem != remote_elem,
              "If the requested element is remote then I think we've messed up our ghosting");

  if (requested_elem && this->hasBlocks(requested_elem->subdomain_id()))
    return adGradSln(requested_elem, elem_from_face.correct_skewness);
  else
    mooseError("We do not currently support ghosting of gradients");
}

template <typename OutputType>
typename MooseVariableFV<OutputType>::DotType
MooseVariableFV<OutputType>::evaluateDot(const ElemArg &, unsigned int) const
{
  mooseError("evaluateDot not implemented for this class of finite volume variables");
}

template <>
ADReal
MooseVariableFV<Real>::evaluateDot(const ElemArg & elem_arg,
                                   const unsigned int libmesh_dbg_var(state)) const
{
  const Elem * const elem = elem_arg.elem;
  mooseAssert(state == 0,
              "We dot not currently support any time derivative evaluations other than for the "
              "current time-step");
  mooseAssert(_time_integrator && _time_integrator->dt(),
              "A time derivative is being requested but we do not have a time integrator so we'll "
              "have no idea how to compute it");

  std::vector<dof_id_type> dof_indices;
  this->_dof_map.dof_indices(elem, dof_indices, _var_num);

  mooseAssert(
      dof_indices.size() == 1,
      "There should only be one dof-index for a constant monomial variable on any given element");

  const dof_id_type dof_index = dof_indices[0];

  if (_var_kind == Moose::VAR_NONLINEAR)
  {
    ADReal dot = (*_solution)(dof_index);
    if (ADReal::do_derivatives)
      Moose::derivInsert(dot.derivatives(), dof_index, 1.);
    _time_integrator->computeADTimeDerivatives(dot, dof_index, _ad_real_dummy);
    return dot;
  }
  else
    return (*_sys.solutionUDot())(dof_index);
}

template <typename OutputType>
void
MooseVariableFV<OutputType>::prepareAux()
{
  _element_data->prepareAux();
  _neighbor_data->prepareAux();
}

template class MooseVariableFV<Real>;
// TODO: implement vector fv variable support. This will require some template
// specializations for various member functions in this and the FV variable
// classes. And then you will need to uncomment out the line below:
// template class MooseVariableFV<RealVectorValue>;
